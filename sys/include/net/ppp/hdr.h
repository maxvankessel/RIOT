/*
 * Copyright (C) 2016 José Ignacio Alamos <jialamos@uc.cl>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    net_ppphdr Point-to-Point Protocol Header
 * @ingroup     net_ppp
 * @brief       PPP header abstraction type and helper functions
 * @{
 *
 * @file
 * @brief   General definitions for PPP header and their helper functions
 *
 * @author  José Ignacio Alamos
 */

#ifndef NET_PPP_HDR_H
#define NET_PPP_HDR_H

#include <inttypes.h>

#include "byteorder.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief   Header of a PPP packet
 * @details A PPP packet is transmited as a payload of an HDLC packet. PPP packets only carry information about control protocol
 * of a PPP stack (Link Control Protocol, IP Control Protocol, etc). IP packets encapsulated in HDLC frame are not
 * considered PPP packet.
 *
 *  A summary of the PPP encapsulation is shown below.  The fields are
 *  transmitted from left to right.
 *
 * +----------+-------------+---------+
 * | Protocol | Information | Padding |
 * | 8/16 bits|      *      |    *    |
 * +----------+-------------+---------+
 *
 * @see <a href="https://tools.ietf.org/html/rfc1661#section-2">
 *          RFC 1661, section 2
 *      </a>
 */
/*  PPP pkt header struct */
typedef struct __attribute__((packed)){
    network_uint16_t protocol;  /**< protocol field, identifies datagram encapsulated in information field*/
} ppp_hdr_t;

#define LCP_CONF_REQ (1)        /**< Code of Configure Request packet */
#define LCP_CONF_ACK (2)        /**< Code of Configure Ack packet */
#define LCP_CONF_NAK (3)        /**< Code of Configure NAK packet */
#define LCP_CONF_REJ (4)        /**< Code of Configure Reject packet */
#define LCP_TERM_REQ (5)        /**< Code of Temrminate Request packet */
#define LCP_TERM_ACK (6)        /**< Code of Terminate ACK packet */
#define LCP_CODE_REJ (7)        /**< Code of Code Reject packet */
#define LCP_PROT_REJ (8)        /**< Code of Protocol Reject packet */
#define LCP_ECHO_REQ (9)        /**< Code of Echo Request packet */
#define LCP_ECHO_REP (10)       /**< Code of Echo Reply packet */
#define LCP_DISC_REQ (11)       /**< Code of Discard Request packet */

/**
 * @brief   Header of a LCP packet
 *
 * A summary of the Link Control Protocol packet format is shown below.
 * The fields are transmitted from left to right.
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |     Code      |  Identifier   |            Length             |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |    Payload ...
 * +-+-+-+-+
 *
 * Exactly one LCP packet is encapsulated in the PPP Information field,
 * where the PPP Protocol field indicates type hex c021 (Link Control Protocol).
 *
 *
 * @see <a href="https://tools.ietf.org/html/rfc1661#section-5">
 *          RFC 1661, section 5
 *      </a>
 */
/*  LCP pkt header struct */
typedef struct __attribute__((packed)){
    uint8_t code;               /**< Code of PPP packet*/
    uint8_t id;                 /**< Identifier PPP of packet*/
    network_uint16_t length;    /**< Length of PPP packet including payload*/
} lcp_hdr_t;

#ifdef __cplusplus
}
#endif

#endif /* NET_PPP_HDR_H */
/** @} */
