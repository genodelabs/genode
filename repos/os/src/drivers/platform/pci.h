/*
 * \brief  Platform driver - PCI helper utilities
 * \author Stefan Kalkowski
 * \date   2022-05-02
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__DRIVERS__PLATFORM__PCI_H_
#define _SRC__DRIVERS__PLATFORM__PCI_H_

/* Genode includes */
#include <base/env.h>
#include <irq_session/irq_session.h>
#include <os/session_policy.h>

/* local includes */
#include <device.h>
#include <io_mmu_domain_registry.h>

namespace Driver {
	class Device_component;
	class Device_pd;

	void pci_enable(Genode::Env            & env,
	                Device const           & dev);
	void pci_disable(Genode::Env            & env,
	                 Device const           & dev);
	void pci_apply_quirks(Genode::Env & env, Device const & dev);
	void pci_msi_enable(Genode::Env & env, Device_component & dc,
	                    addr_t cfg_space, Genode::Irq_session::Info const info,
	                    Irq_session::Type type);
	bool pci_device_matches(Genode::Session_policy const & policy,
	                        Device const & dev);
	void pci_device_specific_info(Device const  & dev,
	                              Env           & env,
	                              Device_model  & model,
	                              Xml_generator & xml);
}

#endif /* _SRC__DRIVERS__PLATFORM__PCI_H_ */
