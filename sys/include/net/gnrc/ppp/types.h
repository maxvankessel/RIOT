/*
 * Copyright (C) 2018 Max van Kessel
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     net_gnrc_ppp
 * @{
 *
 * @file
 * @brief       Internal used types of PPP
 * @internal
 * @author      Max van Kessel
 */

#ifndef NET_GNRC_PPP_TYPES_H
#define NET_GNRC_PPP_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#include "net/gnrc.h"
#include "net/ppp/hdr.h"

#include "xtimer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GNRC_PPP_AUTH_PAP (1)            /**< Label of PAP authentication */

#define GNRC_PPP_CONF_REQ (1)        /**< Code of Configure Request packet */
#define GNRC_PPP_CONF_ACK (2)        /**< Code of Configure Ack packet */
#define GNRC_PPP_CONF_NAK (3)        /**< Code of Configure NAK packet */
#define GNRC_PPP_CONF_REJ (4)        /**< Code of Configure Reject packet */
#define GNRC_PPP_TERM_REQ (5)        /**< Code of Temrminate Request packet */
#define GNRC_PPP_TERM_ACK (6)        /**< Code of Terminate ACK packet */
#define GNRC_PPP_CODE_REJ (7)        /**< Code of Code Reject packet */
#define GNRC_PPP_PROT_REJ (8)        /**< Code of Protocol Reject packet */
#define GNRC_PPP_ECHO_REQ (9)        /**< Code of Echo Request packet */
#define GNRC_PPP_ECHO_REP (10)       /**< Code of Echo Reply packet */
#define GNRC_PPP_DISC_REQ (11)       /**< Code of Discard Request packet */
#define GNRC_PPP_IDENT (12)          /**< Code of Identification (not used yet) */
#define GNRC_PPP_TIME_REM (13)       /**< Code of Time Remaining /not used yet) */
#define GNRC_PPP_UNKNOWN_CODE (0)    /**< Code for Unknown Code packet (internal use)*/

#define GNRC_PPP_BROADCAST_LCP (0xff)    /**< Shortcut to LCP message */
#define GNRC_PPP_BROADCAST_NCP (0xfe)    /**< Broadcast message to al NCP available */

#define GNRC_PPP_MSG_TYPE_EVENT (101)    /**< Messages for GNRC PPP layer */

#define GNRC_PPP_DCP_MONITOR_INIT_DELAY (15000000)   /**< Time that the monitor should wait after the LCP initiation before monitoring */
#define GNRC_PPP_DCP_MONITOR_TIMEOUT (10000000)      /**< time between LCP Echo request monitoriin */
#define GNRC_PPP_DCP_DEAD_COUNTER (5)                /**< Number of failed LCP Echo Request responses before assuming the ppp device is dead */

/**
 * @brief PPP message type.
 *
 * @details The first 8 bytes are the target and the remaining 8 bytes are the event
 */
typedef uint16_t gnrc_ppp_msg_t;
typedef uint8_t gnrc_ppp_target_t;   /**< ppp target type */
typedef uint8_t gnrc_ppp_event_t;    /**< ppp event type */

typedef struct gnrc_ppp_protocol gnrc_ppp_protocol_t;

/**
 * @brief Base class of a generic PPP protocol
 */
typedef struct gnrc_ppp_protocol {
    /**
     * @brief handler of current protocol
     *
     * @details Whenever there's an event to a ppp protocol, this function is in charge of processing it.
     */
    int (*handler)(gnrc_ppp_protocol_t *protocol, uint8_t ppp_event, void *args);
    uint8_t id;                     /**< unique id of this protocol */
    msg_t msg;                      /**< msg structure for sending messages between protocols */
    netif *pppdev;                 /**< pointer to GNRC pppdev interface */
    gnrc_ppp_protocol_state_t state;     /**< state of current protocol */
    gnrc_ppp_target_t upper_layer;       /**< target of the upper layer of this protocol */
    gnrc_ppp_target_t lower_layer;       /**< target of the lower layer of this protocol */
} gnrc_ppp_protocol_t;

/**
 * @brief class of custom driver control protocol
 * @extends ppp_protocol_t
 *
 * @details the DCP is in charge of monitoring the link and exchanging messages with the ppp device
 */
typedef struct gnrc_ppp_dcp {
    gnrc_ppp_protocol_t prot;    /**< base ppp_protocol_t object */
    msg_t timer_msg;        /**< msg struct for handling timeouts messages */
    xtimer_t xtimer;        /**< xtimer struct for sending timeout messages */
    uint8_t dead_counter;   /**< when reaches zero, the link is assumed to be dead */
} gnrc_ppp_dcp_t;

typedef struct gnrc_netif_ppp
{


} gnrc_netif_ppp_t;

#ifdef __cplusplus
}
#endif

#endif /* NET_GNRC_PPP_TYPES_H */
/** @} */
