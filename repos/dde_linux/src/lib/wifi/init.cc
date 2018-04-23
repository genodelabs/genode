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
extern "C" void fs_cfg80211_init(void);
extern "C" void subsys_ieee80211_init(void);
extern "C" int module_iwl_drv_init(void);
extern "C" void subsys_cryptomgr_init(void);
extern "C" void module_crypto_ccm_module_init(void);
extern "C" void module_crypto_ctr_module_init(void);
extern "C" void module_aes_init(void);
extern "C" void module_arc4_init(void);
// extern "C" void module_chainiv_module_init(void);
extern "C" void module_krng_mod_init(void);

extern "C" unsigned int *module_param_11n_disable;

struct workqueue_struct *system_power_efficient_wq;
struct workqueue_struct *system_wq;

struct pernet_operations loopback_net_ops;

struct net init_net;
LIST_HEAD(net_namespace_list);


static Genode::Lock *_wpa_lock;


static void run_linux(void *args)
{
	system_power_efficient_wq = alloc_workqueue("system_power_efficient_wq", 0, 0);
	system_wq                 = alloc_workqueue("system_wq", 0, 0);

	core_sock_init();
	core_netlink_proto_init();
	module_packet_init();
	subsys_genl_init();
	subsys_rfkill_init();
	fs_cfg80211_init();
	subsys_ieee80211_init();

	subsys_cryptomgr_init();
	module_crypto_ccm_module_init();
	module_crypto_ctr_module_init();
	module_aes_init();
	module_arc4_init();
	// module_chainiv_module_init();

	try {
		int const err = module_iwl_drv_init();
		if (err) { throw -1; }
	} catch (...) {
		Genode::Env &env = *(Genode::Env*)args;

		env.parent().exit(1);
		Genode::sleep_forever();
	}

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
	static Lx::Task linux(run_linux, &env, "linux",
	                      Lx::Task::PRIORITY_0, Lx::scheduler());

	/* give all task a first kick before returning */
	Lx::scheduler().schedule();
}
