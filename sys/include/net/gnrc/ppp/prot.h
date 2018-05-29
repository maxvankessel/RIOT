/*
 * Copyright (C) 2016 José Ignacio Alamos <jialamos@uc.cl>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    net_gnrc_ppp Generic PPP subprotocol
 * @ingroup     net_gnrc_ppp
 * @{
 *
 * @file
 * @brief  Definition of a generic PPP subprotocol.
 *
 * @author  José Ignacio Alamos <jialamos@uc.cl>
 */
#ifndef GNRC_PPP_PROTOCOL_H
#define GNRC_PPP_PROTOCOL_H

#include "net/gnrc.h"
#include "xtimer.h"
#include <inttypes.h>
#include <byteorder.h>
#include "net/ppp/hdr.h"
#include "msg.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief status of a PPP protocol */
typedef enum {
    PROTOCOL_DOWN,      /**< Protocol is not down */
    PROTOCOL_STARTING,  /**< Protocol is starting and setting up */
    PROTOCOL_UP         /**< Protocol is running */
} gnrc_ppp_protocol_state_t;

/**
 * @brief identifies of PPP protocol
 */
typedef enum {
    PROT_UNDEF,     /**< ppp protocol is undefined */
    PROT_DCP,       /**< ppp protocol is dcp (a.k.a not standard Driver Control Protocol) */
    PROT_LCP,       /**< ppp protocol is Link Control Protocol */
    PROT_AUTH,      /**< ppp protocol is PAP (CHAP not supported)*/
    PROT_IPCP,      /**< ppp protocol is Internet Protocol Control Protocol */
    PROT_IP,      /**< ppp protocol is IP packet encapsulator */
    NUM_OF_PROTS    /**< number of ppp protocols */
    /* add more if necessary */
} gnrc_ppp_protocol_id_t;

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
struct gnrc_ppp_protocol {
    /**
     * @brief handler of current protocol
     *
     * @details Whenever there's an event to a ppp protocol, this function is in charge of processing it.
     */
    int (*handler)(gnrc_ppp_protocol_t *protocol, uint8_t ppp_event, void *args);
    uint8_t id;                     /**< unique id of this protocol */
    msg_t msg;                      /**< msg structure for sending messages between protocols */
    netdev_t *dev;   /**< pointer to GNRC pppdev interface */
    gnrc_ppp_protocol_state_t state;     /**< state of current protocol */
    gnrc_ppp_target_t upper_layer;       /**< target of the upper layer of this protocol */
    gnrc_ppp_target_t lower_layer;       /**< target of the lower layer of this protocol */
};

/**
 * @brief sets a ppp message based on target and event
 *
 * @param target target protocol that should handle the event
 * @param ppp_event event for the target protocol
 *
 * @return PPP message representing target and event
 */
static inline gnrc_ppp_msg_t ppp_msg_set(gnrc_ppp_target_t target, gnrc_ppp_event_t ppp_event)
{
    return (target << 8) | ppp_event;
}

/**
 * @brief given a ppp message, get the target
 *
 * @param ppp_msg ppp message
 *
 * @return the requested target
 */
static inline gnrc_ppp_target_t ppp_msg_get_target(gnrc_ppp_msg_t ppp_msg)
{
    return (ppp_msg >> 8);
}

/**
 * @brief given a ppp message, get the event
 *
 * @param ppp_msg ppp message
 *
 * @return the requested event
 */
static inline gnrc_ppp_event_t ppp_msg_get_event(gnrc_ppp_msg_t ppp_msg)
{
    return (ppp_msg & 0xffff);
}

/**
 * @brief inits a ppp protocol
 *
 * @param protocol pointer to ppp protocol
 * @param pppdev pointer to gnrc pppdev interface
 * @param handler pointer to handler callback
 * @param id unique id of this protocol
 */
static inline void ppp_protocol_init(gnrc_ppp_protocol_t *protocol, netdev_t *dev, int (*handler)(gnrc_ppp_protocol_t *, uint8_t, void *), uint8_t id)
{
    protocol->handler = handler;
    protocol->id = id;
    protocol->dev = dev;
    protocol->state = PROTOCOL_DOWN;
}


#ifdef __cplusplus
}
#endif

#endif /* GNRC_PPP_PROTOCOL_H */
/** @} */
