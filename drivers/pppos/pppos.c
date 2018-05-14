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

#include "log.h"
#include "xtimer.h"

#include "net/ppp/hdr.h"
#include "net/hdlc/fcs.h"

#include "pppos.h"

#define ENABLE_DEBUG    (0)
#include "debug.h"

#define MODULE  "pppos: "

#define HDLC_ALLSTATIONS        (0xFF)    /* All-Stations broadcast address */
#define HDLC_UI                 (0x03)    /* Unnumbered Information */
#define HDLC_FLAG_SEQUENCE      (0x7E)
#define HDLC_CONTROL_ESCAPE     (0x7D)
#define HDLC_SIX_CMPL           (0x20)

#define PPP_INIT_FCS16          (0xFFFF)
#define PPP_GOOD_FCS16          (0xF0B8)

#define ACCM_DEFAULT            (0xFFFFFFFFUL)

#define ESCAPE_P(accm, c)       ((c > 0x1f) ? c : ((accm) & (1 << c)) ? 0 : c)
#define NEED_ESCAPE(accm, c)    (((c < 0x1f) && ((accm) & (1 << c))) ? c : 0)

static int _tsrb_peek_one(const tsrb_t * rb)
{
    int res = -1;

    if(!tsrb_empty(rb)) {
        res = (unsigned char)rb->buf[rb->reads & (rb->size - 1)];
    }

    return res;
}

static void _pppos_drop_input(pppos_t *dev)
{
    int c = tsrb_get_one(&dev->inbuf);

    while((c > 0) && (c != HDLC_FLAG_SEQUENCE)) {
        c = tsrb_get_one(&dev->inbuf);
    }
}

static void _pppos_rx_cb(void *arg, uint8_t byte)
{
    pppos_t *dev = arg;
    uint8_t esc = ESCAPE_P(dev->accm.rx, byte);

    if(esc){
        /* special character */
        if((byte == HDLC_CONTROL_ESCAPE) || (byte == HDLC_FLAG_SEQUENCE)) {
            if(byte == HDLC_CONTROL_ESCAPE) {
                dev->esc = 1;
            } else if(byte == HDLC_FLAG_SEQUENCE) {
                if(dev->state <= PPP_RX_ADDRESS) {
                    /* ignore */
                }
                else if(dev->state < PPP_RX_DATA) {
                    /* drop, incomplete*/
                    _pppos_drop_input(dev);
                }
                else if(dev->fcs != PPP_GOOD_FCS16) {
                    /* drop, bad fcs */
                    _pppos_drop_input(dev);

                    if (dev->netdev.event_callback != NULL) {
                        dev->netdev.event_callback((netdev_t *)dev, NETDEV_EVENT_CRC_ERROR);
                    }
                }
                else {
                    /* complete package */
                    if(dev->netdev.event_callback != NULL) {
                        dev->netdev.event_callback((netdev_t *)dev, NETDEV_EVENT_ISR);
                    }
                }
                /* new packet preparation */
                dev->fcs = PPP_INIT_FCS16;
                dev->state = PPP_RX_ADDRESS;
                dev->esc = 0;

                /* add sequence flag */
                tsrb_add_one(&dev->inbuf, byte);

                if (dev->netdev.event_callback != NULL) {
                   dev->netdev.event_callback((netdev_t *)dev, NETDEV_EVENT_RX_STARTED);
               }
            }
        }
        else {
            if(dev->esc) {
                dev->esc = 0;
                byte ^= HDLC_SIX_CMPL;
            }

            switch(dev->state) {
                case PPP_RX_IDLE:
                    /* All Stations address */
                    if(byte != HDLC_ALLSTATIONS){
                        break;
                    }
                    /* fall through */
                case PPP_RX_STARTED:
                    dev->fcs = PPP_INIT_FCS16;
                    /* fall through */
                case PPP_RX_ADDRESS:
                    /* All Stations address */
                    if(byte == HDLC_ALLSTATIONS){
                        dev->state = PPP_RX_CONTROL;
                        /* not necessary to store */
                        goto compute;
                    }
                    /* fall through */
                case PPP_RX_CONTROL:
                    if(byte == HDLC_UI){
                        dev->state = PPP_RX_PROTOCOL;
                        dev->prot = 0;
                        /* not necessary to store */
                        goto compute;
                    }

                case PPP_RX_PROTOCOL:
                    if(dev->prot == 0) {
                        /* end of protocol field */
                        if(byte & 1) {
                            dev->prot = byte;
                            dev->state = PPP_RX_DATA;
                        }
                        else {
                            dev->prot = (uint16_t)byte << 8;
                        }
                    }
                    else {
                        dev->prot |= byte;
                        dev->state = PPP_RX_DATA;
                    }
                    break;

                case PPP_RX_DATA:
                    break;
            }

            tsrb_add_one(&dev->inbuf, byte);
compute:
            dev->fcs = fcs16_bit(dev->fcs, byte);
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

    /* initialize buffers */
    tsrb_init(&dev->inbuf, (char*)dev->rxmem, sizeof(dev->rxmem));
    if(uart_init(dev->config.uart, dev->config.baudrate, _pppos_rx_cb, dev) != UART_OK) {
        LOG_ERROR(MODULE"error initializing UART %i with baudrate %" PRIu32 "\n",
                  dev->config.uart, dev->config.baudrate);
        return -ENODEV;
    }
    return 0;
}

static inline void _pppos_write_byte(pppos_t *dev, uint8_t byte, uint8_t accm, uint16_t *fcs)
{
    uint8_t c = byte;

    if(fcs) {
        *fcs = fcs16_bit(*fcs, byte);
    }

    if((accm) && (NEED_ESCAPE(dev->accm.tx, c))) {
        uart_write(dev->config.uart, (uint8_t *)HDLC_CONTROL_ESCAPE, 1);
        c ^= HDLC_SIX_CMPL;
    }

    uart_write(dev->config.uart, &c, 1);
}

static int _send(netdev_t *netdev, const iolist_t *iolist)
{
    pppos_t *dev = (pppos_t *)netdev;
    uint16_t fcs;

    int bytes = 0;

    DEBUG(MODULE"sending iolist\n");

    fcs = PPP_INIT_FCS16;

    if((xtimer_now_usec() - dev->last_xmit) >= PPPOS_MAX_IDLE_TIME_MS) {
        _pppos_write_byte(dev, HDLC_FLAG_SEQUENCE, 0, NULL);
        _pppos_write_byte(dev, HDLC_ALLSTATIONS, 1, &fcs);
        _pppos_write_byte(dev, HDLC_UI, 1, &fcs);
    }

    for(const iolist_t *iol = iolist; iol; iol = iol->iol_next) {
        uint8_t * data = iol->iol_base;

        for(unsigned j = 0; j < iol->iol_len; j++, data++) {
            _pppos_write_byte(dev, *data, 1, &fcs);
            bytes++;
        }
    }

    fcs ^= 0xffff;
    _pppos_write_byte(dev, (uint8_t) fcs & 0x00ff, 1, NULL);
    _pppos_write_byte(dev, (uint8_t) (fcs >> 8) & 0x00ff, 1, NULL);

    _pppos_write_byte(dev, HDLC_FLAG_SEQUENCE, 0, NULL);

    dev->last_xmit = xtimer_now_usec();

    return bytes;
}

static int _recv(netdev_t *netdev, void *buf, size_t len, void *info)
{
    int res = 0;
    pppos_t *dev = (pppos_t *)netdev;

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
}

static int _set(netdev_t *netdev, netopt_t opt, const void *value,
        size_t value_len)
{
    pppos_t *dev = (pppos_t *)netdev;
    (void)value_len;

    network_uint32_t *nu32 = (network_uint32_t *) value;

    switch (opt) {
        case NETOPT_PPP_ACCM_RX:
            dev->accm.rx = byteorder_ntohl(*nu32);
            break;
        case NETOPT_PPP_ACCM_TX:
            dev->accm.tx = byteorder_ntohl(*nu32);
            break;
        default:
            return -ENOTSUP;
    }
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

    dev->accm.rx = ACCM_DEFAULT;
    dev->accm.tx = ACCM_DEFAULT;

    dev->last_xmit = 0;

    dev->netdev.driver = &pppos_driver;
}

/** @} */
