/*
 * Copyright (C) 2015 José Ignacio Alamos <jialamos@uc.cl>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 * @ingroup     net_gnrc_ppp
 * @file
 * @brief       Implementation of the Option Negotiation Automaton FSM
 *
 * @author      José Ignacio Alamos <jialamos@uc.cl>
 *
 * @see <a href="https://tools.ietf.org/html/rfc1661#page-11">
 *				RFC 1661, page 11.
 *		</a>
 * @}
 */
#include <errno.h>

#include "msg.h"
#include "thread.h"
#include "net/gnrc.h"
#include "net/ppptype.h"
#include "net/gnrc/ppp/ppp.h"
#include "net/gnrc/ppp/fsm.h"
#include "net/hdlc/hdr.h"
#include "net/ppp/hdr.h"
#include <errno.h>
#include <string.h>

#define ENABLE_DEBUG    (1)
#include "debug.h"

#if ENABLE_DEBUG
/* For PRIu16 etc. */
#include <inttypes.h>
#endif

#define MODULE "gnrc_ppp_fsm: "

#define FOR_EACH_OPTION(opt, buf, size) \
    for (gnrc_ppp_option_t *opt = (gnrc_ppp_option_t *) buf; opt != NULL; opt = ppp_opt_get_next(opt, (gnrc_ppp_option_t *) buf, size))

#define FOR_EACH_CONF(conf, head) \
    for (gnrc_ppp_fsm_conf_t *conf = head; conf != NULL; conf = conf->next)

lcp_hdr_t *_get_hdr(gnrc_pktsnip_t *pkt)
{
    return pkt->type == GNRC_NETTYPE_UNDEF ? (lcp_hdr_t *) pkt->next->data : (lcp_hdr_t *) pkt->data;
}

int _pkt_has_payload(lcp_hdr_t *hdr)
{
    return byteorder_ntohs(hdr->length) > sizeof(ppp_hdr_t);
}

void set_timeout(gnrc_ppp_fsm_t *cp, uint32_t time)
{
    gnrc_ppp_target_t self = ((gnrc_ppp_protocol_t *)cp)->id;

    send_ppp_event_xtimer(&((gnrc_ppp_protocol_t *) cp)->msg, &cp->xtimer, ppp_msg_set(self, PPP_TIMEOUT), time);
}

void _reset_cp_conf(gnrc_ppp_fsm_conf_t *conf)
{
    FOR_EACH_CONF(c, conf){
        conf->value = conf->default_value;
    }
}
size_t _opts_size(gnrc_ppp_fsm_conf_t *head_conf)
{
    size_t size = 0;

    FOR_EACH_CONF(conf, head_conf){
        if (conf->flags & GNRC_PPP_OPT_ENABLED) {
            size += 2 + conf->size;
        }
    }
    return size;
}

void _write_opts(gnrc_ppp_fsm_conf_t *head_conf, uint8_t *buf)
{
    int cursor = 0;

    FOR_EACH_CONF(conf, head_conf){
        if (conf->flags & GNRC_PPP_OPT_ENABLED) {
            cursor += ppp_opt_fill(buf + cursor, conf->type, (&conf->value) + sizeof(uint32_t) - conf->size, conf->size);
        }
    }
}

gnrc_pktsnip_t *_build_conf_req_options(gnrc_ppp_fsm_t *cp)
{
    /* Get size of outgoing options */
    size_t size = _opts_size(cp->conf);

    if (!size) {
        return NULL;
    }

    /* Write opts to pkt */
    gnrc_pktsnip_t *opts = gnrc_pktbuf_add(NULL, NULL, size, GNRC_NETTYPE_UNDEF);
    _write_opts(cp->conf, (uint8_t *) opts->data);
    return opts;
}

static uint8_t get_scnpkt_data(gnrc_ppp_fsm_t *cp, gnrc_pktsnip_t *pkt, uint16_t *n)
{
    uint8_t rej_size = 0;
    uint8_t nak_size = 0;
    uint8_t curr_type;

    gnrc_ppp_fsm_conf_t *curr_conf;
    uint8_t curr_size;

    FOR_EACH_OPTION(opt, pkt->data, pkt->size){
        curr_type = ppp_opt_get_type(opt);
        curr_conf = cp->get_conf_by_code(cp, curr_type);
        curr_size = ppp_opt_get_length(opt);
        if (curr_conf == NULL) {
            rej_size += curr_size;
        }
        else if (!curr_conf->is_valid(opt)) {
            nak_size += curr_conf->build_nak_opts(NULL);
        }
    }

    /* Append required options */
    FOR_EACH_CONF(conf, cp->conf){
        if (conf->flags & GNRC_PPP_OPT_REQUIRED) {
            nak_size += conf->size;
        }
    }

    *n = rej_size ? rej_size : nak_size;
    return rej_size ? GNRC_PPP_CONF_REJ : GNRC_PPP_CONF_NAK;
}

static void build_nak_pkt(gnrc_ppp_fsm_t *cp, gnrc_pktsnip_t *pkt, uint8_t *buf)
{
    gnrc_ppp_fsm_conf_t *curr_conf;
    uint8_t curr_type;
    uint8_t cursor = 0;

    FOR_EACH_OPTION(opt, pkt->data, pkt->size){
        curr_type = ppp_opt_get_type(opt);
        curr_conf = cp->get_conf_by_code(cp, curr_type);

        if (curr_conf && !curr_conf->is_valid(opt)) {
            cursor += curr_conf->build_nak_opts(buf + cursor);
        }
    }
}

static void build_rej_pkt(gnrc_ppp_fsm_t *cp, gnrc_pktsnip_t *pkt, uint8_t *buf)
{
    gnrc_ppp_fsm_conf_t *curr_conf;

    uint8_t curr_type;
    uint16_t curr_size;

    uint8_t cursor = 0;

    FOR_EACH_OPTION(opt, pkt->data, pkt->size){
        curr_type = ppp_opt_get_type(opt);
        curr_conf = cp->get_conf_by_code(cp, curr_type);
        curr_size = ppp_opt_get_length(opt);

        if (curr_conf == NULL) {
            memcpy(buf + cursor, opt, curr_size);
            cursor += curr_size;
        }
    }
}

#if ENABLE_DEBUG
static const char *  print_state(int state)
{
    switch (state) {
        case PPP_S_INITIAL:
            return("INITIAL");
        case PPP_S_CLOSED:
            return("CLOSED");
        case PPP_S_STOPPED:
            return("STOPPED");
        case PPP_S_CLOSING:
            return("CLOSING");
        case PPP_S_STOPPING:
            return("STOPPING");
        case PPP_S_REQ_SENT:
            return("REQ_SENT");
        case PPP_S_ACK_RCVD:
            return("ACK_RECV");
        case PPP_S_ACK_SENT:
            return("ACK_SENT");
        case PPP_S_OPENED:
            return("OPENED");
        case PPP_S_UNDEF:
        default:
            return("UNDEF");
    }
}
static const char * print_event(uint8_t event)
{
    switch (event) {
        case PPP_E_UP:
            return ("UP");
        case PPP_E_DOWN:
            return("DOWN");
        case PPP_E_CLOSE:
            return("CLOSE");
        case PPP_E_TOp:
            return("TO+");
        case PPP_E_TOm:
            return("TO-");
        case PPP_E_RCRp:
            return("RCR+");
        case PPP_E_RCRm:
            return("RCR-");
        case PPP_E_RCA:
            return("RCA");
        case PPP_E_RCN:
            return("RCN");
        case PPP_E_RTR:
            return("RTR");
        case PPP_E_RTA:
            return("RTA");
        case PPP_E_RUC:
            return("RUC");
        case PPP_E_RXJp:
            return("RXJ+");
        case PPP_E_RXJm:
            return("RXJ-");
        case PPP_E_RXR:
            return("RXR");
    }

    return "UNKOWN";
}

static const char * print_prot(int prot)
{
    switch(prot) {

        case PROT_DCP:
            return "DCP";

        case PROT_LCP:
            return "LCP";

        case PROT_AUTH:
            return "AUTH";

        case PROT_IPCP:
            return "IPCP";

        case PROT_IPV4:
            return "IPV4";

        case PROT_UNDEF:
        default:
            return "UNDEF";
    }
}

static void print_transition(int layer, int state, uint8_t event, int next_state)
{
    DEBUG(MODULE" %s state change %s -> %s, with event %s\n", print_prot(layer),
            print_state(state), print_state(next_state), print_event(event));
}
#endif

gnrc_ppp_target_t _fsm_upper_layer(gnrc_ppp_fsm_t *cp)
{
    return ((gnrc_ppp_protocol_t *) cp)->upper_layer;
}

gnrc_ppp_target_t _fsm_lower_layer(gnrc_ppp_fsm_t *cp)
{
    return ((gnrc_ppp_protocol_t *) cp)->lower_layer;
}
void tlu(gnrc_ppp_fsm_t *cp, void *args)
{
    (void)args;

    _reset_cp_conf(cp->conf);
    ((gnrc_ppp_protocol_t *) cp)->state = PROTOCOL_UP;
    if (cp->on_layer_up) {
        cp->on_layer_up(cp);
    }
    send_ppp_event(&((gnrc_ppp_protocol_t *)cp)->msg, ppp_msg_set(_fsm_upper_layer(cp), PPP_LINKUP));
    (void) cp;
}

void tld(gnrc_ppp_fsm_t *cp, void *args)
{
    (void)args;
    _reset_cp_conf(cp->conf);
    ((gnrc_ppp_protocol_t *) cp)->state = PROTOCOL_DOWN;
    if (cp->on_layer_down) {
        cp->on_layer_down(cp);
    }
    send_ppp_event(&((gnrc_ppp_protocol_t *) cp)->msg, ppp_msg_set(_fsm_upper_layer(cp), PPP_LINKDOWN));
    (void) cp;
}

void tls(gnrc_ppp_fsm_t *cp, void *args)
{
    (void)args;
    _reset_cp_conf(cp->conf);
    send_ppp_event(&((gnrc_ppp_protocol_t *) cp)->msg, ppp_msg_set(_fsm_lower_layer(cp), PPP_UL_STARTED));
    (void) cp;
}

void tlf(gnrc_ppp_fsm_t *cp, void *args)
{
    (void)args;
    send_ppp_event(&((gnrc_ppp_protocol_t *) cp)->msg, ppp_msg_set(_fsm_lower_layer(cp), PPP_UL_FINISHED));
    (void) cp;
}

void irc(gnrc_ppp_fsm_t *cp, void *args)
{

    uint8_t cr = *((int *) args) & PPP_F_SCR;

    cp->restart_counter = cr ? GNRC_PPP_MAX_CONFIG : GNRC_PPP_MAX_TERMINATE;
}

void zrc(gnrc_ppp_fsm_t *cp, void *args)
{
    (void)args;
    cp->restart_counter = 0;
    set_timeout(cp, cp->restart_timer);
}


void scr(gnrc_ppp_fsm_t *cp, void *args)
{
    (void)args;

    /* Decrement configure counter */
    cp->restart_counter -= 1;

    /* Build options */
    gnrc_pktsnip_t *opts = _build_conf_req_options(cp);

    /*In case there are options, copy to sent opts*/
    if (opts) {
        memcpy(cp->cr_sent_opts, opts->data, opts->size);
        cp->cr_sent_size = opts->size;
    }

    /*Send configure request*/
    send_configure_request(((gnrc_ppp_protocol_t *) cp)->dev, cp->prottype, ++cp->cr_sent_identifier, opts);
    set_timeout(cp, cp->restart_timer);
}

void sca(gnrc_ppp_fsm_t *cp, void *args)
{
    gnrc_pktsnip_t *pkt = (gnrc_pktsnip_t *) args;

    lcp_hdr_t *recv_hdr;

    gnrc_pktsnip_t *opts = NULL;

    recv_hdr = _get_hdr(pkt);

    if (_pkt_has_payload(recv_hdr)) {
        opts = gnrc_pktbuf_add(NULL, pkt->data, pkt->size, GNRC_NETTYPE_UNDEF);
    }

    send_configure_ack(((gnrc_ppp_protocol_t *) cp)->dev, cp->prottype, recv_hdr->id, opts);
}

void scn(gnrc_ppp_fsm_t *cp, void *args)
{
    gnrc_pktsnip_t *pkt = (gnrc_pktsnip_t *) args;

    gnrc_pktsnip_t *opts;

    uint16_t scn_len;
    uint8_t type = get_scnpkt_data(cp, pkt, &scn_len);

    opts = gnrc_pktbuf_add(NULL, NULL, scn_len, GNRC_NETTYPE_UNDEF);

    switch (type) {
        case GNRC_PPP_CONF_NAK:
            build_nak_pkt(cp, pkt, (uint8_t *)opts->data);
            send_configure_nak(((gnrc_ppp_protocol_t *) cp)->dev, cp->prottype,((lcp_hdr_t *) pkt->next->data)->id, opts);
            break;
        case GNRC_PPP_CONF_REJ:
            build_rej_pkt(cp, pkt, (uint8_t *) opts->data);
            send_configure_rej(((gnrc_ppp_protocol_t *) cp)->dev, cp->prottype,((lcp_hdr_t *) pkt->next->data)->id, opts);
            break;
        default:
            DEBUG(MODULE"shouldn't be here...\n");
            break;
    }
}

void str(gnrc_ppp_fsm_t *cp, void *args)
{
    (void)args;
    send_terminate_req(((gnrc_ppp_protocol_t *) cp)->dev, cp->prottype, cp->tr_sent_identifier++);
}

void sta(gnrc_ppp_fsm_t *cp, void *args)
{
    gnrc_pktsnip_t *pkt = (gnrc_pktsnip_t *) args;
    gnrc_pktsnip_t *recv_pkt = NULL;

    lcp_hdr_t *recv_hdr = _get_hdr(pkt);

    if (_pkt_has_payload(recv_hdr)) {
        recv_pkt = gnrc_pktbuf_add(NULL, pkt->data, pkt->size, GNRC_NETTYPE_UNDEF);
    }
    send_terminate_ack(((gnrc_ppp_protocol_t *) cp)->dev, cp->prottype, recv_hdr->id, recv_pkt);
}
void scj(gnrc_ppp_fsm_t *cp, void *args)
{
    gnrc_pktsnip_t *pkt = (gnrc_pktsnip_t *) args;

    gnrc_pktsnip_t *payload = gnrc_pktbuf_add(NULL, pkt->data, pkt->size, cp->prottype);

    send_code_rej(((gnrc_ppp_protocol_t *) cp)->dev, cp->prottype, cp->cr_sent_identifier++, payload);
}
void ser(gnrc_ppp_fsm_t *cp, void *args)
{
    gnrc_pktsnip_t *pkt = (gnrc_pktsnip_t *) args;
    gnrc_pktsnip_t *ppp_hdr = gnrc_pktbuf_mark(pkt, sizeof(lcp_hdr_t), cp->prottype);
    lcp_hdr_t *hdr = ppp_hdr->data;
    uint8_t id = hdr->id;

    uint8_t code = hdr->code;
    gnrc_pktsnip_t *data = NULL;

    if (pkt != ppp_hdr) {
        data = gnrc_pktbuf_add(NULL, pkt->data, pkt->size, GNRC_NETTYPE_UNDEF);
    }

    switch (code) {
        case GNRC_PPP_ECHO_REQ:
            send_echo_reply(((gnrc_ppp_protocol_t *) cp)->dev, cp->prottype, id, data);
            break;
        case GNRC_PPP_ECHO_REP:
            break;
        case GNRC_PPP_DISC_REQ:
            break;
    }

    /*Send PPP_LINK_ALIVE to lower layer*/
    send_ppp_event(&((gnrc_ppp_protocol_t *) cp)->msg, ppp_msg_set(_fsm_lower_layer(cp), PPP_LINK_ALIVE));
}

/* Call functions depending on function flag*/
static void _event_action(gnrc_ppp_fsm_t *cp, uint8_t event, gnrc_pktsnip_t *pkt)
{
    int flags;

    flags = actions[event][cp->state];

    if (flags & PPP_F_TLU) {
        tlu(cp, NULL);
    }
    if (flags & PPP_F_TLD) {
        tld(cp, NULL);
    }
    if (flags & PPP_F_TLS) {
        tls(cp, NULL);
    }
    if (flags & PPP_F_TLF) {
        tlf(cp, NULL);
    }
    if (flags & PPP_F_IRC) {
        irc(cp, (void *) &flags);
    }
    if (flags & PPP_F_ZRC) {
        zrc(cp, NULL);
    }
    if (flags & PPP_F_SCR) {
        scr(cp, (void *) pkt);
    }
    if (flags & PPP_F_SCA) {
        sca(cp, (void *) pkt);
    }
    if (flags & PPP_F_SCN) {
        scn(cp, (void *) pkt);
    }
    if (flags & PPP_F_STR) {
        str(cp, NULL);
    }
    if (flags & PPP_F_STA) {
        sta(cp, (void *) pkt);
    }
    if (flags & PPP_F_SCJ) {
        scj(cp, (void *) pkt);
    }
    if (flags & PPP_F_SER) {
        ser(cp, (void *) pkt);
    }
}

int trigger_fsm_event(gnrc_ppp_fsm_t *cp, int event, gnrc_pktsnip_t *pkt)
{
    if (event < 0) {
        return -EBADMSG;
    }
    int next_state;
    next_state = state_trans[event][cp->state];
#if ENABLE_DEBUG
    print_transition(((gnrc_ppp_protocol_t *)cp)->id, cp->state, event, next_state);
#endif

    /* Keep in same state if there's something wrong (RFC 1661) */
    if (next_state != PPP_S_UNDEF) {
        _event_action(cp, event, pkt);
        cp->state = next_state;
    }
    else {
        DEBUG(MODULE"received illegal transition. \n");
    }
    /*Check if next state doesn't have a running timer*/
    if (cp->state < PPP_S_CLOSING || cp->state == PPP_S_OPENED) {
        xtimer_remove(&cp->xtimer);
    }
    return 0;
}

int fsm_init(gnrc_ppp_fsm_t *cp)
{
    cp->state = PPP_S_INITIAL;
    cp->cr_sent_identifier = 0;
    return 0;
}

static int _opt_is_ack(gnrc_ppp_fsm_t *cp, gnrc_ppp_option_t *opt)
{
    gnrc_ppp_fsm_conf_t *curr_conf = NULL;

    curr_conf = cp->get_conf_by_code(cp, ppp_opt_get_type(opt));

    return curr_conf && curr_conf->is_valid(opt);
}

int handle_rcr(gnrc_ppp_fsm_t *cp, gnrc_pktsnip_t *pkt)
{
    /* This packet doesn't have options, it's considered as valid. */
    if (!pkt) {
        return PPP_E_RCRp;
    }

    /* Check if options in pkt are valid */
    if (ppp_conf_opts_valid(pkt, pkt->size) <= 0) {
        return -EBADMSG;
    }
    //                                      opt 2 accm          opt 3 mru   opt 7   opt 8
    //hdlc: [7e ff  3 [c0 21 [1  1  0 12  [2  6  0  a  0  0 ] [3  4 c0 23]  [7  2]  [8  2]]] 55 83 7e]

    /* loop through through the received option list */
    FOR_EACH_OPTION(opt, pkt->data, pkt->size){
        /* check if setting is within our list */
        if (!_opt_is_ack(cp, opt)) {
            return PPP_E_RCRm;
        }
    }

    /* check if there's an option that is required but not sent */
    uint8_t found;
    FOR_EACH_CONF(conf, cp->conf){
        if (!(conf->flags & GNRC_PPP_OPT_REQUIRED)) {
            continue;
        }
        found = false;
        FOR_EACH_OPTION(opt, pkt->data, pkt->size){
            if (conf->type == ppp_opt_get_type(opt)) {
                found = true;
                break;
            }
        }

        if (!found) {
            return PPP_E_RCRm;
        }
    }

    /* Valid options... set them before SCA */
    gnrc_ppp_fsm_conf_t *curr_conf;
    FOR_EACH_OPTION(opt, pkt->data, pkt->size){
        curr_conf = cp->get_conf_by_code(cp, ppp_opt_get_type(opt));
        if (curr_conf) {
            if(curr_conf->set) {
                curr_conf->set(cp, opt, true);
            }
        }
        else {
            DEBUG(MODULE"handle_rcr inconsistency in pkt. \n");
        }
    }

    return PPP_E_RCRp;
}

int handle_rca(gnrc_ppp_fsm_t *cp, lcp_hdr_t *hdr, gnrc_pktsnip_t *pkt)
{
    uint8_t pkt_id = hdr->id;
    uint8_t pkt_length = byteorder_ntohs(hdr->length);

    void *opts = NULL;

    if (pkt) {
        if (ppp_conf_opts_valid(pkt, pkt->size) <= 0) {
            return -EBADMSG;
        }
        opts = pkt->data;
    }

    if (pkt_id != cp->cr_sent_identifier || (pkt && memcmp(cp->cr_sent_opts, opts, pkt_length - sizeof(lcp_hdr_t)))) {
        return -EBADMSG;
    }

    /*Write options in corresponding devices*/
    if (pkt) {
        gnrc_ppp_fsm_conf_t *conf;
        FOR_EACH_OPTION(opt, opts, pkt->size){
            conf = cp->get_conf_by_code(cp, ppp_opt_get_type(opt));
            if (!conf) {
                /*Received invalid ACK*/
                DEBUG(MODULE"peer sent inconsistent ACK\n");
                return -EBADMSG;
            }
            if(conf->set) {
                conf->set(cp, opt, false);
            }
        }
    }
    return PPP_E_RCA;
}

int handle_rcn_nak(gnrc_ppp_fsm_t *cp, lcp_hdr_t *hdr, gnrc_pktsnip_t *pkt)
{
    if (!pkt) {
        /* If the packet doesn't have options, it's considered as invalid. */
        DEBUG(MODULE"received NAK packet without options. Discard\n");
        return -EBADMSG;
    }

    /* Check if options are valid */
    if (ppp_conf_opts_valid(pkt, pkt->size) <= 0) {
        DEBUG(MODULE"received NAK pkt with invalid options. Discard\n");
        return -EBADMSG;
    }


    if (hdr->id != cp->cr_sent_identifier) {
        DEBUG(MODULE"id mismatch in NAK packet\n");
        return -EBADMSG;
    }

    /*Handle nak for each option*/
    gnrc_ppp_fsm_conf_t *curr_conf;
    network_uint32_t value;
    uint8_t *payload;

    FOR_EACH_OPTION(opt, pkt->data, pkt->size){
        curr_conf = cp->get_conf_by_code(cp, ppp_opt_get_type(opt));
        if (curr_conf != NULL) {
            if (!(curr_conf->flags & GNRC_PPP_OPT_ENABLED)) {
                curr_conf->flags |= GNRC_PPP_OPT_ENABLED;
            }
            else if (curr_conf->is_valid(opt)) {
                value = byteorder_htonl(0);
                ppp_opt_get_payload(opt, (void **) &payload);
                memcpy(&value + (4 - curr_conf->size), payload, curr_conf->size);
                curr_conf->value = value;
            }
            else {
                curr_conf->flags &= ~GNRC_PPP_OPT_ENABLED;
            }
        }
    }
    return PPP_E_RCN;
}

int handle_rcn_rej(gnrc_ppp_fsm_t *cp, lcp_hdr_t *hdr, gnrc_pktsnip_t *pkt)
{
    if (!pkt || hdr->id != cp->cr_sent_identifier || ppp_conf_opts_valid(pkt, pkt->size) <= 0 || byteorder_ntohs(hdr->length) - sizeof(lcp_hdr_t) != cp->cr_sent_size) {
        return -EBADMSG;
    }


    uint16_t size = cp->cr_sent_size;

    FOR_EACH_OPTION(opt, pkt->data, pkt->size){
        if (!ppp_opt_is_subset(opt, cp->cr_sent_opts, size)) {
            return -EBADMSG;
        }
    }

    /* Disable every REJ option */
    gnrc_ppp_fsm_conf_t *curr_conf;
    FOR_EACH_OPTION(opt, pkt->data, pkt->size){
        curr_conf = cp->get_conf_by_code(cp, ppp_opt_get_type(opt));
        if (curr_conf == NULL) {
            DEBUG(MODULE"shouldn't be here\n");
            return -EBADMSG;
        }
        curr_conf->flags &= ~GNRC_PPP_OPT_ENABLED;
    }
    return PPP_E_RCN;
}

int handle_coderej(lcp_hdr_t *hdr, gnrc_pktsnip_t *pkt)
{
    (void)pkt;
    if (hdr->code >= GNRC_PPP_CONF_REQ && hdr->code <= GNRC_PPP_TERM_ACK) {
        return PPP_E_RXJm;
    }
    else {
        return PPP_E_RXJp;
    }

}


int handle_term_ack(gnrc_ppp_fsm_t *cp, gnrc_pktsnip_t *pkt)
{
    lcp_hdr_t *hdr = pkt->data;

    int id = hdr->id;

    if (id == cp->tr_sent_identifier) {
        return PPP_E_RTA;
    }
    return -EBADMSG;
}


static int handle_conf_pkt(gnrc_ppp_fsm_t *cp, int type, gnrc_pktsnip_t *pkt)
{
    lcp_hdr_t *hdr = (lcp_hdr_t *) pkt->next->data;

    int event;

    switch (type) {
        case GNRC_PPP_CONF_REQ:
            event = handle_rcr(cp, pkt);
            break;
        case GNRC_PPP_CONF_ACK:
            event = handle_rca(cp, hdr, pkt);
            break;
        case GNRC_PPP_CONF_NAK:
            event = handle_rcn_nak(cp, hdr, pkt);
            break;
        case GNRC_PPP_CONF_REJ:
            event = handle_rcn_rej(cp, hdr, pkt);
            break;
        default:
            DEBUG("Shouldn't be here...\n");
            return -EBADMSG;
            break;
    }
    return event;
}

int fsm_event_from_pkt(gnrc_ppp_fsm_t *cp, gnrc_pktsnip_t *pkt)
{
    lcp_hdr_t *hdr = (lcp_hdr_t *) pkt->next->data;

    int code = hdr->code;
    int supported = cp->supported_codes & (1 << (code - 1));
    int type = supported ? code : GNRC_PPP_UNKNOWN_CODE;

    int event;

    switch (type) {
        case GNRC_PPP_CONF_REQ:
        case GNRC_PPP_CONF_ACK:
        case GNRC_PPP_CONF_NAK:
        case GNRC_PPP_CONF_REJ:
            event = handle_conf_pkt(cp, type, pkt);
            break;
        case GNRC_PPP_TERM_REQ:
            event = PPP_E_RTR;
            break;
        case GNRC_PPP_TERM_ACK:
            event = handle_term_ack(cp, pkt);
            break;
        case GNRC_PPP_CODE_REJ:
            event = handle_coderej(hdr, pkt);
            break;
        case GNRC_PPP_ECHO_REQ:
        case GNRC_PPP_ECHO_REP:
        case GNRC_PPP_DISC_REQ:
            event = PPP_E_RXR;
            break;
        default:
            event = PPP_E_RUC;
            break;
    }

    return event;
}

int fsm_handle_ppp_msg(gnrc_ppp_protocol_t *protocol, uint8_t ppp_event, void *args)
{
    gnrc_ppp_fsm_t *target = (gnrc_ppp_fsm_t *) protocol;
    int event;
    gnrc_pktsnip_t *pkt = ((gnrc_pktsnip_t *)args);

    switch (ppp_event) {
        case PPP_RECV:
            event = fsm_event_from_pkt(target, pkt);
            if (event > 0) {
                trigger_fsm_event(target, event, pkt);
            }
            return event < 0 ? event : 0;
            break;
        case PPP_LINKUP:
            protocol->state = PROTOCOL_STARTING;
            trigger_fsm_event(target, PPP_E_UP, NULL);
            break;
        case PPP_LINKDOWN:
            trigger_fsm_event(target, PPP_E_DOWN, NULL);
            break;
        case PPP_UL_STARTED:
            if (target->state == PPP_S_OPENED) {
                send_ppp_event(&protocol->msg, ppp_msg_set(_fsm_upper_layer(target), PPP_LINKUP));
            }
            break;
        case PPP_TIMEOUT:
            if (target->restart_counter) {
                trigger_fsm_event(target, PPP_E_TOp, NULL);
            }
            else {
                trigger_fsm_event(target, PPP_E_TOm, NULL);
            }
            break;
    }
    return 0;
}
