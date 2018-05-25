/**
 * @ingroup     drivers_gsm_pppos
 * @{
 *
 * @file
 * @brief       GSM Point to Point Protocol Netdev
 *
 * @author      Max van Kessel
 *
 * @}
 */
#include <errno.h>
#include <string.h>
#include <stdbool.h>

#include "net/hdlc.h"
#include "net/netdev.h"

#include "isrpipe.h"

#include "log.h"

#define MODULE  "gsm_ppp: "

#define ACCM_DEFAULT            (0xFFFFFFFFUL)

#define ESCAPE_P(accm, c)       ((c > 0x1f) ? c : ((accm) & (1 << c)) ? 0 : c)
#define NEED_ESCAPE(accm, c)    ((c < 0x20) && ((accm) & (1 << c)))

static void _rx_cb(void *arg, uint8_t byte)
{
    gsm_t *dev = arg;
    netdev_t *netdev = (netdev_t *)&dev->netdev;

    if(ESCAPE_P(dev->accm.rx, byte)){
        tsrb_add_one(&dev->inbuf, byte);

        if((byte == HDLC_FLAG_SEQUENCE) && (tsrb_avail(&dev->inbuf) > 0)) {
            /* new character */
            if (netdev->event_callback != NULL) {
                netdev->event_callback(netdev, NETDEV_EVENT_ISR);
            }
        }
    }
    else {
        /* dropping accm chars */
        LOG_INFO(MODULE"dropping accm char %x\n", byte);
    }
}

static int _send(netdev_t *netdev, const iolist_t *iolist)
{
    gsm_t *dev = (gsm_t *)netdev;

    int bytes = 0;

    LOG_INFO(MODULE"sending iolist\n");
    LOG_DEBUG(MODULE);

    for(const iolist_t *iol = iolist; iol; iol = iol->iol_next) {
        uint8_t * data = iol->iol_base;

        for (unsigned j = 0; j < iol->iol_len; j++, data++) {
            if (NEED_ESCAPE(dev->accm.tx, *data)) {
                uint8_t esc = HDLC_CONTROL_ESCAPE;
                LOG_DEBUG("%02x ", esc);
                uart_write(dev->config.uart, &esc, 1);
                *data ^= HDLC_SIX_CMPL;

                bytes++;
            }

            uart_write(dev->config.uart, data, 1);
            LOG_DEBUG("%02x ", *data);

            bytes++;
        }
    }

    LOG_DEBUG("(%u) [OUT]\n", bytes);

    return bytes;
}

static int _recv(netdev_t *netdev, void *buf, size_t len, void *info)
{
    int res = 0;
    gsm_t *dev = (gsm_t *)netdev;

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
    LOG_DEBUG(MODULE"handling ISR event\n");
    if (dev->event_callback != NULL) {
        LOG_INFO(MODULE"event handler set, issuing RX_COMPLETE event\n");
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
    gsm_t *dev = (gsm_t *)netdev;
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

        default:
            res = -ENOTSUP;
    }

    if (res == -ENOTSUP) {
        res = netdev_ppp_set((netdev_ppp_t *)netdev, opt, value, value_len);
    }

    return res;
}

static const netdev_driver_t gsm_ppp_driver = {
    .send = _send,
    .recv = _recv,
    .init = NULL,
    .isr = _isr,
    .get = _get,
    .set = _set,
};

void gsm_ppp_setup(gsm_t *dev)
{
    netdev_t *netdev = (netdev_t *)&dev->netdev;

    dev->accm.rx = ACCM_DEFAULT;
    dev->accm.tx = ACCM_DEFAULT;

    netdev->driver = &gsm_ppp_driver;
}

void gsm_ppp_handle(gsm_t *dev)
{
    isrpipe_try_read()
}

/** @} */
