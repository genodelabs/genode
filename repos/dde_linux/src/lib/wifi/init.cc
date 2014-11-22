/*
 * \brief  Linux wireless stack
 * \author Josef Soentgen
 * \date   2014-03-03
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <os/config.h>
#include <os/server.h>

/* local includes */
#include <firmware_list.h>
#include <lx.h>

#include <extern_c_begin.h>
# include <lx_emul.h>
#include <extern_c_end.h>

extern "C" void core_netlink_proto_init(void);
extern "C" void core_sock_init(void);
extern "C" void module_packet_init(void);
extern "C" void subsys_genl_init(void);
extern "C" void subsys_rfkill_init(void);
extern "C" void subsys_cfg80211_init(void);
extern "C" void subsys_ieee80211_init(void);
extern "C" void module_iwl_drv_init(void);
extern "C" void subsys_cryptomgr_init(void);
extern "C" void module_crypto_ccm_module_init(void);
extern "C" void module_crypto_ctr_module_init(void);
extern "C" void module_aes_init(void);
extern "C" void module_arc4_init(void);
extern "C" void module_chainiv_module_init(void);
extern "C" void module_krng_mod_init(void);

static bool mac80211_only = false;

struct pernet_operations loopback_net_ops;

struct net init_net;
LIST_HEAD(net_namespace_list);


Firmware_list fw_list[] = {
	{ "iwlwifi-1000-3.ucode", 335056 },
	{ "iwlwifi-1000-5.ucode", 337520 },
	{ "iwlwifi-105-6.ucode", 689680 },
	{ "iwlwifi-135-6.ucode", 701228 },
	{ "iwlwifi-2000-6.ucode", 695876 },
	{ "iwlwifi-2030-6.ucode", 707392 },
	{ "iwlwifi-3160-7.ucode", 670484 },
	{ "iwlwifi-3160-8.ucode", 667284 },
	{ "iwlwifi-3160-9.ucode", 666792 },
	{ "iwlwifi-3945-2.ucode", 150100 },
	{ "iwlwifi-4965-2.ucode", 187972 },
	{ "iwlwifi-5000-1.ucode", 345008 },
	{ "iwlwifi-5000-2.ucode", 353240 },
	{ "iwlwifi-5000-5.ucode", 340696 },
	{ "iwlwifi-5150-2.ucode", 337400 },
	{ "iwlwifi-6000-4.ucode", 454608 },
	/**
	 * Actually, there is no -6 firmware. The last one is revision 4,
	 * but certain devices support up to revision 6 and want to use
	 * this one. Our fw loading mechanism sadly will not work in this
	 * case, therefore we add -6 to the fw whitelist have to provide
	 * a renamed image.
	 */
	{ "iwlwifi-6000-6.ucode", 454608 }, /* XXX same as -4 */
	{ "iwlwifi-6000g2a-5.ucode", 444128 },
	{ "iwlwifi-6000g2a-6.ucode", 677296 },
	{ "iwlwifi-6000g2b-5.ucode", 460236 },
	{ "iwlwifi-6000g2b-6.ucode", 679436 },
	{ "iwlwifi-6050-4.ucode", 463692 },
	{ "iwlwifi-6050-5.ucode", 469780 },
	{ "iwlwifi-7260-7.ucode", 683236 },
	{ "iwlwifi-7260-8.ucode", 679780 },
	{ "iwlwifi-7260-9.ucode", 679380 },
	{ "iwlwifi-7265-8.ucode", 690452 }
};


size_t fw_list_len = sizeof(fw_list) / sizeof(fw_list[0]);


static Genode::Lock *_wpa_lock;


static void run_linux(void *)
{
	core_sock_init();
	core_netlink_proto_init();
	module_packet_init();
	subsys_genl_init();
	subsys_rfkill_init();
	subsys_cfg80211_init();
	subsys_ieee80211_init();

	subsys_cryptomgr_init();
	module_crypto_ccm_module_init();
	module_crypto_ctr_module_init();
	module_aes_init();
	module_arc4_init();
	module_chainiv_module_init();
	module_krng_mod_init();

	if (!mac80211_only) {
		module_iwl_drv_init();
	}

	PINF("+-----------------------+");
	PINF("|   iwl driver loaded   |");
	PINF("+-----------------------+");

	_wpa_lock->unlock();

	while (1) {
		Lx::scheduler().current()->block_and_schedule();
	}
}


void wifi_init(Server::Entrypoint &ep, Genode::Lock &lock)
{
	/**
	 * For testing testing only the wireless stack with wpa_supplicant,
	 * amongst other on linux, we do not want to load the iwlwifi drivers.
	 */
	if (Genode::config()->xml_node().has_attribute("mac80211_only"))
		mac80211_only = Genode::config()->xml_node().attribute("mac80211_only").has_value("yes");
	if (mac80211_only)
		PINF("Initalizing only mac80211 stack without any driver!");

	_wpa_lock = &lock;

	INIT_LIST_HEAD(&init_net.dev_base_head);
	/* add init_net namespace to namespace list */
	list_add_tail_rcu(&init_net.list, &net_namespace_list);

	Lx::timer_init(ep);
	Lx::irq_init(ep);
	Lx::work_init(ep);
	Lx::socket_init(ep);
	Lx::nic_init(ep);

	/* Linux task (handles the initialization only currently) */
	static Lx::Task linux(run_linux, nullptr, "linux",
	                      Lx::Task::PRIORITY_0, Lx::scheduler());

	/* give all task a first kick before returning */
	Lx::scheduler().schedule();
}
