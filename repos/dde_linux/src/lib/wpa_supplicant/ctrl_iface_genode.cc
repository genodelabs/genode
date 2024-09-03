/*
 * \brief  Genode-specific WPA supplicant ctrl_iface
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
extern "C" {
#include "includes.h"
#include "utils/common.h"
#include "utils/eloop.h"
#include "utils/list.h"
#include "common/ctrl_iface_common.h"
#include "eapol_supp/eapol_supp_sm.h"
#include "config.h"
#include "wpa_supplicant_i.h"
#include "ctrl_iface.h"
}

/* rep includes */
#include <wifi/ctrl.h>


static Wifi::Msg_buffer *_msg_buffer;


void Wifi::ctrl_init(Msg_buffer &buffer)
{
	_msg_buffer = &buffer;
}


struct ctrl_iface_priv {
	struct wpa_supplicant *wpa_s;
	int fd;
	int level;

	Wifi::Msg_buffer *buffer;
	unsigned last_send_id;
};


struct ctrl_iface_global_priv {
	struct wpa_global *global;
};


extern "C"
void nl_set_wpa_ctrl_fd(void);


void wpa_ctrl_set_fd()
{
	nl_set_wpa_ctrl_fd();
}


static void send_reply(Wifi::Msg_buffer &buffer, char const *txt, size_t len)
{
	buffer.block_for_processing();

	/* XXX hack to trick poll() into returning faster */
	wpa_ctrl_set_fd();

	if (len >= sizeof(buffer.recv))
		len = sizeof(buffer.recv) - 1;

	memcpy(buffer.recv, txt, len);
	buffer.recv[len] = 0;
	buffer.recv_id++;

	buffer.notify_response();
}


/*
 * This function is called by wpa_supplicant whenever it receives a
 * command via the CTRL interface, i.e. the manager has sent a new
 * message.
 */
static void wpa_supplicant_ctrl_iface_receive(int fd, void *eloop_ctx,
                                              void *sock_ctx)
{
	struct wpa_supplicant *wpa_s = static_cast<wpa_supplicant*>(eloop_ctx);
	struct ctrl_iface_priv *priv = static_cast<ctrl_iface_priv*>(sock_ctx);

	Wifi::Msg_buffer &buffer = *priv->buffer;

	char   *reply     = NULL;
	size_t  reply_len = 0;

	if (buffer.send[0] == 0 || buffer.send_id == priv->last_send_id)
		return;

	priv->last_send_id = buffer.send_id;

	reply = wpa_supplicant_ctrl_iface_process(wpa_s,
	                                          buffer.send,
	                                          &reply_len);

	if (reply) {
		send_reply(buffer, reply, reply_len);
		os_free(reply);
	} else

	if (reply_len == 1) {
		send_reply(buffer, "FAIL", 4);
	} else

	if (reply_len == 2) {
		send_reply(buffer, "OK", 2);
	}
}


static void send_event(Wifi::Msg_buffer &buffer, char const *txt, size_t len)
{
	buffer.block_for_processing();

	/* XXX hack to trick poll() into returning faster */
	wpa_ctrl_set_fd();

	if (len >= sizeof(buffer.event))
		len = sizeof(buffer.event) - 1;

	memcpy(buffer.event, txt, len);
	buffer.event[len] = 0;
	buffer.event_id++;

	buffer.notify_event();
}


/*
 * This function is called by wpa_supplicant whenever it wants to
 * forward some message. We filter these messages and forward only
 * those, which are of interest to the Wifi manager.
 */
static void wpa_supplicant_ctrl_iface_msg_cb(void *ctx, int level,
                                             enum wpa_msg_type type,
                                             const char *txt, size_t len)
{
	/* there is not global support */
	if (type == WPA_MSG_ONLY_GLOBAL)
		return;

	struct wpa_supplicant *wpa_s = static_cast<wpa_supplicant*>(ctx);
	if (wpa_s == NULL)
		return;

	struct ctrl_iface_priv *priv = wpa_s->ctrl_iface;
	if (!priv || level < priv->level)
		return;

	/*
	 * Filter messages and only forward events the manager cares
	 * about or rather knows how to handle.
	 */
	bool const forward =
	     strncmp(txt, "CTRL-EVENT-SCAN-RESULTS", 23) == 0
	  || strncmp(txt, "CTRL-EVENT-CONNECTED",    20) == 0
	  || strncmp(txt, "CTRL-EVENT-DISCONNECTED", 23) == 0
	  || strncmp(txt, "CTRL-EVENT-NETWORK-NOT-FOUND", 28) == 0
	  /* needed to detect connecting state */
	  || strncmp(txt, "SME: Trying to authenticate", 27) == 0;
	if (!forward)
		return;

	Wifi::Msg_buffer &buffer = *priv->buffer;
	send_event(buffer, txt, len);
}


extern "C" struct ctrl_iface_priv *
wpa_supplicant_ctrl_iface_init(struct wpa_supplicant *wpa_s)
{
	struct ctrl_iface_priv *priv = (ctrl_iface_priv*)os_zalloc(sizeof(*priv));
	if (priv == NULL) { return NULL; }

	if (wpa_s->conf->ctrl_interface == NULL) {
		return priv;
	}

	priv->buffer = _msg_buffer;
	priv->level  = MSG_INFO;
	priv->fd     = Wifi::CTRL_FD;

	eloop_register_read_sock(priv->fd,
	                         wpa_supplicant_ctrl_iface_receive,
	                         wpa_s, priv);

	wpa_msg_register_cb(wpa_supplicant_ctrl_iface_msg_cb);
	return priv;
}

extern "C"
void wpa_supplicant_ctrl_iface_deinit(struct wpa_supplicant *wpa_s,
                                      struct ctrl_iface_priv *priv)
{
	(void)wpa_s;

	os_free(priv);
}


extern "C"
void wpa_supplicant_ctrl_iface_wait(struct ctrl_iface_priv *priv) { }


extern "C" struct ctrl_iface_global_priv *
wpa_supplicant_global_ctrl_iface_init(struct wpa_global *global)
{
	struct ctrl_iface_global_priv *priv =
		(ctrl_iface_global_priv*)os_zalloc(sizeof(*priv));
	return priv;
}


extern "C"
void wpa_supplicant_global_ctrl_iface_deinit(struct ctrl_iface_global_priv *p)
{
	os_free(p);
}
