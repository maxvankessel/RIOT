/*
 * Copyright (C) 2018 Max van Kessel
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup
 * @{
 *
 * @file
 * @brief
 * 
 * @author  Max van Kessel
 * 
 * @} 
 */

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdbool.h>

#include "net/netdev.h"
#include "net/netdev/layer.h"

#include "net/hdlc/fcs.h"
#include "net/hdlc.h"

#define ENABLE_DEBUG    (0)
#include "debug.h"

#define MODULE  "hdlc: "

#define FCS16_INIT              (0xFFFF)
#define FCS16_GOOD              (0xF0B8)

static int _init(netdev_t *netdev)
{
    hdlc_t *dev = (hdlc_t *)&dev->netdev;

    dev->station_id = 0xFF;
    dev->control = 0;
    dev->last_xmit = 0;

    /* initialize buffers */
   tsrb_init(&dev->inbuf, (char*)dev->rxmem, sizeof(HDLC_BUFSIZE));

    netdev->event_callback = _event_cb;

    return netdev_init_pass(netdev);
}

static int _tsrb_peek_one(const tsrb_t * rb)
{
    int res = -1;

    if(!tsrb_empty(rb)) {
        res = (unsigned char)rb->buf[rb->reads & (rb->size - 1)];
    }

    return res;
}

static void _drop_input(hdlc_t *dev)
{
    int c = tsrb_get_one(&dev->inbuf);

    while((c > 0) && (c != HDLC_FLAG_SEQUENCE)) {
        c = tsrb_get_one(&dev->inbuf);
    }
}

static void _rx_cb(void *arg, uint8_t byte)
{
    hdlc_t *dev = arg;
    netdev_t *netdev = (netdev_t *)&dev->netdev;

    /* special character */
    if ((byte == HDLC_CONTROL_ESCAPE) || (byte == HDLC_FLAG_SEQUENCE)) {
        if (byte == HDLC_CONTROL_ESCAPE) {
            dev->esc = 1;
        }
        else if (byte == HDLC_FLAG_SEQUENCE) {
            if (dev->state <= HDLC_ADDRESS) {
                /* ignore */
            }
            else if (dev->state < HDLC_DATA) {
                /* drop, incomplete*/
                _drop_input(dev);
            }
            else if (dev->fcs != FCS16_GOOD) {
                /* drop, bad fcs */
                _drop_input(dev);
            }
            else {
                /* complete package */
                if (netdev->event_callback != NULL) {
                    netdev->event_callback((netdev_t *)dev, NETDEV_EVENT_RX_COMPLETE);
                }
            }
            /* new packet preparation */
            dev->fcs = FCS16_INIT;
            dev->state = HDLC_ADDRESS;
            dev->esc = 0;

            /* add sequence flag */
            tsrb_add_one(&dev->inbuf, byte);
        }
    }
    else {
        if (dev->esc) {
            dev->esc = 0;
            byte ^= HDLC_SIX_CMPL;
        }

        switch (dev->state) {
            case HDLC_IDLE:
                /* fall through */
            case HDLC_START:
                dev->fcs = FCS16_INIT;
                /* fall through */
            case HDLC_ADDRESS:
                dev->state = HDLC_CONTROL;
                break;
            case HDLC_CONTROL:
                dev->state = HDLC_DATA;
                break;
            case HDLC_DATA:
                break;
        }

        tsrb_add_one(&dev->inbuf, byte);

        dev->fcs = fcs16_bit(dev->fcs, byte);
    }
}

static int _recv(netdev_t *netdev, void *buf, size_t len, void *info)
{
    int res = 0;
    hdlc_t *dev = (hdlc_t *)netdev;

    (void)info;
    uint8_t *ptr = buf;

    for(; len > 0; len--) {
        int byte;

        if ((byte = _tsrb_peek_one(&dev->inbuf)) < 0) {
            /* something went wrong, return error */
            return -EIO;
        }

        /* start or restart */
        if(byte == HDLC_FLAG_SEQUENCE) {
            if(res >= 2) {
                /* complete, remove checksum */
                res -= 2;
                return res;
            }
        }
        else {
            /* drop if buf is null */
            if(buf) {
                *(ptr++) = (uint8_t)byte;
                res++;
            }
        }

        /* remove from buffer */
        (void)tsrb_get_one(&dev->inbuf);
    }

    if(len == 0) {
        /* the user was warned not to use a buffer size > `INT_MAX` ;-) */
       res = (int)tsrb_avail(&dev->inbuf);
    }

    return res;
}

static void _isr(netdev_t *netdev)
{
    hdlc_t * dev = (hdlc_t *)netdev;

    int bytes_expected = netdev_recv_pass(netdev, NULL, 0, NULL);

    for(; bytes_expected; --bytes_expected) {
        uint8_t byte;

        netdev_recv_pass(netdev, &byte, 1, NULL);

        _rx_cb(dev, byte);
    }
}

static uint8_t * _add(uint8_t *arr, uint8_t byte, bool flag, uint16_t *fcs)
{
    uint8_t c = byte;

    if (fcs) {
        *fcs = fcs16_bit(*fcs, byte);
    }

    if (!flag) {
        if ((c == 0x7E) || (c == 0x7D)) {
            *arr++ = HDLC_CONTROL_ESCAPE;
            c ^= HDLC_SIX_CMPL;
        }
    }

    *arr++ = c;

    return arr;
}

static int _send(netdev_t *netdev, const iolist_t *iolist)
{
    hdlc_t *dev = (hdlc_t *)netdev;
    uint16_t fcs = FCS16_INIT;
    uint8_t * ptr = dev->txmem;

    if((xtimer_now_usec() - dev->last_xmit) >= HDLC_MAX_IDLE_TIME_MS) {
        ptr = _add(ptr, HDLC_FLAG_SEQUENCE, true, NULL);
        ptr = _add(ptr, dev->station_id, false, &fcs);
        ptr = _add(ptr, dev->control, false, &fcs);
    }

    for(const iolist_t *iol = iolist; iol; iol = iol->iol_next) {
        uint8_t * data = iol->iol_base;

        for(unsigned j = 0; j < iol->iol_len; j++, data++) {
            ptr = _add(ptr, *data, false, &fcs);
        }
    }

    fcs ^= 0xffff;
    ptr = _add(ptr, (uint8_t) fcs & 0x00ff, false, NULL);
    ptr = _add(ptr, (uint8_t) (fcs >> 8) & 0x00ff, false, NULL);

    ptr = _add(ptr, HDLC_FLAG_SEQUENCE, true, NULL);

    dev->last_xmit = xtimer_now_usec();

    iolist_t new_iolist = {
            .iol_next = NULL,
            .iol_base = dev->txmem,
            .iol_len = ptr - dev->txmem,
    };

    return netdev_send_pass(netdev, &new_iolist);
}

static int _get(netdev_t *netdev, netopt_t opt, void *value, size_t max_len)
{
    int res = -ENODEV;
    hdlc_t *dev = (hdlc_t *)netdev;

    if (netdev == NULL) {
        switch (opt) {
            case NETOPT_HDLC_CONTROL:
                *((uint8_t *)value) = dev->control;
                res = 1;
                break;
            case NETOPT_HDLC_STATION_ID:
                *((uint8_t *)value) = dev->station_id;
                res = 1;
                break;
            default:
                res = -ENOTSUP;
        }

        if (res < 0) {
            res = netdev_get_pass(netdev, opt, value, max_len);
        }
    }

    return res;
}

static int _set(netdev_t *netdev, netopt_t opt, const void *value,
        size_t value_len)
{
    hdlc_t *dev = (hdlc_t *)netdev;
    int res = -ENODEV;

    if (dev != NULL) {
        switch (opt) {
            case NETOPT_HDLC_CONTROL:
                dev->control = *((uint8_t *)value);
                res = sizeof(dev->control);
                break;
            case NETOPT_HDLC_STATION_ID:
                dev->station_id = *((uint8_t *)value);
                res = sizeof(dev->station_id);
                break;
            default:
                res = -ENOTSUP;
        }

        if (res < 0) {
            res = netdev_set_pass(netdev, opt, value, value_len);
        }

    }
    return res;
}

static const netdev_driver_t hdlc_driver = {
    .send = _send,
    .recv = _recv,
    .init = _init,
    .isr = _isr,
    .get = _get,
    .set = _set,
};

/**
 * @brief   Function called by the device driver on device events
 *
 * @param[in] event     type of event
 */
static void _event_cb(netdev_t *dev, netdev_event_t event)
{
    netdev_event_cb_pass(dev, event);
}

void hdlc_setup(hdlc_t *dev)
{
    netdev_t *netdev = (netdev_t *)&dev->netdev;

    netdev->driver = &hdlc_driver;
}

void hdlc_hdr_print(hdlc_hdr_t *hdr)
{
    printf("   address: %" PRIu8 "\n", hdr->address);
    printf("   control: %" PRIu8 "\n", hdr->control);
}
