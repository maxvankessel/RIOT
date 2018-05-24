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
#include "net/hdlc/hdr.h"

#include "net/gnrc/ppp/ppp.h"

#include "byteorder.h"

#define ENABLE_DEBUG    (1)
#include "debug.h"

#if ENABLE_DEBUG
/* For PRIu16 etc. */
#include <inttypes.h>
#endif

#define MODULE  "gnrc_netif_ppp: "

void _dispatch_ppp_msg(gnrc_netif_t *netif, msg_t *msg);
static void _gnrc_ppp_event_cb(netdev_t *dev, netdev_event_t event);

static void _gnrc_ppp_init(gnrc_netif_t *netif);
static int _send(gnrc_netif_t *netif, gnrc_pktsnip_t *pkt);
static gnrc_pktsnip_t *_recv(gnrc_netif_t *netif);

static inline netdev_t * get_lowest_netdev(netdev_t * n)
{
#ifdef MODULE_NETDEV_LAYER
    while(n->lower != NULL) {
        n = n->lower;
    }
#endif
    return n;
}

static inline gnrc_netif_t * get_netif(netdev_t *n)
{
    while(n->context != NULL) {
        n = n->context;
    }
    return (gnrc_netif_t *)n;
}

static const gnrc_netif_ops_t ppp_ops = {
    .init = _gnrc_ppp_init,
    .send = _send,
    .recv = _recv,
    .get = gnrc_netif_get_from_netdev,
    .set = gnrc_netif_set_from_netdev,
    .msg_handler = _dispatch_ppp_msg,
};

static void _gnrc_ppp_init(gnrc_netif_t *netif)
{
    netif->dev->event_callback = _gnrc_ppp_event_cb;
    netdev_t *dev = get_lowest_netdev(netif->dev);

    dcp_init(dev);
    lcp_init(dev);
    ipcp_init(dev);
    ppp_ipv4_init(dev);
    pap_init(dev);

    ((netdev_ppp_t *)dev)->netif = netif;
}


static int _send(gnrc_netif_t *netif, gnrc_pktsnip_t *pkt)
{
    int res = 0;
    ppp_hdr_t ppp_hdr;
    uint16_t type = GNRC_NETTYPE_UNDEF;

    netdev_t *dev = netif->dev;

    if (pkt != NULL) {
        iolist_t iolist = {
            .iol_next = (iolist_t *)pkt,
            .iol_base = &ppp_hdr,
            .iol_len = sizeof(ppp_hdr_t)
        };

        dev->driver->get(dev, NETOPT_DEVICE_TYPE, &type, sizeof(type));

        ppp_hdr.protocol = byteorder_htons(gnrc_nettype_to_ppp_protnum(pkt->type));

        /* add HDLC framing, see RFC 1662 */
        if(type == NETDEV_TYPE_PPPOS) {
            gnrc_pktsnip_t *hdr;
            netdev_ppp_t * ppp = (netdev_ppp_t *) get_lowest_netdev(dev);
           hdlc_hdr_t hdlc_hdr = {
               .address = 0xFF,
               .control.frame = 0x03,
           };

           if (pkt->type == GNRC_NETTYPE_NETIF) {
                /* we don't need the netif snip: remove it */
                pkt = gnrc_pktbuf_remove_snip(pkt, pkt);
            }

            hdr = gnrc_pktbuf_add(pkt, (void *)&hdlc_hdr, sizeof(hdlc_hdr_t),
                    GNRC_NETTYPE_HDLC);

            if (!hdr) {
                DEBUG(MODULE"no space left in packet buffer\n");
                goto safe_out;
            }

            if(gnrc_pkt_len(hdr) > ppp->lcp.peer_mru) {
                DEBUG(MODULE"sending exceeds peer MRU. Dropping packet.\n");
                res = -EBADMSG;
                goto safe_out;
            }

            iolist_t _hdlc_iolist = {
                .iol_next = &iolist,
                .iol_base = &hdlc_hdr,
                .iol_len = sizeof(hdlc_hdr_t)
            };

            res = dev->driver->send(dev, &_hdlc_iolist);

            goto safe_out;
        }

        res = dev->driver->send(dev, &iolist);
    }
    else {
        DEBUG(MODULE"pkt was NULL\n");
        res = -EINVAL;
        goto out;
    }

safe_out:
    gnrc_pktbuf_release(pkt);

out:
    return res;
}

int _prot_is_allowed(netdev_t *dev, uint16_t protocol)
{
    netdev_ppp_t *pppdev = (netdev_ppp_t*)dev;
    switch (protocol) {
        case PPPTYPE_LCP:
            return ((gnrc_ppp_protocol_t*) &pppdev->lcp)->state == PROTOCOL_STARTING || ((gnrc_ppp_protocol_t*) &pppdev->lcp)->state == PROTOCOL_UP;
        case PPPTYPE_NCP_IPV4:
            return ((gnrc_ppp_protocol_t*) &pppdev->ipcp)->state == PROTOCOL_STARTING || ((gnrc_ppp_protocol_t*) &pppdev->ipcp)->state == PROTOCOL_UP;
        case PPPTYPE_PAP:
            return ((gnrc_ppp_protocol_t*) &pppdev->pap)->state == PROTOCOL_STARTING;
        case PPPTYPE_IPV4:
            return ((gnrc_ppp_protocol_t*) &pppdev->ipv4)->state == PROTOCOL_UP;
    }
    return 0;
}

gnrc_ppp_protocol_t *_get_prot_by_target(netdev_ppp_t *pppdev, gnrc_ppp_target_t target)
{
    gnrc_ppp_protocol_t *target_prot;
    switch (target) {
        case PROT_LCP:
            target_prot = (gnrc_ppp_protocol_t *) &pppdev->lcp;
            break;
        case PROT_IPCP:
        case GNRC_PPP_BROADCAST_NCP:
            target_prot = (gnrc_ppp_protocol_t *) &pppdev->ipcp;
            break;
        case PROT_IPV4:
            target_prot = (gnrc_ppp_protocol_t *) &pppdev->ipv4;
            break;
        case PROT_DCP:
        case GNRC_PPP_BROADCAST_LCP:
            target_prot = (gnrc_ppp_protocol_t *) &pppdev->dcp;
            break;
        case PROT_AUTH:
            target_prot = (gnrc_ppp_protocol_t *) &pppdev->pap;
            break;
        default:
            target_prot = NULL;
    }
    return target_prot;
}

static gnrc_pktsnip_t *_recv(gnrc_netif_t *netif)
{
    int nbytes;
    netdev_t *dev = netif->dev;
    gnrc_pktsnip_t *pkt;

    /* get number of bytes to read */
    nbytes = dev->driver->recv(dev, NULL, 0, NULL);

    if(nbytes > 0) {
        int nread;
        uint16_t type = GNRC_NETTYPE_UNDEF;
        gnrc_pktsnip_t *ppp_hdr;
        ppp_hdr_t *hdr;
        netdev_ppp_t * ppp = (netdev_ppp_t *) get_lowest_netdev(dev);

        dev->driver->get(dev, NETOPT_DEVICE_TYPE, &type, sizeof(type));

        /* prepare packet */
        pkt = gnrc_pktbuf_add(NULL, NULL, nbytes, GNRC_NETTYPE_UNDEF);

        if(!pkt) {
            DEBUG(MODULE"cannot allocate pktsnip.\n");

            /* drop the packet */
            dev->driver->recv(dev, NULL, nbytes, NULL);

            goto out;
        }

        nread = dev->driver->recv(dev, pkt->data, nbytes, NULL);
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

        /* TODO check packet length against maximum receive unit (MRU)*/
        if(gnrc_pkt_len(pkt) > ppp->lcp.mru) {
            DEBUG(MODULE"receiving exceeds MRU. Dropping packet.\n");
            goto safe_out;
        }

        if (type == NETDEV_TYPE_PPPOS) {
            /* mark header */
            hdlc_hdr_t *hdlc;
            gnrc_pktsnip_t *hdlc_hdr = gnrc_pktbuf_mark(pkt, sizeof(hdlc_hdr_t),
                    GNRC_NETTYPE_HDLC);

            if (!hdlc_hdr) {
                DEBUG(MODULE"no space left in packet buffer\n");
                goto safe_out;
            }

            hdlc = (hdlc_hdr_t *)hdlc_hdr->data;
            if((hdlc->address != 0xFF) || (hdlc->control.frame != 0x03)){
                DEBUG(MODULE"unsupported hdlc frame\n");
                goto safe_out;
            }
        }

        /* mark header */
        ppp_hdr = gnrc_pktbuf_mark(pkt, sizeof(ppp_hdr_t), GNRC_NETTYPE_PPP);

        if (!ppp_hdr) {
            DEBUG(MODULE"no space left in packet buffer\n");
            goto safe_out;
        }

        hdr = (ppp_hdr_t *)ppp_hdr->data;

        DEBUG(MODULE"packet received - protocol: %04X\n", byteorder_ntohs(hdr->protocol));
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

gnrc_ppp_target_t _get_target_from_protocol(uint16_t protocol)
{
    switch (protocol) {
        case PPPTYPE_LCP:
            return PROT_LCP;
            break;
        case PPPTYPE_NCP_IPV4:
            return PROT_IPCP;
            break;
        case PPPTYPE_IPV4:
            return PROT_IPV4;
        case PPPTYPE_PAP:
            return PROT_AUTH;
        default:
            DEBUG("gnrc_ppp: Received unknown PPP protocol. Discard.\n");
    }
    return PROT_UNDEF;
}

static int _gnrc_ppp_dispatch(gnrc_netif_t *netif, gnrc_pktsnip_t *pkt)
{
    netdev_ppp_t *dev = (netdev_ppp_t *)get_lowest_netdev(netif->dev);
    ppp_hdr_t *hdr = (ppp_hdr_t *)pkt->next->data;

    gnrc_pktsnip_t *ret_pkt = NULL;

    gnrc_ppp_target_t target = _get_target_from_protocol(byteorder_ntohs(hdr->protocol));

    switch (target) {
        case PROT_LCP: {
            /* mark header */
            gnrc_pktsnip_t *lcp_hdr = gnrc_pktbuf_mark(pkt, sizeof(lcp_hdr_t), GNRC_NETTYPE_LCP);

            if(lcp_hdr) {
                fsm_handle_ppp_msg((gnrc_ppp_protocol_t*)&dev->lcp, PPP_RECV, pkt);
            }
            break;
        }
        case PROT_IPCP: {
            /* mark header */
            gnrc_pktsnip_t *ipcp_hdr = gnrc_pktbuf_mark(pkt, sizeof(lcp_hdr_t), GNRC_NETTYPE_IPCP);

            if(ipcp_hdr) {
                fsm_handle_ppp_msg((gnrc_ppp_protocol_t*)&dev->ipcp, PPP_RECV, pkt);
            }
            break;
        }

            break;
        case PROT_IPV4:
            ret_pkt = ppp_ipv4_recv(netif, pkt);
            break;
        case PROT_AUTH:
            pap_recv((gnrc_ppp_protocol_t*)&dev->pap, pkt);
            break;
        default:
            DEBUG(MODULE"unrecognized target: %u\n", byteorder_ntohs(hdr->protocol));
    }

    if(ret_pkt) {
        netif->ops->send(netif, ret_pkt);
    }

    return 0;
}

static void _gnrc_ppp_event_cb(netdev_t *dev, netdev_event_t event)
{
    gnrc_netif_t *netif = (gnrc_netif_t *)dev->context;
    netdev_ppp_t *ppp = (netdev_ppp_t *)get_lowest_netdev(netif->dev);

    if(event == NETDEV_EVENT_ISR) {
        msg_t msg;

        msg.type = NETDEV_MSG_TYPE_EVENT;
        msg.content.ptr = (void *)netif;

        if(msg_send(&msg, netif->pid) <= 0) {
            //DEBUG("gnrc_netdev: possibly lost interrupt.\n");
        }
    }
    else {
        DEBUG("gnrc_netdev: event triggered -> %u\n", event);
        switch (event) {
            case NETDEV_EVENT_RX_COMPLETE: {
                gnrc_pktsnip_t *pkt = netif->ops->recv(netif);
                if (pkt != NULL) {
                    int err = _gnrc_ppp_dispatch(netif, pkt);
                    if (err < 0) {
                        DEBUG(MODULE"failed to dispatch packet %d.\n", err);
                    }
                    gnrc_pktbuf_release(pkt);
                }
                break;
            }
            case NETDEV_EVENT_TX_COMPLETE:
                break;

            case NETDEV_EVENT_LAYER_UP:
                /* start negation */

                trigger_fsm_event((gnrc_ppp_fsm_t *) &ppp->lcp, PPP_E_OPEN, NULL);
                trigger_fsm_event((gnrc_ppp_fsm_t *) &ppp->ipcp, PPP_E_OPEN, NULL);

                break;

            default:
                DEBUG(MODULE"unhandled event %u.\n", event);
        }
    }
}

void _dispatch_ppp_msg(gnrc_netif_t *netif, msg_t *msg)
{
    gnrc_ppp_msg_t event = msg->content.value;

    if (msg->type == GNRC_PPP_MSG_TYPE_EVENT) {
        gnrc_ppp_target_t target = ppp_msg_get_target(event);
        gnrc_ppp_event_t e = ppp_msg_get_event(event);

        netdev_ppp_t *pppdev = (netdev_ppp_t*)netif->dev;

        gnrc_ppp_protocol_t *target_prot;

        if (event == PPP_RECV) {
            assert(false);
        }
        else {
            target_prot = _get_prot_by_target(pppdev, target);

            if (!target_prot) {
                DEBUG("Unrecognized target\n");
            }
            else {
                target_prot->handler(target_prot, e, NULL);
            }
        }
    }

}

void gnrc_ppp_trigger_event(msg_t *msg, kernel_pid_t pid, uint8_t target, uint8_t event)
{
    msg->type = GNRC_PPP_MSG_TYPE_EVENT;
    msg->content.value = (target << 8) | (event & 0xffff);
    msg_send(msg, pid);
}

gnrc_pktsnip_t *pkt_build(gnrc_nettype_t pkt_type, uint8_t code, uint8_t id, gnrc_pktsnip_t *payload)
{
    lcp_hdr_t hdr;

    hdr.code = code;
    hdr.id = id;

    int payload_length = payload ? payload->size : 0;
    hdr.length = byteorder_htons(payload_length + sizeof(ppp_hdr_t));

    gnrc_pktsnip_t *ppp_pkt = gnrc_pktbuf_add(payload, (void *) &hdr, sizeof(ppp_hdr_t), pkt_type);
    return ppp_pkt;
}

void send_packet(netdev_ppp_t *dev, gnrc_pktsnip_t *payload)
{
        dev->netif->ops->send(dev->netif, payload);
}


//#else   /* MODULE_NETDEV_PPP */
//typedef int dont_be_pedantic;
//#endif  /* MODULE_NETDEV_PPP */
