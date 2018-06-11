#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "at.h"
#include "fmt.h"
#include "board.h"
#include "shell.h"
#include "xtimer.h"

#include "quectel.h"
#include "gsm/gprs.h"
#include "gsm/call.h"
#include "gsm/ppp.h"

#include "net/gnrc.h"
#include "net/gnrc/netif/ppp.h"
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

#define MAX_CMD_LEN       (256)

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

static int _connect_id;

static xtimer_ticks32_t latency_ticks;

int _at_send_handler(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: %s <command>, [timeout]\n", argv[0]);
        return 1;
    }

    uint8_t timeout = 20;

    if (argc > 2) {
        timeout = (uint8_t)atoi(argv[2]);
    }

    uint8_t resp[MAX_CMD_LEN + 1];
    resp[MAX_CMD_LEN] = '\0';

    gsm_cmd((gsm_t *)&_modem, argv[1], resp, MAX_CMD_LEN, timeout);
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

int _modem_gprs_attach(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: %s <on(1)/off(0)>\n", argv[0]);
        return 1;
    }

    if(strncmp(argv[1], "1", 1) == 0) {
        int result = gsm_grps_attach((gsm_t *)&_modem);

        if(result == 0) {
            printf("Attached\n");
        }
        else {
            printf("Error %d", result);
        }
    }
    else {
        gsm_grps_detach((gsm_t *)&_modem);
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
        printf("Usage: %s <context>\n", argv[0]);
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

    int rssi;
    unsigned ber;

    if(gsm_get_signal((gsm_t *)&_modem, &rssi, &ber) == 0) {
        printf("RSSI= %ddBm ber=%u%%\n", rssi, ber);
    }
    else {
        puts("Failed to get signal strength");
    }

    return 0;
}

static void udp_open(int argc, char **argv)
{
    gsm_t * m = (gsm_t *)&_modem;
    int res;

    if (argc < 5) {
        printf("usage: %s %s <ctx> <domain> <port>\n", argv[0], argv[1]);
        return;
    }

    char buf[128] = { 0 };
    char *pos = buf;

    pos += fmt_str(pos, "AT+QIOPEN=");
    pos += fmt_str(pos, argv[2]);
    pos += fmt_str(pos, ",0,");
    pos += fmt_str(pos, "\"UDP\",\"");
    pos += fmt_str(pos, argv[3]);
    pos += fmt_str(pos, "\",");
    pos += fmt_str(pos, argv[4]);
    fmt_str(pos, ",0,0\0");

    pos = buf;

    res = at_send_cmd_wait_ok(&m->at_dev, buf, (uint32_t)(2 * US_PER_SEC));
    if (res < 0) {
        printf("failed to open socket\n");
    }
}

static void udp_close(int argc, char **argv)
{
    int con = _connect_id;

    if(argc > 1) {
        con = atoi(argv[1]);
    }

    if(con != -1) {
        gsm_t * m = (gsm_t *)&_modem;

        char buf[32] = {0};
        char *pos = buf;

        pos += fmt_str(pos, "AT+QICLOSE=");
        pos += fmt_s32_dec(pos, con);
        *pos = '\0';

        if(at_send_cmd_wait_ok(&m->at_dev, buf, 20 * US_PER_SEC) == 0) {
            puts("closed");

            if(con == _connect_id) {
                _connect_id = -1;
            }
        }
        else {
            puts("failed to close");
        }
    }
    else {
        puts("not opnened");
    }
}

static void udp_read(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    gsm_t * m = (gsm_t *)&_modem;
    char buf[256] = { 0 };
    char *pos = buf;

    pos += fmt_str(pos, "AT+QIRD=");
    pos += fmt_s32_dec(pos, _connect_id);
    pos += fmt_str(pos, ",");
    pos += fmt_u32_dec(pos, sizeof(buf));
    pos += fmt_str(pos, "\0");

    pos = buf;

    if(at_send_cmd_get_resp(&m->at_dev, buf, buf,
            sizeof(buf), 2 * US_PER_SEC) <= 0) {
        puts("failed to get data");
    }
    else {
        int number_of_bytes = 0;
        pos = strchr(buf, ' ');
        if (isdigit((int)*(++pos))) {
            number_of_bytes = atoi(pos);
        }
        ssize_t len = at_readline(&m->at_dev, buf, number_of_bytes + 2, false, 2 * US_PER_SEC);

        printf("read: %s (%d)\n", buf, len);
    }


}

static void udp_write(int argc, char **argv)
{
    gsm_t * m = (gsm_t *)&_modem;

    if (argc < 3) {
        printf("usage: %s %s <string>\n", argv[0], argv[1]);
        return;
    }

    char buf[256] = { 0 };
    char *pos = buf;

    pos += fmt_str(pos, "AT+QISEND=");
    pos += fmt_s32_dec(pos, _connect_id);
    pos += fmt_str(pos, ",");
    pos += fmt_s32_dec(pos, strlen(argv[2]));
    pos += fmt_str(pos, "\0");

    if (at_send_cmd_wait_prompt(&m->at_dev, buf, 20 * US_PER_SEC) < 0) {
        int err;

        at_drain(&m->at_dev);

        at_send_bytes(&m->at_dev, argv[2], strlen(argv[2]));

        if((err = at_expect_bytes(&m->at_dev, argv[2], (uint32_t)2 * US_PER_SEC)) == 0) {

            err = at_readline(&m->at_dev, buf, sizeof(buf), false, (uint32_t)2 * US_PER_SEC);
            if (err == 0) {
                /* skip possible empty line */
                err = at_readline(&m->at_dev, buf, sizeof(buf), false, (uint32_t)2 * US_PER_SEC);
            }
        }

        if(err > 0) {
            printf("response: %s\n", buf);

            if(strncmp(buf, "SEND OK", 7) == 0) {
                latency_ticks = xtimer_now();
            }
        }
        else {
            printf("failed to send, error number: %d\n", err);
        }
    }
}

int _upd_handler(int argc, char **argv)
{
    rmutex_lock(&_modem.base.mutex);

    if(strcmp(argv[1], "open") == 0) {
        udp_open(argc, argv);
    }
    else if(strcmp(argv[1], "close") == 0) {
        udp_close(argc, argv);
    }
    else if(strcmp(argv[1], "recv") == 0) {
        udp_read(argc, argv);
    }
    else if(strcmp(argv[1], "send") == 0) {
        udp_write(argc, argv);
    }
    else {
        printf("usage: %s open|close|recv|send\n", argv[0]);
    }

    rmutex_unlock(&_modem.base.mutex);

    return 0;
}

static void urc_callback(void *arg, const char *urc)
{
    (void)arg;

    if (strlen(urc) > 0) {
        char * pos = strchr(urc, ' ');
        if (pos) {
            if(strncmp(++pos, "\"recv\"", 6) == 0) {

                if(latency_ticks.ticks32 != 0) {
                    xtimer_ticks32_t t = xtimer_diff(xtimer_now(), latency_ticks);

                    uint32_t msec = xtimer_usec_from_ticks(t) / US_PER_MS;

                    printf("new data received, latency %u msec\n", (unsigned int)msec);

                    latency_ticks.ticks32 = 0;
                }
            }
        }
    }
}

static void open_callback(void *arg, const char *urc)
{
    (void)arg;
    ssize_t len = strlen(urc);

    if (len > 0) {
        int err = -EBADMSG;

        char * pos = strchr(urc, ',');
        if (pos) {
            err = atoi(++pos);
            if (err == 0) {
                pos = strchr(urc, ' ');
                if (isdigit((int)*(++pos))) {
                    _connect_id = atoi(pos);
                }
                printf("opened socket successfully\n");
            }
        }

        if(err) {
            printf("failed to open socket, %d\n", err);
        }
    }
}

static const shell_command_t commands[] = {
    {"atcmd",        "Sends an AT cmd",     _at_send_handler},
    {"modem_status", "Print Modem status",  _modem_status_handler},
    {"init_pdp",     "Init PDP context",    _modem_init_pdp_handler},
    {"simpin",       "Enter simpin",        _modem_cpin_handler},
    {"sim_status",   "Check sim status",    _modem_cpin_status_handler},
    {"power",        "Power (On/Off)",      _modem_power_handler},
    {"radio",        "Radio (On/Off)",      _modem_radio_handler},
    {"attach",       "Attach(1), Detach(0)", _modem_gprs_attach },
    {"dial",         "PPP Dial out",        _ppp_dialout_handler},
    {"datamode",     "Switch to datamode",  _modem_datamode_handler },
    {"cmdmode",      "Switch to commandmode",_modem_cmdmode_handler },
    {"rssi",         "Get rssi",             _modem_rssi_handler },
    {"addr",         "Get address",         _modem_ip_handler },
    {"udp",          "UDP handler",         _upd_handler },
    {NULL, NULL, NULL}
};

int main(void)
{
    char line_buf[SHELL_DEFAULT_BUFSIZE];

    gsm_init((gsm_t *)&_modem, (gsm_params_t *)&params, &quectel_driver);

    at_oob_t urc = {
        .urc = "+QIURC: ",
        .cb = urc_callback,
        .arg = &_modem,
    };
    gsm_register_urc_callback((gsm_t *)&_modem, &urc);

    at_oob_t open = {
        .urc = "+QIOPEN: ",
        .cb = open_callback,
        .arg = &_modem,
    };
    gsm_register_urc_callback((gsm_t *)&_modem, &open);

    gsm_ppp_setup((gsm_t *)&_modem);

    hdlc_setup(&_hdlc);

    _iface = gnrc_netif_ppp_create(_ppp_stack, PPP_STACKSIZE, PPP_PRIO, "ppp",
            netdev_add_layer((netdev_t *)&_modem, (netdev_t *)&_hdlc));

    puts("PPP test");

    /* start the shell */
    puts("Initialization OK, starting shell now");

    shell_run(commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}
