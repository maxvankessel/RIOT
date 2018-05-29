/*
 * Copyright (C) Fundación Inria Chile 2017
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     driver_netdev_ppp
 * @{
 *
 * @file
 * @brief       Common code for netdev ppp drivers
 *
 * @author      José Ignacio Alamos <jose.alamos@inria.cl>
 *
 * @}
 */

#include <assert.h>
#include <errno.h>

#include "net/netdev.h"
#include "net/netdev/ppp.h"

#define ENABLE_DEBUG (0)
#include "debug.h"

int netdev_ppp_get(netdev_ppp_t *dev, netopt_t opt, void *value, size_t max_len)
{
    int res = -ENOTSUP;

    (void)max_len;

    switch (opt) {
        case NETOPT_PPP_LCP_STATE:
            *((uint8_t *) value) = ((gnrc_ppp_protocol_t*) &dev->lcp)->state;
            res = 0;
            break;
        case NETOPT_PPP_AUTH_STATE:
            *((uint8_t *) value) = ((gnrc_ppp_protocol_t*) &dev->pap)->state;
            res = 0;
            break;
        case NETOPT_PPP_IPCP_STATE:
            *((uint8_t *) value) = ((gnrc_ppp_protocol_t*) &dev->ipcp)->state;
            res = 0;
            break;
        case NETOPT_PPP_IS_IPV6_READY:
            res = ((gnrc_ppp_protocol_t*) &dev->ipv4)->state == PROTOCOL_UP;
            *((uint8_t *) value) = res;
            break;
        case NETOPT_STATE: {
                gnrc_ppp_protocol_t *dcp = (gnrc_ppp_protocol_t *) &dev->dcp;
                *((netopt_enable_t *)value) = (dcp->state == PROTOCOL_DOWN) ?
                                              NETOPT_DISABLE : NETOPT_ENABLE;
                res = sizeof(netopt_enable_t);
            };
            break;
        default:
            return -ENOTSUP;
            break;
    }
    return res;
}

int netdev_ppp_set(netdev_ppp_t *dev, netopt_t opt, const void *value, size_t value_len)
{
    int res = -ENOTSUP;

    switch (opt) {
        case NETOPT_TUNNEL_IPV4_ADDRESS:
            dev->ipv4.tunnel_addr = *((ipv4_addr_t *) value);
            res = 0;
            break;
        case NETOPT_TUNNEL_UDP_PORT:
            dev->ipv4.tunnel_port = *((uint16_t *) value);
            res = 0;
            break;
        case NETOPT_APN_USER:
            memcpy(dev->pap.username, value, value_len);
            dev->pap.user_size = value_len;
            res = 0;
            break;
        case NETOPT_APN_PASS:
            memcpy(dev->pap.password, value, value_len);
            dev->pap.pass_size = value_len;
            res = 0;
            break;
        case NETOPT_STATE:
            if (*((netopt_enable_t *)value) == NETOPT_DISABLE) {
                /* set link down in device context
                 * should not require dispatch_ppp_msg */
            }
            else {
                /* set link up in device context
                 * should not require dispatch_ppp_msg */
            }
            break;
        default:
            return -ENOTSUP;
            break;
    }
    return res;
}
