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

#include "log.h"
#include "pppos.h"

#include "net/hdlc/fcs.h"

#define ENABLE_DEBUG    (0)
#include "debug.h"

#define MODULE  "pppos: "

#define HDLC_FLAG_SEQUENCE      (0x7E)
#define HDLC_CONTROL_ESCAPE     (0x7D)
#define HDLC_SIX_CMPL           (0x20)

#define PPP_INIT_FCS16          (0xFFFF)
#define PPP_GOOD_FCS16          (0xF0B8)

#define ACCM_DEFAULT            (0xFFFFFFFFUL)

static inline void _pppos_rx_ready(pppos_t *dev)
{
    dev->state = PPP_RX_IDLE;

    if((dev->fcs == PPP_GOOD_FCS16) && (dev->esc == 0) && (dev->idx >= 4)){
        if(dev->netdev.event_callback != NULL) {
            dev->netdev.event_callback((netdev_t *)dev, NETDEV_EVENT_ISR);
        }

        dev->count = dev->idx;
    }

    dev->idx = 0;
    dev->esc = 0;

    dev->fcs = PPP_INIT_FCS16;
}

static void _pppos_rx_cb(void *arg, uint8_t byte)
{
    pppos_t *dev = arg;

    switch(byte) {

        case HDLC_FLAG_SEQUENCE:
            if(dev->state != PPP_RX_IDLE) {
                _pppos_rx_ready(dev);
            }
            break;
        case HDLC_CONTROL_ESCAPE:
            dev->state = PPP_RX_STARTED;
            dev->esc = HDLC_SIX_CMPL;
            break;

        default:
            dev->state = PPP_RX_STARTED;
            if(!((byte <= HDLC_SIX_CMPL) && (dev->accm.rx & (1 << byte)))){
                uint8_t c = byte ^ dev->esc;

                dev->state = PPP_RX_STARTED;

                tsrb_add_one(&dev->inbuf, c);
                dev->idx++;

                dev->fcs = fcs16_bit(dev->fcs, c);
                dev->esc = 0;
            }
    }




    if(byte == HDLC_FLAG_SEQUENCE) {
        if(dev->state == PPP_RX_IDLE){
            dev->state = PPP_RX_STARTED;

            dev->esc = HDLC_SIX_CMPL;
            dev->fcs = PPP_INIT_FCS16;

            dev->count = 0;
        }
        else {
            if((dev->fcs == PPP_GOOD_FCS16) && (dev->esc == 0) && (dev->count >= 4)){
                if(dev->netdev.event_callback != NULL) {
                    dev->netdev.event_callback((netdev_t *)dev, NETDEV_EVENT_ISR);
                }

                dev->state = PPP_RX_IDLE;
            }
        }
    }
}

static int _init(netdev_t *netdev)
{
    pppos_t *dev = (pppos_t *)netdev;

    DEBUG(MODULE"initializing device %p on UART %i with baudrate %" PRIu32 "\n",
          (void *)dev, dev->config.uart, dev->config.baudrate);

    /* initialize buffers */
    tsrb_init(&dev->inbuf, dev->rxmem, sizeof(dev->rxmem));
    if(uart_init(dev->config.uart, dev->config.baudrate, _pppos_rx_cb, dev) != UART_OK) {
        LOG_ERROR(MODULE"error initializing UART %i with baudrate %" PRIu32 "\n",
                  dev->config.uart, dev->config.baudrate);
        return -ENODEV;
    }
    return 0;
}

static inline void _write_byte(pppos_t *dev, uint8_t byte)
{
    uart_write(dev->config.uart, &byte, 1);
}

static int _send(netdev_t *netdev, const iolist_t *iolist)
{
    pppos_t *dev = (pppos_t *)netdev;
    int bytes = 0;
    uint16_t fcs = PPP_INIT_FCS16;

    DEBUG(MODULE"sending iolist\n");

    _write_byte(dev, HDLC_FLAG_SEQUENCE);

    for(const iolist_t *iol = iolist; iol; iol = iol->iol_next) {
         = iol->iol_base;

        for(unsigned j = 0; j < iol->iol_len; j++, data++) {
            /* if flag, escape character or current char in ACCM, add escape character plus XOR HDLC_SIX_CMPL*/
            if((data == HDLC_FLAG_SEQUENCE) || (data == HDLC_CONTROL_ESCAPE)
                    || dev->accm.tx & (1 << data)) {
                _write_byte(dev, (uint8_t) HDLC_FLAG_SEQUENCE);
                _write_byte(dev, (uint8_t)data ^ HDLC_SIX_CMPL);
            } else {
                _write_byte(dev, data);
            }
            fcs = fcs16_bit(fcs, data);
        }
        bytes++;
    }

    fcs ^= 0xffff;
    _write_byte(dev, (uint8_t) fcs & 0x00ff);
    _write_byte(dev, (uint8_t) (fcs >> 8) & 0x00ff);

    _write_byte(dev, HDLC_FLAG_SEQUENCE);
    return bytes;

}

static int _recv(netdev_t *netdev, void *buf, size_t len, void *info)
{
    pppos_t *dev = (pppos_t *)netdev;
    int res = 0;

    (void)info;
    if(buf == NULL) {
        if(len > 0) {
            /* remove data */
            for(; len > 0; len--) {
                int byte = tsrb_get_one(&dev->inbuf);
                if((byte == (int)HDLC_CONTROL_ESCAPE) || (byte < 0)) {
                    /* end early if end of packet or ringbuffer is reached;
                     * len might be larger than the actual packet */
                    break;
                }
            }
        } else {
            /* the user was warned not to use a buffer size > `INT_MAX` ;-) */
            res = (int)tsrb_avail(&dev->inbuf);
        }
    } else {
        int byte;
        uint8_t *ptr = buf;

        do {
            if((byte = tsrb_get_one(&dev->inbuf)) < 0) {
                /* something went wrong, return error */
                return -EIO;
            }
            switch(byte) {
            case HDLC_FLAG_SEQUENCE:
                if((dev->fcs == PPP_GOOD_FCS16)
                       && (dev->esc == 0) && (res >= 4)) {
                   /* valid data */
                    return res;
               }
                /* start sequence */
                dev->esc = HDLC_SIX_CMPL;
                dev->fcs = PPP_INIT_FCS16;

                res = 0;
                break;

            case HDLC_CONTROL_ESCAPE:
                /* escape next character */
                dev->esc = HDLC_SIX_CMPL;
                break;

            default:
                if(!((byte <= HDLC_SIX_CMPL) && (dev->accm.rx & (1 << byte)))) {
                    uint8_t c = byte ^ dev->esc;

                    *(ptr++) = c;
                    res++;

                    dev->fcs = fcs16_bit(dev->fcs, c);
                    dev->esc = 0;
                }
                break;
            }
        } while(byte != HDLC_CONTROL_ESCAPE);
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

static int _get(netdev_t *dev, netopt_t opt, void *value, size_t max_len)
{
    (void)netdev;
    (void)value;
    (void)max_len;
    switch (opt) {
        case NETOPT_IS_WIRED:
            return 1;
        case NETOPT_DEVICE_TYPE:
            assert(max_len == sizeof(uint16_t));
            *((uint16_t *)value) = NETDEV_TYPE_PPPOS;
            return sizeof(uint16_t);
        default:
            return -ENOTSUP;
}

static int _set(netdev_t *dev, netopt_t opt, const void *value, size_t value_len)
{
    (void)netdev;
    (void)opt;
    (void)value;
    (void)value_len;
    return -ENOTSUP;
}

static const netdev_driver_t pppos_driver = {
    .send = _send,
    .recv = _recv,
    .init = _init,
    .isr = _isr,
    .get = _get,
    .set = _set,
};

void pppos_setup(pppos_t *dev, const pppos_params_t *params)
{
    /* set device descriptor fields */
    memcpy(&dev->config, params, sizeof(dev->config));

    dev->fcs = PPP_INIT_FCS16;
    dev->accm.rx = ACCM_DEFAULT;
    dev->accm.tx = ACCM_DEFAULT;

    dev->netdev.driver = &pppos_driver;
}

/** @} */
