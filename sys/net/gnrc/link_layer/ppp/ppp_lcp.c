/*
 * Copyright (C) 2016 José Ignacio Alamos
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 * @ingroup     net_gnrc_ppp
 * @file
 * @brief       Implementation of PPP's LCP protocol
 *
 * @author      José Ignacio Alamos <jialamos@uc.cl>
 * @}
 */

#include "net/gnrc/ppp/lcp.h"
#include "net/gnrc/ppp/ppp.h"
#include <inttypes.h>
#include "net/gnrc/ppp/opt.h"
#include "net/gnrc/pkt.h"
#include "net/ppp/hdr.h"
#include "net/gnrc/pktbuf.h"
#include "net/gnrc/nettype.h"
#include <errno.h>
#include "net/gnrc/ppp/fsm.h"

#define ENABLE_DEBUG    (0)
#include "debug.h"

#if ENABLE_DEBUG
/* For PRIu16 etc. */
#include <inttypes.h>
#endif

#define GNRC_PPP_OPT_SIZE_MRU (2)        /**< size of MRU option */
#define GNRC_PPP_OPT_SIZE_AUTH_PAP (2)   /**< size of PAP option */
#define GNRC_PPP_OPT_SIZE_ACCM (4)       /**< size of ACCM option */

static gnrc_ppp_fsm_conf_t *lcp_get_conf_by_code(gnrc_ppp_fsm_t *cp, uint8_t code)
{
    switch (code) {
        case GNRC_PPP_LCP_OPT_MRU:
            return &cp->conf[LCP_MRU];
        case GNRC_PPP_LCP_OPT_ACCM:
            return &cp->conf[LCP_ACCM];
        case GNRC_PPP_LCP_OPT_AUTH:
            return &cp->conf[LCP_AUTH];
        case GNRC_PPP_LCP_OPT_PFC:
            return &cp->conf[LCP_PFC];
        default:
            return NULL;
    }
}

uint8_t lcp_mru_is_valid(gnrc_ppp_option_t *opt)
{
    uint8_t *payload;

    ppp_opt_get_payload(opt, (void **) &payload);
    uint16_t u16 = ((*payload) << 8) + *(payload + 1);
    if (u16 > GNRC_PPP_LCP_MAX_MRU) {
        return false;
    }
    return true;
}

uint8_t lcp_mru_build_nak_opts(uint8_t *buf)
{
    uint8_t len = GNRC_PPP_OPT_HDR_SIZE + GNRC_PPP_OPT_SIZE_MRU;
    gnrc_ppp_option_t *opt = (gnrc_ppp_option_t *) buf;
    network_uint16_t mru = byteorder_htons(GNRC_PPP_LCP_DEFAULT_MRU);

    if (opt) {
        ppp_opt_set_type(opt, GNRC_PPP_LCP_OPT_MRU);
        ppp_opt_set_length(opt, len);
        ppp_opt_set_payload(opt, &mru, sizeof(network_uint16_t));
    }
    return len;
}


void lcp_mru_set(gnrc_ppp_fsm_t *lcp, gnrc_ppp_option_t *opt, uint8_t peer)
{
    uint8_t *payload;

    ppp_opt_get_payload(opt, (void **) &payload);
    if (peer) {
        ((gnrc_ppp_lcp_t *) lcp)->peer_mru = byteorder_ntohs(*((network_uint16_t *) payload));
    }
    else {
        ((gnrc_ppp_lcp_t *) lcp)->mru = byteorder_ntohs(*((network_uint16_t *) payload));
    }
}


uint8_t lcp_accm_is_valid(gnrc_ppp_option_t *opt)
{
    (void)opt;

    /*Flags are always valid*/
    return true;
}

uint8_t lcp_accm_build_nak_opts(uint8_t *buf)
{
    (void)buf;

    /* Never called */
    return true;
}

void lcp_accm_set(gnrc_ppp_fsm_t *lcp, gnrc_ppp_option_t *opt, uint8_t peer)
{
    netdev_t *dev = ((gnrc_ppp_protocol_t *) lcp)->dev;
    uint8_t *payload;

    ppp_opt_get_payload(opt, (void **) &payload);
    if (peer) {
        dev->driver->set(dev, NETOPT_PPP_ACCM_RX, (void *) payload, sizeof(uint32_t));
    }
    else {
        dev->driver->set(dev, NETOPT_PPP_ACCM_TX, (void *) payload, sizeof(uint32_t));
    }
}

uint8_t lcp_auth_is_valid(gnrc_ppp_option_t *opt)
{
    network_uint16_t *u16;

    ppp_opt_get_payload(opt, (void **) &u16);
    uint16_t val = byteorder_ntohs(*u16);

    /*Only accept PAP*/
    if (val == PPPTYPE_PAP) {
        return true;
    }

    return false;
}

uint8_t lcp_auth_build_nak_opts(uint8_t *buf)
{
    uint8_t len = GNRC_PPP_OPT_HDR_SIZE + GNRC_PPP_OPT_SIZE_AUTH_PAP;
    gnrc_ppp_option_t *opt = (gnrc_ppp_option_t *) buf;
    network_uint16_t protnum = byteorder_htons(PPPTYPE_PAP);

    if (opt) {
        ppp_opt_set_type(opt, GNRC_PPP_LCP_OPT_AUTH);
        ppp_opt_set_length(opt, len);
        ppp_opt_set_payload(opt, &protnum, sizeof(network_uint16_t));
    }
    return len;
}

void lcp_auth_set(gnrc_ppp_fsm_t *lcp, gnrc_ppp_option_t *opt, uint8_t peer)
{
    (void)opt;
    gnrc_ppp_lcp_t *l = (gnrc_ppp_lcp_t *) lcp;

    if (peer) {
        l->local_auth = GNRC_PPP_AUTH_PAP;
    }
    else {
        l->remote_auth = GNRC_PPP_AUTH_PAP;
    }
}

uint8_t lcp_pcomp_is_valid(gnrc_ppp_option_t *opt)
{
    (void)opt;
    return true;
}


static void lcp_config_init(gnrc_ppp_fsm_t *lcp)
{
    lcp->conf = LCP_NUMOPTS ? ((gnrc_ppp_lcp_t *) lcp)->lcp_opts : NULL;

    gnrc_ppp_lcp_t *dev = (gnrc_ppp_lcp_t *)lcp;

    dev->lcp_opts[LCP_MRU].type = GNRC_PPP_LCP_OPT_MRU;
    dev->lcp_opts[LCP_MRU].default_value = byteorder_htonl(GNRC_PPP_LCP_DEFAULT_MRU);
    dev->lcp_opts[LCP_MRU].size = GNRC_PPP_OPT_SIZE_MRU;
    dev->lcp_opts[LCP_MRU].flags = 0;
    dev->lcp_opts[LCP_MRU].next = &dev->lcp_opts[LCP_ACCM];
    dev->lcp_opts[LCP_MRU].is_valid = &lcp_mru_is_valid;
    dev->lcp_opts[LCP_MRU].build_nak_opts = &lcp_mru_build_nak_opts;
    dev->lcp_opts[LCP_MRU].set = &lcp_mru_set;

    dev->lcp_opts[LCP_ACCM].type = GNRC_PPP_LCP_OPT_ACCM;
    dev->lcp_opts[LCP_ACCM].default_value = byteorder_htonl(GNRC_PPP_LCP_DEFAULT_ACCM);
    dev->lcp_opts[LCP_ACCM].size = GNRC_PPP_OPT_SIZE_ACCM;
    dev->lcp_opts[LCP_ACCM].flags = 0;
    dev->lcp_opts[LCP_ACCM].next = &dev->lcp_opts[LCP_AUTH];
    dev->lcp_opts[LCP_ACCM].is_valid = &lcp_accm_is_valid;
    dev->lcp_opts[LCP_ACCM].build_nak_opts = &lcp_accm_build_nak_opts;
    dev->lcp_opts[LCP_ACCM].set = &lcp_accm_set;

    dev->lcp_opts[LCP_AUTH].type = GNRC_PPP_LCP_OPT_AUTH;
    dev->lcp_opts[LCP_AUTH].default_value = byteorder_htonl(GNRC_PPP_LCP_DEFAULT_AUTH);
    dev->lcp_opts[LCP_AUTH].size = GNRC_PPP_OPT_SIZE_AUTH_PAP;
    dev->lcp_opts[LCP_AUTH].flags = 0;
    dev->lcp_opts[LCP_AUTH].next = &dev->lcp_opts[LCP_PFC];
    dev->lcp_opts[LCP_AUTH].is_valid = &lcp_auth_is_valid;
    dev->lcp_opts[LCP_AUTH].build_nak_opts = &lcp_auth_build_nak_opts;
    dev->lcp_opts[LCP_AUTH].set = &lcp_auth_set;

    dev->lcp_opts[LCP_PFC].type = GNRC_PPP_LCP_OPT_PFC;
    dev->lcp_opts[LCP_PFC].default_value = byteorder_htonl(0);
    dev->lcp_opts[LCP_PFC].size = 0;
    dev->lcp_opts[LCP_PFC].flags = 0;
    dev->lcp_opts[LCP_PFC].next = NULL;
    dev->lcp_opts[LCP_PFC].is_valid = &lcp_pcomp_is_valid;
    dev->lcp_opts[LCP_PFC].build_nak_opts = NULL;
    dev->lcp_opts[LCP_PFC].set = NULL;
}


int lcp_handler(gnrc_ppp_protocol_t *protocol, uint8_t ppp_event, void *args)
{
    gnrc_ppp_fsm_t *lcp = (gnrc_ppp_fsm_t *) protocol;

    if (ppp_event == PPP_MONITOR) {
        /*Send Echo Request*/
        DEBUG("gnrc_ppp: Sending echo request (link monitor)");
        gnrc_pktsnip_t *pkt = pkt_build(GNRC_NETTYPE_LCP, GNRC_PPP_ECHO_REQ, ((gnrc_ppp_lcp_t *) lcp)->monitor_id++, NULL);
        send_packet((netdev_ppp_t *)protocol->dev, pkt);
        return 0;
    }
    else if (ppp_event == PPP_UL_FINISHED) {
        send_ppp_event(&protocol->msg, ppp_msg_set(protocol->lower_layer, PPP_UL_FINISHED));
        return 0;
    }
    else {
        return fsm_handle_ppp_msg(protocol, ppp_event, args);
    }
}

int lcp_init(netdev_t *dev)
{
    netdev_ppp_t *pppdev = (netdev_ppp_t*) dev;
    gnrc_ppp_fsm_t *lcp = (gnrc_ppp_fsm_t *) &pppdev->lcp;

    ppp_protocol_init((gnrc_ppp_protocol_t*) lcp, dev, lcp_handler, PROT_LCP);
    fsm_init(lcp);
    lcp_config_init(lcp);

    lcp->supported_codes = FLAG_CONF_REQ | FLAG_CONF_ACK | FLAG_CONF_NAK | FLAG_CONF_REJ | FLAG_TERM_REQ | FLAG_TERM_ACK | FLAG_CODE_REJ | FLAG_ECHO_REQ | FLAG_ECHO_REP | FLAG_DISC_REQ;
    ((gnrc_ppp_lcp_t *)lcp)->pr_id = 0;
    lcp->prottype = GNRC_NETTYPE_LCP;
    lcp->restart_timer = GNRC_PPP_LCP_RESTART_TIMER;
    lcp->get_conf_by_code = &lcp_get_conf_by_code;
    ((gnrc_ppp_protocol_t *) lcp)->lower_layer = PROT_DCP;
    ((gnrc_ppp_protocol_t *) lcp)->upper_layer = PROT_AUTH;
    ((gnrc_ppp_lcp_t *) lcp)->mru = GNRC_PPP_LCP_DEFAULT_MRU;
    ((gnrc_ppp_lcp_t *) lcp)->peer_mru = GNRC_PPP_LCP_DEFAULT_MRU;
    ((gnrc_ppp_lcp_t *) lcp)->remote_auth = 0;
    ((gnrc_ppp_lcp_t *) lcp)->local_auth = 0;
    ((gnrc_ppp_lcp_t *) lcp)->monitor_id = 0;
    return 0;
}
