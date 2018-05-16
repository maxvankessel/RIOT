/*
 * Copyright (C) 2015 José Ignacio Alamos <jialamos@uc.cl>
 * Copyright (C) 2018 Max van Kessel
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup net_gnrc_netif
 * @{
 *
 * @file
 * @brief   Point to Point Protocol adaption for @ref net_gnrc_netif
 *
 * @author  José Ignacio Alamos <jialamos@uc.cl>
 * @author  Max van Kessel
 */

#ifndef NET_GNRC_NETIF_PPP_H
#define NET_GNRC_NETIF_PPP_H

#include "net/gnrc/netif.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Creates an Point to Point network interface
 *
 * @param[in] stack     The stack for the network interface's thread.
 * @param[in] stacksize Size of @p stack.
 * @param[in] priority  Priority for the network interface's thread.
 * @param[in] name      Name for the network interface. May be NULL.
 * @param[in] dev       Device for the interface
 *
 * @see @ref gnrc_netif_create()
 *
 * @return  The network interface on success.
 * @return  NULL, on error.
 */
gnrc_netif_t *gnrc_netif_ppp_create(char *stack, int stacksize, char priority,
                                        char *name, netdev_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* NET_GNRC_NETIF_PPP_H */
/** @} */
