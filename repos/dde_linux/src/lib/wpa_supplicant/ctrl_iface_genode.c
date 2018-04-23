/*
 * WPA Supplicant / UNIX domain socket -based control interface
 * Copyright (c) 2004-2014, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "utils/common.h"
#include "utils/eloop.h"
#include "utils/list.h"
#include "common/ctrl_iface_common.h"
#include "eapol_supp/eapol_supp_sm.h"
#include "config.h"
#include "wpa_supplicant_i.h"
#include "ctrl_iface.h"

struct ctrl_iface_priv {
	struct wpa_supplicant *wpa_s;
};


struct ctrl_iface_global_priv {
	struct wpa_global *global;
};

struct ctrl_iface_msg {
	struct wpa_supplicant *wpa_s;
};


struct ctrl_iface_priv *
wpa_supplicant_ctrl_iface_init(struct wpa_supplicant *wpa_s)
{
	printf("%s:%d\n", __func__, __LINE__);

	struct ctrl_iface_priv *priv;

	priv = os_zalloc(sizeof(*priv));
	if (priv == NULL)
		return NULL;

	if (wpa_s->conf->ctrl_interface == NULL)
		return priv;

	return priv;
}


void wpa_supplicant_ctrl_iface_deinit(struct ctrl_iface_priv *priv)
{
	printf("%s:%d\n", __func__, __LINE__);

	struct wpa_ctrl_dst *dst, *prev;
	struct ctrl_iface_msg *msg, *prev_msg;
	struct ctrl_iface_global_priv *gpriv;

	os_free(priv);
}


void wpa_supplicant_ctrl_iface_wait(struct ctrl_iface_priv *priv)
{
	printf("%s:%d\n", __func__, __LINE__);
}


struct ctrl_iface_global_priv *
wpa_supplicant_global_ctrl_iface_init(struct wpa_global *global)
{
	printf("%s:%d\n", __func__, __LINE__);

	struct ctrl_iface_global_priv *priv;

	priv = os_zalloc(sizeof(*priv));
	if (priv == NULL)
		return NULL;

	if (global->params.ctrl_interface == NULL)
		return priv;

	return priv;
}


void wpa_supplicant_global_ctrl_iface_deinit(struct ctrl_iface_global_priv *priv)
{
	printf("%s:%d\n", __func__, __LINE__);

	struct wpa_ctrl_dst *dst, *prev;
	struct ctrl_iface_msg *msg, *prev_msg;

	os_free(priv);
}
