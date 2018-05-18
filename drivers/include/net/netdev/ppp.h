/*
 * Copyright (C) 2016 Fundación Inria Chile
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup drivers_netdev
 * @brief
 * @{
 *
 * @file
 * @brief   Definitions for netdev2 common Point to Point Protocol code
 *
 * @author  José Ignacio Alamos
 */
#ifndef NETDEV_PPP_H
#define NETDEV_PPP_H

//#include "net/gnrc/ppp/prot.h"
//
//#include "net/gnrc/ppp/lcp.h"
//#include "net/gnrc/ppp/pap.h"
//#include "net/gnrc/ppp/ipcp.h"

#include "net/gnrc/nettype.h"
#include "net/netopt.h"
#include "net/netdev.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(MODULE_GNRC_PPP) || DOXYGEN
/**
 * @brief class of custom driver control protocol
 * @extends ppp_protocol_t
 *
 * @details the DCP is in charge of monitoring the link and exchanging messages with the ppp device
 */
//typedef struct gnrc_ppp_dcp {
//    gnrc_ppp_protocol_t prot;   /**< base ppp_protocol_t object */
//    msg_t timer_msg;            /**< msg struct for handling timeouts messages */
//    xtimer_t xtimer;            /**< xtimer struct for sending timeout messages */
//    uint8_t dead_counter;       /**< when reaches zero, the link is assumed to be dead */
//} gnrc_ppp_dcp_t;
#endif


/**
 * @brief Extended structure to hold PPP driver state
 *
 * @extends netdev_t
 *
 * Supposed to be extended by driver implementations.
 * The extended structure should contain all variable driver state.
 */
typedef struct netdev_ppp {
    netdev_t netdev;                       /**< @ref netdev_t base class */
#if defined(MODULE_GNRC_PPP) || DOXYGEN
    /**
     * @brief PPP specific fields
     * @{
     */
//    gnrc_ppp_dcp_t dcp;                    /**< Control protocol for driver */
//    gnrc_ppp_lcp_t lcp;                    /**< Link Control Protocol */
//    gnrc_ppp_pap_t pap;                    /**< Password Authentication Protocol */
//    gnrc_ppp_ipcp_t ipcp;                  /**< IPv4 Network Control Protocol */
//    gnrc_ppp_ipv4_t ipv4;                  /**< Handler for IPv4 packets */
#endif
} netdev_ppp_t;


/**
 * @brief   Fallback function for netdev PPP devices' _get function
 *
 * Supposed to be used by netdev drivers as default case.
 *
 * @param[in]   dev     network device descriptor
 * @param[in]   opt     option type
 * @param[out]  value   pointer to store the option's value in
 * @param[in]   max_len maximal amount of byte that fit into @p value
 *
 * @return              number of bytes written to @p value
 * @return              <0 on error
 */
int netdev_ppp_get(netdev_ppp_t *dev, netopt_t opt, void *value,
                    size_t max_len);

/**
 * @brief   Fallback function for netdev PPP devices' _set function
 *
 *
 * @param[in] dev       network device descriptor
 * @param[in] opt       option type
 * @param[in] value     value to set
 * @param[in] value_len the length of @p value
 *
 * @return              number of bytes used from @p value
 * @return              <0 on error
 */
int netdev_ppp_set(netdev_ppp_t *dev, netopt_t opt, const void *value,
                    size_t value_len);

#ifdef __cplusplus
}
#endif

#endif /* NETDEV2_PPP_H */
/** @} */