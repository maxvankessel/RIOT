/*
 * Copyright (C) 2018 Max van Kessel <maxvankessel@betronic.nl>
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

#include <string.h>
#include <assert.h>
#include <errno.h>

#include "periph/uart.h"
#include "fmt.h"
#include "log.h"

#include "net/netdev.h"
#include "net/netdev/ppp.h"
#include "byteorder.h"

#include "gsm/call.h"

/**
 * @ingroup     drivers_gsm
 * @{
 *
 * @file
 * @brief       gsm call implementation.
 *
 * @author      Max van Kessel <maxvankessel@betronic.nl>
 *
 * @}
 */
#define LOG_HEADER  "gsm_call: "

int gsm_call_dial(gsm_t *dev, const char * number, bool is_voice_call)
{
    int result = -EINVAL;

    if(dev && number) {
        char buf[GSM_AT_LINEBUFFER_SIZE];
        char *pos = buf;

        pos += fmt_str(pos, "ATD");
        pos += fmt_str(pos, number);

        if(is_voice_call) {
            pos += fmt_str(pos, ";");
        }
        *pos = '\0';

        rmutex_lock(&dev->mutex);

        result = at_send_cmd_get_resp(&dev->at_dev, buf, buf, sizeof(buf), GSM_SERIAL_TIMEOUT_US * 5);

        rmutex_unlock(&dev->mutex);

        if (result > 0) {
            if (strcmp(buf, "CONNECT") == 0) {
                result = 0;

                if(!is_voice_call){
                    at_drain(&dev->at_dev);

                    /* switch to data mode requires another buffer */
                    dev->state = GSM_PPP;
                }
            }
            else {
                LOG_INFO(LOG_HEADER"unexpected response: %s\n", buf);
                result = -1;
            }
        }
    }

    return result;
}

int __attribute__((weak)) gsm_call_switch_to_command_mode(gsm_t *dev)
{
    int err = -EINVAL;

    if(dev) {
        rmutex_lock(&dev->mutex);

        err = at_send_cmd_wait_ok(&dev->at_dev, "+++", GSM_SERIAL_TIMEOUT_US);

        rmutex_unlock(&dev->mutex);
    }

    return err;
}

int __attribute__((weak)) gsm_call_switch_to_data_mode(gsm_t *dev)
{
    int err = -EINVAL;

    if(dev) {
        char buf[GSM_AT_LINEBUFFER_SIZE];

        rmutex_lock(&dev->mutex);

        err = at_send_cmd_get_resp(&dev->at_dev, "ATO0", buf, GSM_AT_LINEBUFFER_SIZE, GSM_SERIAL_TIMEOUT_US);

        rmutex_unlock(&dev->mutex);

        if(err > 0) {
            if(strncmp(buf, "CONNECT", err) == 0) {
                err = 0;
            }
            else {
                err = -1;
            }
        }
    }

    return err;
}

#ifdef GNRC_PPP

static int _get(netdev_t *netdev, netopt_t opt, void *value, size_t max_len)
{
    int res = -ENOTSUP;
    if (netdev == NULL) {
        return -ENODEV;
    }

    switch (opt) {
        case NETOPT_IS_WIRED:
            res = 0;
            break;
        case NETOPT_DEVICE_TYPE:
            assert(max_len == sizeof(uint16_t));
            *((uint16_t *)value) = NETDEV_TYPE_PPPOS;
            res = sizeof(uint16_t);
            break;
        default:
            res = -ENOTSUP;
    }

    if (res == -ENOTSUP) {
        res = netdev_ppp_get((netdev_ppp_t *)netdev, opt, value, max_len);
    }

    return res;
}



static int _set(netdev_t *netdev, netopt_t opt, const void *value,
        size_t value_len)
{
    int res = -ENOTSUP;

    network_uint32_t *nu32 = (network_uint32_t *) value;

    if (netdev == NULL) {
        return -ENODEV;
    }

    switch (opt) {
        case NETOPT_PPP_ACCM_RX:
            dev->accm.rx = byteorder_ntohl(*nu32);
            res = sizeof(network_uint32_t);
            break;
        case NETOPT_PPP_ACCM_TX:
            dev->accm.tx = byteorder_ntohl(*nu32);
            res = sizeof(network_uint32_t);
            break
        default:
            res = -ENOTSUP;
    }

    if (res == -ENOTSUP) {
        res = netdev_ppp_set((netdev_ppp_t *)netdev, opt, value, value_len);
    }

    return res;
}


static const netdev_driver_t gsm_driver = {
    .send = _send,
    .recv = _recv,
    .init = NULL,
    .isr = _isr,
    .get = _get,
    .set = _set,
};

#endif
