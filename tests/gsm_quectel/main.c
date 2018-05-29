
#define AT_EOL            "\r\n"

#include <string.h>
#include <stdlib.h>

#include "shell.h"
#include "xtimer.h"
#include "board.h"

#include "quectel.h"
#include "gsm/gprs.h"
#include "gsm/call.h"
#include "gsm/ppp.h"

#include "net/gnrc.h"
#include "net/gnrc/netif/ppp.h"
#include "net/gnrc/pktdump.h"
#include "net/hdlc.h"
#include "net/netdev/layer.h"

#ifndef UART_MODEM
#define UART_MODEM      MODEM_UART
#endif

#ifndef MODEM_RI_PIN
#define MODEM_RI_PIN      GPIO_UNDEF
#endif

#ifndef MODEM_RST_PIN_
#define MODEM_RST_PIN_    GPIO_UNDEF
#endif

#ifndef MODEM_PWR_ON_PIN
#define MODEM_PWR_ON_PIN  GPIO_UNDEF
#endif

#ifndef MODEM_STATUS_PIN
#define MODEM_STATUS_PIN  GPIO_UNDEF
#endif

#ifndef MODEM_DTR_PIN
#define MODEM_DTR_PIN     GPIO_UNDEF
#endif

#ifndef MODEM_DCD_PIN
#define MODEM_DCD_PIN     GPIO_UNDEF
#endif

#define MAX_CMD_LEN 128

#define PPP_STACKSIZE       (THREAD_STACKSIZE_DEFAULT)
#ifndef PPP_PRIO
#define PPP_PRIO            (GNRC_NETIF_PRIO)
#endif

static const quectel_params_t params = {
    .base = {
        .uart            = UART_MODEM,
        .baudrate        = MODEM_BAUDRATE,
        .ri_pin          = MODEM_RI_PIN,
    },
    .power_pin          = MODEM_PWR_ON_PIN,
    .invert_power_pin   = true,
    .status_pin         = MODEM_STATUS_PIN,
    .invert_status_pin  = true,
    .reset_pin          = MODEM_RST_PIN_,
    .invert_reset_pin   = true,
    .dtr_pin            = MODEM_DTR_PIN,
    .dcd_pin            = MODEM_DCD_PIN,
};

static quectel_t _modem;

static char _ppp_stack[PPP_STACKSIZE];

static hdlc_t _hdlc;

static gnrc_netif_t *_iface;

int _at_send_handler(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: %s <command>\n", argv[0]);
        return 1;
    }

    uint8_t resp[MAX_CMD_LEN + 1];
    resp[MAX_CMD_LEN] = '\0';

    gsm_cmd((gsm_t *)&_modem, argv[1], resp, sizeof(resp), 20);
    puts((char *)resp);

    return 0;
}

int _modem_status_handler(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    gsm_print_status((gsm_t *)&_modem);
    return 0;
}

int _modem_init_pdp_handler(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if(argc < 2) {
        printf("Usage: %s <context> <apn> [user [pass]]\n", argv[0]);
        return 1;
    }

    gsm_gprs_setup_pdp_context((gsm_t *)&_modem, (uint8_t)atoi(argv[1]),
            GSM_CONTEXT_IP, argv[2], (argv[3]) ? argv[3] : NULL,
            (argv[4]) ? argv[4] : NULL);
    return 0;
}

int _modem_cpin_status_handler(int argc, char **argv)
{
    int result;
    (void)argc;
    (void)argv;

    result = gsm_check_pin((gsm_t *)&_modem);
    if(result == 0) {
        printf("Simcard unlocked.\n");
    }
    else if (result == 1) {
        printf("Simcard present, needs unlocking.\n");
    }
    else {
        printf("Failed to check simcard status.\n");
    }

    return 0;
}

int _modem_cpin_handler(int argc, char **argv)
{
    int result;

    if (argc < 2) {
        printf("Usage: %s <pin> [puk]\n", argv[0]);
        return 1;
    }

    result = gsm_set_puk((gsm_t *)&_modem, argv[2], argv[1]);
    if(result == 0) {
        printf("Simcard unlocked\n");
    }
    else {
        printf("Error %d", result);
    }

    return 0;
}

int _modem_power_handler(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: %s <on/off>\n", argv[0]);
        return 1;
    }

    if(strncmp(argv[1], "1", 1) == 0) {
        int result = gsm_power_on((gsm_t *)&_modem);

        if(result == 0) {
            printf("Device powered on\n");
        }
        else {
            printf("Error %d", result);
        }
    }
    else {
        gsm_power_off((gsm_t *)&_modem);
    }

    return 0;
}

int _modem_radio_handler(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: %s <on/off>\n", argv[0]);
        return 1;
    }

    if(strncmp(argv[1], "1", 1) == 0) {
        int result = gsm_enable_radio((gsm_t *)&_modem);

        if(result == 0) {
            printf("Device radio on\n");
        }
        else {
            printf("Error %d", result);
        }
    }
    else {
        gsm_disable_radio((gsm_t *)&_modem);
    }

    return 0;
}

int _ppp_dialout_handler(int argc, char **argv)
{
    int result = -1;

    if(_iface) {
        if(argc > 1) {
            result = gnrc_netapi_set(_iface->pid, NETOPT_DIAL_UP, 0, argv[1], strlen(argv[1]));
        } else {
            result = gnrc_netapi_set(_iface->pid, NETOPT_DIAL_UP, 0, "*99#", 5);
        }
    }
    if(result >= 0) {
        printf("PPP dialout success\n");
    }
    else {
        printf("Failed to dialout PPP\n");
    }

    return 0;
}

int _modem_datamode_handler(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if(gsm_call_switch_to_data_mode((gsm_t *)&_modem) != 0) {
        printf("Failed to switch to data mode\n");
    }

    return 0;
}

int _modem_cmdmode_handler(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if(gsm_call_switch_to_command_mode((gsm_t *)&_modem) != 0) {
        printf("Failed to switch to command mode\n");
    }

    return 0;
}

int _modem_ip_handler(int argc, char **argv)
{
    char buf[IPV4_ADDR_MAX_STR_LEN];

    if(argc < 2) {
        printf("Usage: %s <context> <apn> [user [pass]]\n", argv[0]);
        return 1;
    }

    uint32_t ip = gsm_gprs_get_address((gsm_t *)&_modem, (uint8_t)atoi(argv[1]));

    printf("Address (ipv4) %s\n", ipv4_addr_to_str(buf, (ipv4_addr_t *)&ip, IPV4_ADDR_MAX_STR_LEN));

    return 0;
}

int _modem_rssi_handler(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    unsigned rssi, ber;

    gsm_get_signal((gsm_t *)&_modem, &rssi, &ber);

    printf("RSSI=%u ber=%u%%\n", rssi, ber);

    return 0;
}

static const shell_command_t commands[] = {
    {"atcmd",        "Sends an AT cmd",     _at_send_handler},
    {"modem_status", "Print Modem status",  _modem_status_handler},
    {"init_pdp",     "Init PDP context",    _modem_init_pdp_handler},
    {"simpin",       "Enter simpin",        _modem_cpin_handler},
    {"sim_status",   "Check sim status",    _modem_cpin_status_handler},
    {"power",        "Power (On/Off)",      _modem_power_handler},
    {"radio",        "Radio (On/Off)",      _modem_radio_handler},
    {"dial",         "PPP Dial out",        _ppp_dialout_handler},
    {"datamode",     "Switch to datamode",  _modem_datamode_handler },
    {"cmdmode",      "Switch to commandmode",_modem_cmdmode_handler },
    {"rssi",         "Get rssi",             _modem_rssi_handler },
    {NULL, NULL, NULL}
};

int main(void)
{
    char line_buf[SHELL_DEFAULT_BUFSIZE];

    gsm_init((gsm_t *)&_modem, (gsm_params_t *)&params, &quectel_driver);

    gsm_ppp_setup((gsm_t *)&_modem);

    hdlc_setup(&_hdlc);

    _iface = gnrc_netif_ppp_create(_ppp_stack, PPP_STACKSIZE, PPP_PRIO, "ppp",
            netdev_add_layer((netdev_t *)&_modem, (netdev_t *)&_hdlc));

    gnrc_netreg_entry_t dump = GNRC_NETREG_ENTRY_INIT_PID(
            GNRC_NETREG_DEMUX_CTX_ALL, gnrc_pktdump_pid);

    puts("PPP test");

    /* register pktdump */
    if(dump.target.pid <= KERNEL_PID_UNDEF) {
        printf("Error starting pktdump thread");
        return -1;
    }

    gnrc_netreg_register(GNRC_NETTYPE_PPP, &dump);

    /* start the shell */
    puts("Initialization OK, starting shell now");

    shell_run(commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}
