/*
 * Copyright (C)
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     tests
 * @{
 *
 * @file
 * @brief       Test application for gnrc_tapnet network device driver
 *
 * @author
 *
 * @}
 */

#include <stdio.h>

#include "shell.h"
#include "shell_commands.h"
#include "net/gnrc.h"
#include "net/gnrc/pktdump.h"
#include "net/gnrc/netif/raw.h"

#include "pppos.h"

#define PPPOS_STACKSIZE       (THREAD_STACKSIZE_DEFAULT)
#ifndef PPPOS_PRIO
#define PPPOS_PRIO            (GNRC_NETIF_PRIO)
#endif

static pppos_t _pppos;
static char _pppos_stack[PPPOS_STACKSIZE];

static const pppos_params_t _pppos_params = {
    .uart = PPPOS_UART,
    .baudrate = PPPOS_BAUDRATE,
};

/**
 * @brief   Maybe you are a golfer?!
 */
int main(void)
{
    pppos_setup(&_pppos, &_pppos_params);
    gnrc_netif_raw_create(_pppos_stack, PPPOS_STACKSIZE, PPPOS_PRIO, "pppos",
            (netdev_t *)&_pppos);

    gnrc_netreg_entry_t dump = GNRC_NETREG_ENTRY_INIT_PID(GNRC_NETREG_DEMUX_CTX_ALL,
                                                          gnrc_pktdump_pid);

    puts("PPPOS test");

    /* register pktdump */
    if (dump.target.pid <= KERNEL_PID_UNDEF) {
        puts("Error starting pktdump thread");
        return -1;
    }

    gnrc_netreg_register(GNRC_NETTYPE_UNDEF, &dump);

    /* start the shell */
    puts("Initialization OK, starting shell now");

    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(NULL, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}
