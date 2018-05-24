/*
 * Copyright (C) 2018 Max van Kessel
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 *
 * @file
 * @author  Max van Kessel
 */

#include <errno.h>
#include <string.h>
#include <stdbool.h>

#include "net/hdlc.h"
#include "net/netdev.h"

#include "at.h"
#include "log.h"
#include "fmt.h"
#include "xtimer.h"

#include "pppos.h"

#define ENABLE_DEBUG    (1)
#include "debug.h"

#define MODULE  "pppos: "

#define ACCM_DEFAULT            (0xFFFFFFFFUL)

#define ESCAPE_P(accm, c)       ((c > 0x1f) ? c : ((accm) & (1 << c)) ? 0 : c)
#define NEED_ESCAPE(accm, c)    (((c < 0x1f) && ((accm) & (1 << c))) ? c : 0)

int _dialup(pppos_t *dev, const char * number);

static void _pppos_rx_cb(void *arg, uint8_t byte)
{
    pppos_t *dev = arg;
    netdev_t *netdev = (netdev_t *)&dev->netdev;
    uint8_t esc = ESCAPE_P(dev->accm.rx, byte);

    if(esc){

        tsrb_add_one(&dev->inbuf, byte);

        /* new character */
        if (netdev->event_callback != NULL) {
            netdev->event_callback(netdev, NETDEV_EVENT_ISR);
        }
    }
    else {
        /* dropping accm chars */
        DEBUG(MODULE"dropping accm char %x\n", byte);
    }
}

static int _init(netdev_t *netdev)
{
    pppos_t *dev = (pppos_t *)netdev;

    DEBUG(MODULE"initializing device %p on UART %i with baudrate %" PRIu32 "\n",
          (void *)dev, dev->config.uart, dev->config.baudrate);

    /* initialize at parser */
    if(at_dev_init(&dev->at, dev->config.uart, dev->config.baudrate,
            (char*)dev->rxmem, PPPOS_BUFSIZE) < 0) {
        LOG_ERROR(
                MODULE"error initializing AT parser on uart %i with baudrate %" PRIu32 "\n",
                dev->config.uart, dev->config.baudrate);
        return -ENODEV;
    }

    if(at_send_cmd_wait_ok(&dev->at, "AT", (1 * US_PER_SEC)) != 0) {
        LOG_ERROR(MODULE" no modem response on \"AT\"\n");
        return -EIO;
    }

    return 0;
}

static int _send(netdev_t *netdev, const iolist_t *iolist)
{
    pppos_t *dev = (pppos_t *)netdev;

    int bytes = 0;

    DEBUG(MODULE"sending iolist\n");

    for(const iolist_t *iol = iolist; iol; iol = iol->iol_next) {
        uint8_t * data = iol->iol_base;

        for (unsigned j = 0; j < iol->iol_len; j++, data++) {
            if (NEED_ESCAPE(dev->accm.tx, *data)) {
                uint8_t esc = HDLC_CONTROL_ESCAPE;
                uart_write(dev->config.uart, &esc, 1);
                *data ^= HDLC_SIX_CMPL;
            }

            uart_write(dev->config.uart, data, 1);

            bytes++;
        }
    }

    return bytes;
}

static int _recv(netdev_t *netdev, void *buf, size_t len, void *info)
{
    int res = 0;
    pppos_t *dev = (pppos_t *)netdev;

    (void)info;

    if (buf == NULL) {
        if (len > 0) {
            /* remove data */
            for (; len > 0; len--) {
                if (tsrb_get_one(&dev->inbuf) < 0) {
                    /* end early if end of ringbuffer is reached;
                     * len might be larger than the actual packet */
                    break;
                }
            }
        }
        else {
            /* the user was warned not to use a buffer size > `INT_MAX` ;-) */
            res = (int)tsrb_avail(&dev->inbuf);
        }
    }
    else {
        res = tsrb_get(&dev->inbuf, buf, len);
    }

    return res;
}

static void _isr(netdev_t *dev)
{
    DEBUG(MODULE"handling ISR event\n");
    if (dev->event_callback != NULL) {
        DEBUG(MODULE"event handler set, issuing RX_COMPLETE event\n");
        dev->event_callback(dev, NETDEV_EVENT_RX_COMPLETE);
    }
}

static int _get(netdev_t *netdev, netopt_t opt, void *value, size_t max_len)
{
    int res = -ENOTSUP;
    pppos_t *dev = (pppos_t *)netdev;

    if (netdev == NULL) {
        return -ENODEV;
    }

    switch (opt) {
        case NETOPT_IS_WIRED:
            res = 1;
            break;
        case NETOPT_DEVICE_TYPE:
            assert(max_len == sizeof(uint16_t));
            *((uint16_t *)value) = NETDEV_TYPE_PPPOS;
            res = sizeof(uint16_t);
            break;
        case NETOPT_APN_NAME:
            res = (max_len > PPPOS_APN_SIZE) ? max_len : PPPOS_APN_SIZE;
            strncpy(value, dev->apn, res);
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
    pppos_t *dev = (pppos_t *)netdev;
    int res = -ENOTSUP;

    network_uint32_t *nu32 = (network_uint32_t *) value;

    if (dev == NULL) {
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
            break;
        case NETOPT_APN_NAME:
            res = (value_len < PPPOS_APN_SIZE) ? value_len : PPPOS_APN_SIZE;
            strncpy(dev->apn, value, res);
            break;
        case NETOPT_DIAL_UP:
            if(value) {
                res = _dialup(dev, value);
            }
            else {
                if (netdev->event_callback != NULL) {
                    netdev->event_callback(netdev, NETDEV_EVENT_LAYER_DOWN);
                }

                uart_close(dev->config.uart);
                /* initialize at parser */
                if(at_dev_init(&dev->at, dev->config.uart, dev->config.baudrate,
                        (char*)dev->rxmem, PPPOS_BUFSIZE) < 0) {
                    LOG_ERROR(
                            MODULE"error initializing AT parser on uart %i with baudrate %" PRIu32 "\n",
                            dev->config.uart, dev->config.baudrate);
                    return -ENODEV;
                }
            }
        break;

        default:
            res = -ENOTSUP;
    }

    if (res == -ENOTSUP) {
        res = netdev_ppp_set((netdev_ppp_t *)netdev, opt, value, value_len);
    }

    return res;
}

static const netdev_driver_t pppos_driver = {
    .send = _send,
    .recv = _recv,
    .init = _init,
    .isr = _isr,
    .get = _get,
    .set = _set,
};

int _dialup(pppos_t *dev, const char * number)
{
    int err;
    char buf[128] = { 0 };
    char *pos = buf;

    /* TODO: make dynamic */
    uint8_t ctx = 1;
    const char * ctx_type = "IP";

    pos += fmt_str(pos, "AT+CGDCONT=");
    pos += fmt_u32_dec(pos, ctx);
    pos += fmt_str(pos, ",\"");
    pos += fmt_str(pos, ctx_type);
    pos += fmt_str(pos, "\",\"");
    pos += fmt_str(pos, dev->apn);
    pos += fmt_str(pos, "\"\0");

    /* AT+CGDCONT=<ctx>,"<type>","<apn>" */
    err = at_send_cmd_wait_ok(&dev->at, buf, (5 * US_PER_SEC));
    if(err < 0) {
        LOG_ERROR(MODULE" no modem response\n");
        return -EIO;
    }

    pos = buf;
    pos += fmt_str(pos, "ATD");
    pos += fmt_str(pos, number);
    *pos = '\0';

    err = at_send_cmd_get_resp(&dev->at, buf, buf, 128, (5 * US_PER_SEC));
    if(err < 0) {
         LOG_ERROR(MODULE" no modem response\n");
         return -EIO;
     }
    else if (err > 0) {
        if(strncmp(buf, "CONNECT", 7) != 0) {
            LOG_ERROR(MODULE"unexpected response: %s\n", buf);
            return -EIO;
        }
    }
    else {
        return -EIO;
    }

    at_drain(&dev->at);

    uart_close(dev->config.uart);

    tsrb_init(&dev->inbuf, (char *)dev->rxmem, PPPOS_BUFSIZE);

    uart_init(dev->config.uart, dev->config.baudrate, _pppos_rx_cb, dev);

    if (((netdev_t*)(dev))->event_callback != NULL) {
        ((netdev_t*)(dev))->event_callback((netdev_t*)dev, NETDEV_EVENT_LAYER_UP);
    }

    return 0;
}

void pppos_setup(pppos_t *dev, const pppos_params_t *params)
{
    netdev_t *netdev = (netdev_t *)&dev->netdev;

    /* set device descriptor fields */
    memcpy(&dev->config, params, sizeof(dev->config));

    dev->accm.rx = ACCM_DEFAULT;
    dev->accm.tx = ACCM_DEFAULT;

    netdev->driver = &pppos_driver;

}

/** @} */
