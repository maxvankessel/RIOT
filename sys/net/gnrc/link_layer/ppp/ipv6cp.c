/*
 * Copyright (C) 2015 José Ignacio Alaos
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 * @ingroup     net_gnrc_ppp
 * @file
 * @brief       Implementation of PPP's IPCP protocol
 *
 * @author      José Ignacio Alamos <jialamos@uc.cl>
 * @}
 */

#include "net/gnrc/ppp/ipv6cp.h"
#include "net/gnrc/ppp/ppp.h"

#include "net/ppp/hdr.h"

#include "net/gnrc/ppp/opt.h"
#include "net/gnrc/ppp/fsm.h"
#include "net/gnrc/pkt.h"
#include "net/gnrc/pktbuf.h"
#include "net/gnrc/nettype.h"

#include "net/udp.h"
#include "net/icmpv6.h"
#include "net/inet_csum.h"
#include <errno.h>

#if ENABLE_DEBUG
/* For PRIu16 etc. */
#include <inttypes.h>
#endif

#define IPCP_OPT_SIZE_ADDRESS (64 / 8)   /**< size of Address option */

static gnrc_ppp_fsm_conf_t *ipv6cp_get_conf_by_code(gnrc_ppp_fsm_t *cp, uint8_t code)
{
    switch (code) {
        case GNRC_PPP_IPV6CP_OPT_IFACE_ID:
            return &cp->conf[IPV6CP_IFACE_ID];
        default:
            return NULL;
    }
}

uint8_t ipv6cp_ipaddress_is_valid(gnrc_ppp_option_t *opt)
{
    (void)opt;
    return true;
}

uint8_t ipv6cp_ipaddress_build_nak_opts(uint8_t *buf)
{
    (void)buf;
    return 0;
}

void ipv6cp_ipaddress_set(gnrc_ppp_fsm_t *ipcp, gnrc_ppp_option_t *opt, uint8_t peer)
{
    uint8_t *payload;

    ppp_opt_get_payload(opt, (void **) &payload);
    if (!peer) {
        ((gnrc_ppp_ipv6cp_t *) ipcp)->ip.u64[0] = *((network_uint64_t *) payload);
        ((gnrc_ppp_ipv6cp_t *) ipcp)->ip.u64[1] = (network_uint64_t)0xFE80000000000000ULL;
    }
    else {
        ((gnrc_ppp_ipv6cp_t *) ipcp)->local_ip.u64[0] = *((network_uint64_t *) payload);
        ((gnrc_ppp_ipv6cp_t *) ipcp)->local_ip.u64[1] = (network_uint64_t)0xFE80000000000000ULL;
    }
}

static void ipv6cp_config_init(gnrc_ppp_fsm_t *ipcp)
{
    ipcp->conf = IPV6CP_NUMOPTS ? ((gnrc_ppp_ipv6cp_t *) ipcp)->ipcp_opts : NULL;

    ipcp->conf[IPV6CP_IFACE_ID].type = GNRC_PPP_IPV6CP_OPT_IFACE_ID;
    ipcp->conf[IPV6CP_IFACE_ID].default_value = byteorder_htonl(0);
    ipcp->conf[IPV6CP_IFACE_ID].size = IPCP_OPT_SIZE_ADDRESS;
    ipcp->conf[IPV6CP_IFACE_ID].flags = GNRC_PPP_OPT_ENABLED;
    ipcp->conf[IPV6CP_IFACE_ID].next = NULL;
    ipcp->conf[IPV6CP_IFACE_ID].is_valid = &ipv6cp_ipaddress_is_valid;
    ipcp->conf[IPV6CP_IFACE_ID].build_nak_opts = &ipv6cp_ipaddress_build_nak_opts;
    ipcp->conf[IPV6CP_IFACE_ID].set = &ipv6cp_ipaddress_set;
}

int ipv6cp_init(netdev_t *dev)
{
    netdev_ppp_t *pppdev = (netdev_ppp_t*) dev;
	gnrc_ppp_protocol_t *prot_ipcp = (gnrc_ppp_protocol_t*) &pppdev->ipcp;
    ppp_protocol_init(prot_ipcp, dev, fsm_handle_ppp_msg, PROT_IPCP);
    fsm_init((gnrc_ppp_fsm_t *) prot_ipcp);
    ipv6cp_config_init((gnrc_ppp_fsm_t *) prot_ipcp);

    //gnrc_ppp_ipv6cp_t *ipcp = (gnrc_ppp_ipv6cp_t *) prot_ipcp;
    gnrc_ppp_fsm_t *ipv6cp_fsm = (gnrc_ppp_fsm_t *) prot_ipcp;

    ipv6cp_fsm->supported_codes = FLAG_CONF_REQ | FLAG_CONF_ACK | FLAG_CONF_NAK | FLAG_CONF_REJ | FLAG_TERM_REQ | FLAG_TERM_ACK | FLAG_CODE_REJ;
    ipv6cp_fsm->prottype = GNRC_NETTYPE_IPV6CP;
    ipv6cp_fsm->restart_timer = GNRC_PPP_IPCP_RESTART_TIMER;
    ipv6cp_fsm->get_conf_by_code = &ipv6cp_get_conf_by_code;
    prot_ipcp->lower_layer = PROT_LCP;
    prot_ipcp->upper_layer = PROT_IP;

    return 0;
}

//gnrc_pktsnip_t *gen_ip_pkt(gnrc_ppp_ipv6_t *ip, gnrc_pktsnip_t *payload, uint8_t protocol)
//{
//    gnrc_pktsnip_t *pkt = gnrc_pktbuf_add(payload, NULL, sizeof(ipv6_hdr_t), GNRC_NETTYPE_IPV6);
//    ipv6_hdr_t *hdr = pkt->data;
//
//    ipv4_addr_t dst = ip->tunnel_addr;
//    netdev_t *gnrc_pppdev = ((gnrc_ppp_protocol_t *) ip)->dev;
//    netdev_ppp_t *pppdev = (netdev_ppp_t*) gnrc_pppdev;
//    gnrc_ppp_ipv6cp_t *ipcp = (gnrc_ppp_ipv6cp_t *) (&pppdev->ipcp);
//    ipv4_addr_t src = ipcp->ip;
//
//    ipv6_hdr_set_version(hdr);
//    ipv6_hdr_set_ihl(hdr, 5);
//    hdr->ts = 0;
//    hdr->tl = byteorder_htons(gnrc_pkt_len(pkt));
//    hdr->id = byteorder_htons(++(ipcp->ip_id));
//    ipv6_hdr_set_flags(hdr, 0);
//    ipv6_hdr_set_fo(hdr, 0);
//    hdr->ttl = 64;
//    hdr->protocol = protocol;
//    hdr->csum = byteorder_htons(0);
//    hdr->src = src;
//    hdr->dst = dst;
//
//    /*Calculate checkshum*/
//    hdr->csum = byteorder_htons(~inet_csum_slice(0, pkt->data, pkt->size, 0));
//
//    return pkt;
//}

int handle_ipv6(gnrc_ppp_protocol_t *protocol, uint8_t ppp_event, void *args)
{
    netdev_ppp_t *pppdev = (netdev_ppp_t*) protocol->dev;
    gnrc_ppp_ipv6cp_t *ipcp = (gnrc_ppp_ipv6cp_t *) &pppdev->ipcp;

    (void) ipcp;

    gnrc_pktsnip_t *recv_pkt = (gnrc_pktsnip_t *) args;
    (void) recv_pkt;

    switch (ppp_event) {
        case PPP_LINKUP:
            DEBUG("gnrc_ppp: Obtained IP address! \n");
            //DEBUG("Ip address is %i.%i.%i.%i\n", ipcp->ip.u8[0], ipcp->ip.u8[1], ipcp->ip.u8[2], ipcp->ip.u8[3]);
            protocol->state = PROTOCOL_UP;

            if(pppdev->netdev.event_callback != NULL) {
                pppdev->netdev.event_callback((netdev_t *)pppdev, NETDEV_EVENT_LINK_UP);
            }

            break;
        case PPP_LINKDOWN:
            DEBUG("gnrc_ppp: IPv6 down\n");
            protocol->state = PROTOCOL_DOWN;

            if(pppdev->netdev.event_callback != NULL) {
                pppdev->netdev.event_callback((netdev_t *)pppdev, NETDEV_EVENT_LINK_DOWN);
            }
            break;
        default:
            break;

    }
    return 0;
}


int ppp_ipv6_init(netdev_t *dev)
{
    netdev_ppp_t *pppdev = (netdev_ppp_t*) dev;
    gnrc_ppp_ipv6_t *ip = (gnrc_ppp_ipv6_t *) &pppdev->ip;

    ppp_protocol_init((gnrc_ppp_protocol_t*) ip, dev, handle_ipv6, PROT_IP);

    return 0;
}

int ppp_ipv6_send(gnrc_netif_t *netif, gnrc_pktsnip_t *pkt)
{
    (void)netif;
     (void)pkt;

    return -EINVAL;
}

gnrc_pktsnip_t *ppp_ipv6_recv(gnrc_netif_t *netif, gnrc_pktsnip_t *pkt)
{
    (void)netif;
    (void)pkt;

    return NULL;
}

