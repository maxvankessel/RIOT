/*
 * Copyright (C) 2015 José Ignacio Alamos <jialamos@uc.cl>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    net_gnrc_ppp gnrc ppp definitions
 * @ingroup     net_gnrc_ppp
 * @{
 *
 * @file
 * @brief  Definitions and interface of gnrc ppp
 *
 * @author  José Ignacio Alamos <jialamos@uc.cl>
 */

#ifndef GNRC_PPP_H
#define GNRC_PPP_H

#include <inttypes.h>

#include "net/gnrc.h"
#include "net/ppp/hdr.h"
#include "net/gnrc/pkt.h"
#include "net/gnrc/pktbuf.h"
#include "xtimer.h"
#include "net/gnrc/ppp/prot.h"
#include "net/netdev/ppp.h"
#include "sys/uio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GNRC_PPP_MSG_QUEUE 64

#define GNRC_PPP_HDLC_ADDRESS (0xFF) /**< HDLC address field for PPP */
#define GNRC_PPP_HDLC_CONTROL (0x03) /**< HDLC control field for PPP */

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


#define GNRC_PPP_MSG_QUEUE_SIZE (20)

#define GNRC_PPP_MSG_TYPE_EVENT (101)    /**< Messages for GNRC PPP layer */

#define GNRC_PPP_DCP_MONITOR_INIT_DELAY (15000000)   /**< Time that the monitor should wait after the LCP initiation before monitoring */
#define GNRC_PPP_DCP_MONITOR_TIMEOUT (10000000)      /**< time between LCP Echo request monitoriin */
#define GNRC_PPP_DCP_DEAD_COUNTER (5)                /**< Number of failed LCP Echo Request responses before assuming the ppp device is dead */


/**
 * @brief list of events for gnrc_ppp
 */
typedef enum {
    PPP_LINKUP,         /**< link up event for a protocol */
    PPP_RECV,           /**< protocol received a packet */
    PPP_TIMEOUT,        /**< protocol received a timeout message */
    PPP_LINKDOWN,       /**< link down event for a protocol */
    PPP_UL_STARTED,     /**< upper layer of a protocol started */
    PPP_UL_FINISHED,    /**< upper layer of a protocol finished */
    PPP_MONITOR,        /**< Message for the monitor */
    PPP_LINK_ALIVE,     /**< Message from LCP to DCP indicating the link is alive */
} gnrc_ppp_dev_event_t;


static inline void send_ppp_event(msg_t *msg, gnrc_ppp_msg_t ppp_msg)
{
    msg->type = GNRC_PPP_MSG_TYPE_EVENT;
    msg->content.value = ppp_msg;
    msg_send(msg, thread_getpid());
}

static inline void send_ppp_event_xtimer(msg_t *msg, xtimer_t *xtimer, gnrc_ppp_msg_t ppp_msg, int timeout)
{
    msg->type = GNRC_PPP_MSG_TYPE_EVENT;
    msg->content.value = ppp_msg;
    xtimer_remove(xtimer);
    xtimer_set_msg(xtimer, timeout, msg, thread_getpid());
}

gnrc_pktsnip_t *pkt_build(gnrc_nettype_t pkt_type, uint8_t code, uint8_t id, gnrc_pktsnip_t *payload);


/**
 * @brief init function for DCP
 *
 * @param ppp_dev pointer to gnrc ppp interface
 * @param dcp pointer to dcp protocol
 *
 * @return 0
 */
int dcp_init(netdev_t *dev);


void send_packet(netdev_ppp_t *dev, gnrc_pktsnip_t *payload);

/**
 * @brief send a configure request packet
 *
 * @param dev pointer to gnrc ppp interface
 * @param protocol nettype of packet
 * @param id id of packet
 * @param payload payload of configure request packet
 */
void send_configure_request(netdev_t *dev, gnrc_nettype_t protocol, uint8_t id, gnrc_pktsnip_t *payload);

/**
 * @brief send a configure ack packet
 *
 * @param dev pointer to gnrc ppp interface
 * @param protocol nettype of packet
 * @param id id of packet
 * @param opts ACK'd options
 */
void send_configure_ack(netdev_t *dev, gnrc_nettype_t protocol, uint8_t id, gnrc_pktsnip_t *opts);

/**
 * @brief send a configure nak packet
 *
 * @param dev pointer to gnrc ppp interface
 * @param protocol nettype of packet
 * @param id id of packet
 * @param opts NAK'd options
 */
void send_configure_nak(netdev_t *dev, gnrc_nettype_t protocol, uint8_t id, gnrc_pktsnip_t *opts);

/**
 * @brief send configure reject
 *
 * @param dev pointer to gnrc ppp interface
 * @param protocol nettype of packet
 * @param id id of packet
 * @param opts rejected options
 */
void send_configure_rej(netdev_t *dev, gnrc_nettype_t protocol, uint8_t id, gnrc_pktsnip_t *opts);

/**
 * @brief
 *
 * @param dev pointer to gnrc ppp interface
 * @param protocol nettype of packet
 * @param id id of packet
 */
void send_terminate_req(netdev_t *dev, gnrc_nettype_t protocol, uint8_t id);

/**
 * @brief send terminate ack
 *
 * @param dev pointer to gnrc ppp interface
 * @param protocol nettype of packet
 * @param id id of packet
 * @param response response of terminate request
 */
void send_terminate_ack(netdev_t *dev, gnrc_nettype_t protocol, uint8_t id, gnrc_pktsnip_t *response);

/**
 * @brief send code reject packet
 *
 * @param dev pointer to gnrc ppp interface
 * @param protocol nettype of packet
 * @param id id of packet
 * @param rejected rejected packet including ppp header
 */
void send_code_rej(netdev_t *dev, gnrc_nettype_t protocol, uint8_t id, gnrc_pktsnip_t *rejected);

/**
 * @brief send echo reply
 *
 * @param dev pointer to gnrc ppp interface
 * @param protocol nettype of packet
 * @param id id of packet
 * @param data data of the echo reply
 */
void send_echo_reply(netdev_t *dev, gnrc_nettype_t protocol, uint8_t id, gnrc_pktsnip_t *data);

/**
 * @brief send protocol reject packet
 *
 * @param dev pointer to gnrc ppp interface
 * @param protocol nettype of packet
 * @param id id of packet
 * @param pkt rejected packet
 */
void send_protocol_reject(netdev_t *dev, uint8_t id, gnrc_pktsnip_t *pkt);

/**
 * @brief send PAP auth request
 *
 * @param dev pointer to gnrc ppp interface
 * @param protocol nettype of packet
 * @param id id of packet
 * @param credentials credentials of the PAP request
 */
void send_pap_request(netdev_t *dev, uint8_t id, gnrc_pktsnip_t *credentials);

#ifdef __cplusplus
}
#endif

#endif /* GNRC_PPP_H */
/** @} */
