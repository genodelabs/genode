/*
 * \brief  Suplib driver implementation
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2020-11-20
 */

/*
 * Copyright (C) 2020-2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Virtualbox includes */
#include <NEMInternal.h> /* enable access to nem.s.* */

/* local includes */
#include <sup_drv.h>
#include <pthread_emt.h>


Sup::Cpu_freq_khz Sup::Drv::_cpu_freq_khz_from_rom()
{
	unsigned khz = 0;

	_platform_info_rom.xml().with_optional_sub_node("hardware", [&] (Xml_node const &node) {
		node.with_optional_sub_node("tsc", [&] (Xml_node const &node) {
			khz = node.attribute_value("freq_khz", khz); });
	});

	if (khz == 0) {
		error("could not read CPU frequency");
		sleep_forever();
	}

	return Cpu_freq_khz { khz };
}


Sup::Drv::Cpu_virt Sup::Drv::_cpu_virt_from_rom()
{
	Cpu_virt virt = Cpu_virt::NONE;

	_platform_info_rom.xml().with_optional_sub_node("hardware", [&] (Xml_node const &node) {
		node.with_optional_sub_node("features", [&] (Xml_node const &node) {
			if (node.attribute_value("vmx", false))
				virt = Cpu_virt::VMX;
			else if (node.attribute_value("svm", false))
				virt = Cpu_virt::SVM;
		});
	});

	return virt;
}


Sup::Vcpu & Sup::Drv::create_vcpu(VM &vm, Cpu_index cpu_index, Pthread::Emt &emt)
{
	switch (_cpu_virt) {

	case Cpu_virt::VMX:
		return Vcpu::create_vmx(_env, vm, _vm_connection, cpu_index, emt);

	case Cpu_virt::SVM:
		return Vcpu::create_svm(_env, vm, _vm_connection, cpu_index, emt);

	case Cpu_virt::NONE:
		break;
	}

	throw Virtualization_support_missing();
}
