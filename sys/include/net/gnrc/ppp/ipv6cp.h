/*
 * Copyright (C) 2018
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    net_gnrc_ppp Definitions of IPV6CP
 * @ingroup     net_gnrc_ppp
 * @{
 *
 * @file
 * @brief  Definitions and configuration of Internet Protocol Control Protocol
 *
 * @author
 */
#ifndef GNRC_PPP_IPV6CP_H
#define GNRC_PPP_IPV6CP_H

#include "net/gnrc/ppp/fsm.h"

#include "net/ipv6.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GNRC_PPP_IPV6CP_OPT_IFACE_ID    (1)

#define GNRC_PPP_IPCP_RESTART_TIMER     (3000000U)   /**< restart time value for IPCP */


/**
 * @brief IPCP options
 */
typedef enum {
    IPV6CP_IFACE_ID,
    IPV6CP_NUMOPTS
} gnrc_ppp_ipv6cp_options_t;

/**
 * @brief definition of IPCP protocol
 * @extends ppp_fsm_t
 */
typedef struct gnrc_ppp_ipv6cp {
    gnrc_ppp_fsm_t fsm;                          /**< base fsm class */
    ipv6_addr_t local_ip;                        /**< local ip address obtained from ppp device */
    ipv6_addr_t ip;                              /**< ip address of ppp device */
    gnrc_ppp_fsm_conf_t ipcp_opts[IPV6CP_NUMOPTS]; /**< configuration options for IPCP */
    int ip_id;                                   /**< id of ip packet */
} gnrc_ppp_ipv6cp_t;

/**
 * @brief definition of the PPP IPv4 encapsulator
 * @extends ppp_protocol_t
 *
 * @details since most mobile operators don't support IPv6, it's necessary to use a tunnel for transmitting data
 */
typedef struct gnrc_ppp_ipv6 {
    gnrc_ppp_protocol_t prot;        /**< base ppp_protocol class */
} gnrc_ppp_ipv6_t;

struct gnrc_ppp_fsm_t;

/**
 * @brief init function of IPCP
 *
 * @param ppp_dev pointer to gnrc ppp interface
 * @param ipcp pointer to ipcp protocol
 *
 * @return 0
 */
int ipv6cp_init(netdev_t *dev);


/**
 * @brief init function for ipv4
 *
 * @param[in] dev
 *
 * @return 0
 */
int ppp_ipv6_init(netdev_t *dev);

/**
 * @brief send an encapsulated pkt
 *
 * @param ppp_dev pointer to to gnrc ppp interface
 * @param pkt packet to be sent
 *
 * @return negative value if there was an error
 * @return 0 otherwise
 */
int ppp_ipv6_send(gnrc_netif_t *netif, gnrc_pktsnip_t *pkt);


/**
 * @brief receive an encapsulated ipv4 packet for decapsulation
 *
 * @param ppp_dev pointer to gnrc ppp interface
 * @param pkt encapsulated packet
 *
 * @return
 */
gnrc_pktsnip_t *ppp_ipv6_recv(gnrc_netif_t *netif, gnrc_pktsnip_t *pkt);


#ifdef __cplusplus
}
#endif

#endif /* GNRC_PPP_IPV6CP_H */
/** @} */
