/*
 * \brief  Linux wireless stack
 * \author Josef Soentgen
 * \date   2014-03-03
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <base/lock.h>

/* local includes */
#include <firmware_list.h>
#include <lx.h>

#include <lx_emul.h>

#include <lx_kit/malloc.h>
#include <lx_kit/env.h>
#include <lx_kit/irq.h>
#include <lx_kit/work.h>
#include <lx_kit/timer.h>
#include <lx_kit/pci.h>


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

extern "C" unsigned int *module_param_11n_disable;

struct workqueue_struct *system_power_efficient_wq;
struct workqueue_struct *system_wq;

struct pernet_operations loopback_net_ops;

struct net init_net;
LIST_HEAD(net_namespace_list);


Firmware_list fw_list[] = {
	{ "iwlwifi-1000-3.ucode", 335056, nullptr },
	{ "iwlwifi-1000-5.ucode", 337520, nullptr },
	{ "iwlwifi-105-6.ucode",  689680, nullptr },
	{ "iwlwifi-135-6.ucode",  701228, nullptr },
	{ "iwlwifi-2000-6.ucode", 695876, nullptr },
	{ "iwlwifi-2030-6.ucode", 707392, nullptr },
	{ "iwlwifi-3160-7.ucode", 670484, nullptr },
	{ "iwlwifi-3160-8.ucode", 667284, nullptr },
	{ "iwlwifi-3160-9.ucode", 666792, nullptr },
	{ "iwlwifi-3945-2.ucode", 150100, nullptr },
	{ "iwlwifi-4965-2.ucode", 187972, nullptr },
	{ "iwlwifi-5000-1.ucode", 345008, nullptr },
	{ "iwlwifi-5000-2.ucode", 353240, nullptr },
	{ "iwlwifi-5000-5.ucode", 340696, nullptr },
	{ "iwlwifi-5150-2.ucode", 337400, nullptr },
	{ "iwlwifi-6000-4.ucode", 454608, nullptr },
	/**
	 * Actually, there is no -6 firmware. The last one is revision 4,
	 * but certain devices support up to revision 6 and want to use
	 * this one. To make things simple we refer to the available
	 * firmware under the requested name.
	 */
	{ "iwlwifi-6000-6.ucode",     454608, "iwlwifi-6000-4.ucode" },
	{ "iwlwifi-6000g2a-5.ucode",  444128, nullptr },
	{ "iwlwifi-6000g2a-6.ucode",  677296, nullptr },
	{ "iwlwifi-6000g2b-5.ucode",  460236, nullptr },
	{ "iwlwifi-6000g2b-6.ucode",  679436, nullptr },
	{ "iwlwifi-6050-4.ucode",     463692, nullptr },
	{ "iwlwifi-6050-5.ucode",     469780, nullptr },
	{ "iwlwifi-7260-16.ucode",   1049284, nullptr },
	{ "iwlwifi-7260-17.ucode",   1049284, "iwlwifi-7260-16.ucode" },
	{ "iwlwifi-7265-16.ucode",   1180356, nullptr },
	{ "iwlwifi-7265D-16.ucode",  1384500, nullptr },
	{ "iwlwifi-7265D-19.ucode",  1384500, "iwlwifi-7265D-16.ucode" },
	{ "iwlwifi-8000C-16.ucode",  2351636, nullptr },
	{ "iwlwifi-8000C-19.ucode",  2351636, "iwlwifi-8000C-16.ucode" },
	{ "iwlwifi-3160-16.ucode",    918212, nullptr },
	{ "iwlwifi-3160-17.ucode",    918212, "iwlwifi-3160-16.ucode" }
};


size_t fw_list_len = sizeof(fw_list) / sizeof(fw_list[0]);


static Genode::Lock *_wpa_lock;


static void run_linux(void *)
{
	system_power_efficient_wq = alloc_workqueue("system_power_efficient_wq", 0, 0);
	system_wq                 = alloc_workqueue("system_wq", 0, 0);

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

	module_iwl_drv_init();

	_wpa_lock->unlock();

	while (1) {
		Lx::scheduler().current()->block_and_schedule();
	}
}

unsigned long jiffies;


void wifi_init(Genode::Env &env, Genode::Lock &lock, bool disable_11n)
{
	Lx_kit::construct_env(env);

	_wpa_lock = &lock;

	INIT_LIST_HEAD(&init_net.dev_base_head);
	/* add init_net namespace to namespace list */
	list_add_tail_rcu(&init_net.list, &net_namespace_list);

	Lx::scheduler(&env);

	Lx::timer(&env, &env.ep(), &Lx_kit::env().heap(), &jiffies);

	Lx::Irq::irq(&env.ep(), &Lx_kit::env().heap());
	Lx::Work::work_queue(&Lx_kit::env().heap());

	Lx::socket_init(env.ep(), Lx_kit::env().heap());
	Lx::nic_init(env, Lx_kit::env().heap());

	Lx::pci_init(env, env.ram(), Lx_kit::env().heap());
	Lx::malloc_init(env, Lx_kit::env().heap());

	/* set IWL_DISABLE_HT_ALL if disable 11n is requested */
	if (disable_11n) {
		Genode::log("Disable 11n mode");
		*module_param_11n_disable = 1;
	}

	/* Linux task (handles the initialization only currently) */
	static Lx::Task linux(run_linux, nullptr, "linux",
	                      Lx::Task::PRIORITY_0, Lx::scheduler());

	/* give all task a first kick before returning */
	Lx::scheduler().schedule();
}
