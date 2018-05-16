/*
 * Copyright (C) 2016 José Ignacio Alamos <jialamos@uc.cl>
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
 * @author  José Ignacio Alamos <jialamos@uc.cl>
 * @author  Max van Kessel
 */

//#ifdef MODULE_NETDEV_PPP
#include <errno.h>
#include <string.h>

#include "net/gnrc.h"
#include "net/gnrc/netif/internal.h"
#include "net/gnrc/netif/ppp.h"
#include "net/netdev/ppp.h"
#include "net/ppp/hdr.h"

#include "byteorder.h"

#define ENABLE_DEBUG    (0)
#include "debug.h"

#if ENABLE_DEBUG
/* For PRIu16 etc. */
#include <inttypes.h>
#endif

#define MODULE  "gnrc_ppp: "

static void _gnrc_ppp_event_cb(netdev_t *dev, netdev_event_t event);
static const char * _nettype_to_string(gnrc_nettype_t type);

static void _gnrc_ppp_init(gnrc_netif_t *netif);
static int _send(gnrc_netif_t *netif, gnrc_pktsnip_t *pkt);
static gnrc_pktsnip_t *_recv(gnrc_netif_t *netif);

static const gnrc_netif_ops_t ppp_ops = {
    .init = _gnrc_ppp_init,
    .send = _send,
    .recv = _recv,
    .get = gnrc_netif_get_from_netdev,
    .set = gnrc_netif_set_from_netdev,
};

static void _gnrc_ppp_init(gnrc_netif_t *netif)
{
    netdev_ppp_t *ppp = (netdev_ppp_t *)netif->dev;

    netif->dev->event_callback = _gnrc_ppp_event_cb;

    (void)ppp;

//    dcp_init(ppp);
//    lcp_init(ppp);
//    ipcp_init(ppp);
//    ppp_ipv4_init(ppp);
//    pap_init(ppp);
//
//    trigger_fsm_event((gnrc_ppp_fsm_t *)&ppp->lcp, PPP_E_OPEN, NULL);
//    trigger_fsm_event((gnrc_ppp_fsm_t *)&ppp->ipcp, PPP_E_OPEN, NULL);
}

static int _send(gnrc_netif_t *netif, gnrc_pktsnip_t *pkt)
{
    int res = 0;
    ppp_hdr_t hdr;
    netdev_t *dev = netif->dev;
    gnrc_pktsnip_t *payload;

    if (pkt == NULL) {
        DEBUG(MODULE"pkt was NULL\n");
        res = -EINVAL;
        goto out;
    }

    hdr.protocol = byteorder_htons(gnrc_nettype_to_ppp_protnum(pkt->type));

    payload = pkt->next;

//    /* check packet length against maximum receive unit (MRU)*/
//    if(gnrc_pkt_len(hdr) > lcp_get_mru((netdev_ppp_t *)dev)) {
//        DEBUG(MODULE"sending exceeds peer MRU. Dropping packet.\n");
//        res = -EBADMSG;
//        goto safe_out;
//    }

    iolist_t iolist = {
        .iol_next = (iolist_t *)payload,
        .iol_base = &hdr,
        .iol_len = sizeof(ppp_hdr_t)
    };

    res = dev->driver->send(dev, &iolist);

//safe_out:
    gnrc_pktbuf_release(pkt);

out:
    return res;
}

static gnrc_pktsnip_t *_recv(gnrc_netif_t *netif)
{
    int nbytes;
    gnrc_pktsnip_t *pkt;

    netdev_t *dev = netif->dev;

    /* get number of bytes to read */
    nbytes = dev->driver->recv(dev, NULL, 0, NULL);

    if(nbytes > 0) {
        /* prepare packet */
        pkt = gnrc_pktbuf_add(NULL, NULL, nbytes, GNRC_NETTYPE_UNDEF);

        if(!pkt) {
            DEBUG(MODULE"cannot allocate pktsnip.\n");

            /* drop the packet */
            dev->driver->recv(dev, NULL, nbytes, NULL);

            goto out;
        }

        int nread = dev->driver->recv(dev, pkt->data, nbytes, NULL);
        if(nread <= 0) {
            DEBUG(MODULE"read error.\n");
            goto safe_out;
        }

        if(nread < nbytes) {
            /* we've got less than the expected packet size,
             * so free the unused space.*/
            DEBUG(MODULE"reallocating.\n");
            gnrc_pktbuf_realloc_data(pkt, nread);
        }

        /* mark header */
        gnrc_pktsnip_t *ppp_hdr = gnrc_pktbuf_mark(pkt, sizeof(ppp_hdr_t), GNRC_NETTYPE_PPP);
        if (!ppp_hdr) {
          DEBUG(MODULE"no space left in packet buffer\n");
          goto safe_out;
        }

        ppp_hdr_t *hdr = (ppp_hdr_t *)ppp_hdr->data;

        /* set payload type from ppptype */
        pkt->type = gnrc_nettype_from_ppp_protnum(byteorder_ntohs(hdr->protocol));

        if(pkt->type != GNRC_NETTYPE_UNDEF) {
            DEBUG(MODULE"packet received - protocol: x%04X\n", byteorder_ntohs(hdr->protocol));
        }
        else {
            DEBUG(MODULE"packet received - protocol: %s\n", _nettype_to_string(pkt->type));
        }

        pkt = ppp_hdr;
    }

out:
    return pkt;

safe_out:
    gnrc_pktbuf_release(pkt);
    return NULL;
}

gnrc_netif_t *gnrc_netif_ppp_create(char *stack, int stacksize, char priority,
        char *name, netdev_t *dev)
{
    return gnrc_netif_create(stack, stacksize, priority, name, dev, &ppp_ops);
}

static int _gnrc_ppp_dispatch(gnrc_netif_t *netif, gnrc_pktsnip_t *pkt)
{
//    netdev_ppp_t *dev = (netdev_ppp_t *)netif->dev;
//    ppp_hdr_t *hdr = (ppp_hdr_t *)pkt->data;

    (void) netif;

    if(pkt->type != GNRC_NETTYPE_UNDEF) {

        if(pkt->type == GNRC_NETTYPE_NCP) {

        }
        else if (pkt->type == GNRC_NETTYPE_LCP) {

        }
    }


    return 0;
}

static void _gnrc_ppp_event_cb(netdev_t *dev, netdev_event_t event)
{
    gnrc_netif_t *netif = (gnrc_netif_t *)dev->context;

    if(event == NETDEV_EVENT_ISR) {
        msg_t msg;

        msg.type = NETDEV_MSG_TYPE_EVENT;
        msg.content.ptr = (void *)netif;

        if(msg_send(&msg, netif->pid) <= 0) {
            DEBUG("gnrc_netdev: possibly lost interrupt.\n");
        }
    } else {
        DEBUG("gnrc_netdev: event triggered -> %u\n", event);
        switch(event) {

        case NETDEV_EVENT_RX_COMPLETE: {
            gnrc_pktsnip_t *pkt = netif->ops->recv(netif);
            if(pkt != NULL) {
                int err = _gnrc_ppp_dispatch(netif, pkt);
                if(err < 0) {
                   DEBUG(MODULE"failed to dispatch packet %d.\n", err);
                }
                gnrc_pktbuf_release(pkt);
            }
            break;
        }
        case NETDEV_EVENT_TX_COMPLETE:
            break;

        default:
            DEBUG(MODULE"unhandled event %u.\n", event);
        }
    }
}

static const char * _nettype_to_string(gnrc_nettype_t type)
{
    switch (type) {
        case GNRC_NETTYPE_LCP:
            return "lcp";
        case GNRC_NETTYPE_IPCP:
            return "ipcp";
        case GNRC_NETTYPE_IPV4:
            return "ipv4";
        case GNRC_NETTYPE_PAP:
            return "pap";
        default:
            return "undef";
    }
}

//#else   /* MODULE_NETDEV_PPP */
//typedef int dont_be_pedantic;
//#endif  /* MODULE_NETDEV_PPP */
