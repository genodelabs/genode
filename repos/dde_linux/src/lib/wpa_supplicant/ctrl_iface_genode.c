/*
 * \brief  WPA Supplicant frontend
 * \author Josef Soentgen
 * \date   2018-07-18
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/*
 * based on:
 *
 * WPA Supplicant / UNIX domain socket -based control interface
 * Copyright (c) 2004-2014, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

/* wpa_supplicant includes */
#include "includes.h"
#include "utils/common.h"
#include "utils/eloop.h"
#include "utils/list.h"
#include "common/ctrl_iface_common.h"
#include "eapol_supp/eapol_supp_sm.h"
#include "config.h"
#include "wpa_supplicant_i.h"
#include "ctrl_iface.h"

/* rep includes */
#include <wifi/ctrl.h>


struct ctrl_iface_priv {
	struct wpa_supplicant *wpa_s;
	int fd;
	int level;

	/* TODO replace w/ Msg_buffer */
	char     *send_buffer;
	size_t    send_buffer_size;
	unsigned *send_id;

	char     *recv_buffer;
	size_t    recv_buffer_size;
	unsigned *recv_id;

	unsigned last_recv_id;

	char     *event_buffer;
	size_t    event_buffer_size;
	unsigned *event_id;
};


struct ctrl_iface_global_priv {
	struct wpa_global *global;
};


extern void lx_printf(char const *, ...) __attribute__((format(printf, 1, 2)));
extern void nl_set_wpa_ctrl_fd(void);


void wpa_ctrl_set_fd()
{
	nl_set_wpa_ctrl_fd();
}


static void send_reply(struct ctrl_iface_priv *priv, char const *txt, size_t len)
{
	char   *msg  = priv->send_buffer;
	size_t  mlen = priv->send_buffer_size;

	if (len >= mlen) {
		lx_printf("Warning: cmd reply will be truncated\n");
		len = mlen - 1;
	}

	memcpy(msg, txt, len);
	msg[len] = 0;
	(*priv->send_id)++;
}


/*
 * This function is called by wpa_supplicant whenever it receives a
 * command via the CTRL interface, i.e. the front end has sent a new
 * message.
 */
static void wpa_supplicant_ctrl_iface_receive(int fd, void *eloop_ctx,
                                              void *sock_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	struct ctrl_iface_priv *priv = sock_ctx;

	char *msg = priv->recv_buffer;

	unsigned const recv_id = *priv->recv_id;

	char *reply      = NULL;
	size_t reply_len = 0;

	if (msg[0] == 0 || recv_id == priv->last_recv_id) { return; }

	priv->last_recv_id = recv_id;

	reply = wpa_supplicant_ctrl_iface_process(wpa_s, msg,
	                                          &reply_len);

	if (reply) {
		wifi_block_for_processing();
		send_reply(priv, reply, reply_len);
		wifi_notify_cmd_result();
		os_free(reply);
	} else

	if (reply_len == 1) {
		wifi_block_for_processing();
		send_reply(priv, "FAIL", 4);
		wifi_notify_cmd_result();
	} else

	if (reply_len == 2) {
		wifi_block_for_processing();
		send_reply(priv, "OK", 2);
		wifi_notify_cmd_result();
	}
}


static void print_txt(char const *txt, size_t len)
{
	char buffer[256];
	memset(buffer, 0, sizeof(buffer));
	memcpy(buffer, txt, len < sizeof(buffer) - 1 ? len : sizeof(buffer) - 1);
	lx_printf("   %s\n", buffer);
}


static void send_event(struct ctrl_iface_priv *priv, char const *txt, size_t len)
{
	char   *msg  = priv->event_buffer;
	size_t  mlen = priv->event_buffer_size;

	if (len >= mlen) {
		lx_printf("Warning: event will be truncated\n");
		len = mlen - 1;
	}

	memcpy(msg, txt, len);
	msg[len] = 0;
	(*priv->event_id)++;
}


/*
 * This function is called by wpa_supplicant whenever it wants to
 * forward some message. We filter these messages and forward only
 * those, which are of interest to the front end.
 */
static void wpa_supplicant_ctrl_iface_msg_cb(void *ctx, int level,
                                             enum wpa_msg_type type,
                                             const char *txt, size_t len)
{
#if 0
	int const dont_print =
		   strncmp(txt, "BSS:", 4) == 0
		|| strncmp(txt, "BSS:", 4) == 0
		|| strncmp(txt, "CTRL-EVENT-BSS", 14) == 0
		|| strncmp(txt, "   skip", 7) == 0
	;
	if (!dont_print) { print_txt(txt, len); }
#endif

	/* there is not global support */
	if (type == WPA_MSG_ONLY_GLOBAL) { return; }

	struct wpa_supplicant *wpa_s = ctx;
	if (wpa_s == NULL) { return; }

	struct ctrl_iface_priv *priv = wpa_s->ctrl_iface;
	if (!priv || level < priv->level) { return; }

	/*
	 * Filter messages and only forward events the front end cares
	 * about or rather knows how to handle.
	 */
	int const forward =
		   strncmp(txt, "CTRL-EVENT-SCAN-RESULTS", 23) == 0
		|| strncmp(txt, "CTRL-EVENT-CONNECTED",    20) == 0
		|| strncmp(txt, "CTRL-EVENT-DISCONNECTED", 23) == 0
		|| strncmp(txt, "CTRL-EVENT-NETWORK-NOT-FOUND", 28) == 0
		/* needed to detect connecting state */
		|| strncmp(txt, "SME: Trying to authenticate", 27) == 0
	;
	if (!forward) { return; }

	wifi_notify_event();
	send_event(priv, txt, len);
}


struct ctrl_iface_priv *
wpa_supplicant_ctrl_iface_init(struct wpa_supplicant *wpa_s)
{
	struct ctrl_iface_priv *priv;

	priv = os_zalloc(sizeof(*priv));
	if (priv == NULL) { return NULL; }

	if (wpa_s->conf->ctrl_interface == NULL) {
		return priv;
	}

	struct Msg_buffer *msg_buffer = (struct Msg_buffer*)wifi_get_buffer();
	priv->recv_buffer      = (char *)msg_buffer->send;
	priv->recv_buffer_size = sizeof(msg_buffer->send);
	priv->send_buffer      = (char *)msg_buffer->recv;
	priv->send_buffer_size = sizeof(msg_buffer->recv);
	priv->send_id          = &msg_buffer->recv_id;
	priv->recv_id          = &msg_buffer->send_id;

	priv->event_buffer      = (char *)msg_buffer->event;
	priv->event_buffer_size = sizeof(msg_buffer->event);
	priv->event_id          = &msg_buffer->event_id;

	priv->level = MSG_INFO;
	priv->fd    = WPA_CTRL_FD;

	eloop_register_read_sock(priv->fd,
	                         wpa_supplicant_ctrl_iface_receive,
	                         wpa_s, priv);

	wpa_msg_register_cb(wpa_supplicant_ctrl_iface_msg_cb);
	return priv;
}


void wpa_supplicant_ctrl_iface_deinit(struct ctrl_iface_priv *priv)
{
	os_free(priv);
}


void wpa_supplicant_ctrl_iface_wait(struct ctrl_iface_priv *priv) { }


struct ctrl_iface_global_priv *
wpa_supplicant_global_ctrl_iface_init(struct wpa_global *global)
{
	struct ctrl_iface_global_priv *priv;

	priv = os_zalloc(sizeof(*priv));
	return priv;
}


void wpa_supplicant_global_ctrl_iface_deinit(struct ctrl_iface_global_priv *p)
{
	os_free(p);
}
