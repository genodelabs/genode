/*
 * \brief  Representation of USB storage devices
 * \author Norman Feske
 * \date   2018-04-30
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__USB_STORAGE_DEVICE_H_
#define _MODEL__USB_STORAGE_DEVICE_H_

#include "storage_device.h"

namespace Sculpt {

	struct Usb_storage_device;
	struct Usb_storage_device_update_policy;

	typedef List_model<Usb_storage_device> Usb_storage_devices;
};


struct Sculpt::Usb_storage_device : List_model<Usb_storage_device>::Element,
                                    Storage_device
{
	/**
	 * Information that is reported asynchronously by 'usb_block_drv'
	 */
	struct Driver_info
	{
		typedef String<28> Vendor;
		typedef String<48> Product;

		Vendor   const vendor;
		Product  const product;

		Driver_info(Xml_node device)
		:
			vendor (device.attribute_value("vendor",  Vendor())),
			product(device.attribute_value("product", Product()))
		{ }
	};

	/* information provided asynchronously by usb_block_drv */
	Constructible<Driver_info> driver_info { };

	Attached_rom_dataspace _driver_report_rom;

	void process_driver_report()
	{
		_driver_report_rom.update();

		Xml_node report = _driver_report_rom.xml();

		if (!report.has_sub_node("device"))
			return;

		Xml_node device = report.sub_node("device");

		capacity = Capacity { device.attribute_value("block_count", 0ULL)
		                    * device.attribute_value("block_size",  0ULL) };

		driver_info.construct(device);
	}

	bool usb_block_drv_needed() const
	{
		bool drv_needed = false;
		for_each_partition([&] (Partition const &partition) {
			drv_needed |= partition.check_in_progress
			           || partition.format_in_progress
			           || partition.file_system.inspected
			           || partition.relabel_in_progress()
			           || partition.expand_in_progress(); });

		return drv_needed || Storage_device::state == UNKNOWN;
	}

	/**
	 * Release USB device
	 *
	 * This method is called as response to a failed USB-block-driver
	 * initialization.
	 */
	void discard_usb_block_drv() { Storage_device::state = FAILED; }

	bool discarded() const { return Storage_device::state == FAILED; }

	Label usb_block_drv_name() const { return Label(label, ".drv"); }

	Usb_storage_device(Env &env, Allocator &alloc, Signal_context_capability sigh,
	                   Label const &label)
	:
		Storage_device(env, alloc, label, Capacity{0}, sigh),
		_driver_report_rom(env, String<80>("report -> runtime/", usb_block_drv_name(),
		                                   "/devices").string())
	{
		_driver_report_rom.sigh(sigh);
		process_driver_report();
	}

	inline void gen_usb_block_drv_start_content(Xml_generator &xml) const;
};


void Sculpt::Usb_storage_device::gen_usb_block_drv_start_content(Xml_generator &xml) const
{
	gen_common_start_content(xml, usb_block_drv_name(),
	                         Cap_quota{100}, Ram_quota{6*1024*1024});

	gen_named_node(xml, "binary", "usb_block_drv");

	xml.node("config", [&] () {
		xml.attribute("report",    "yes");
		xml.attribute("writeable", "yes");
	});

	gen_provides<Block::Session>(xml);

	xml.node("route", [&] () {
		gen_service_node<Usb::Session>(xml, [&] () {
			xml.node("parent", [&] () {
				xml.attribute("label", label); }); });

		gen_parent_rom_route(xml, "usb_block_drv");
		gen_parent_rom_route(xml, "ld.lib.so");
		gen_parent_route<Cpu_session>    (xml);
		gen_parent_route<Pd_session>     (xml);
		gen_parent_route<Log_session>    (xml);
		gen_parent_route<Timer::Session> (xml);

		gen_service_node<Report::Session>(xml, [&] () {
			xml.node("parent", [&] () { }); });
	});
}


struct Sculpt::Usb_storage_device_update_policy
:
	List_model<Usb_storage_device>::Update_policy
{
	Env       &_env;
	Allocator &_alloc;

	bool device_added_or_vanished = false;

	Signal_context_capability _sigh;

	Usb_storage_device_update_policy(Env &env, Allocator &alloc,
	                                 Signal_context_capability sigh)
	:
		_env(env), _alloc(alloc), _sigh(sigh)
	{ }

	typedef Usb_storage_device::Label Label;

	void destroy_element(Usb_storage_device &elem)
	{
		device_added_or_vanished = true;

		destroy(_alloc, &elem);
	}

	Usb_storage_device &create_element(Xml_node node)
	{
		device_added_or_vanished = true;

		return *new (_alloc)
			Usb_storage_device(_env, _alloc, _sigh,
			                   node.attribute_value("label_suffix", Label()));
	}

	void update_element(Usb_storage_device &, Xml_node) { }

	static bool node_is_element(Xml_node node)
	{
		return node.attribute_value("class", String<32>()) == "storage";
	};

	static bool element_matches_xml_node(Usb_storage_device const &elem, Xml_node node)
	{
		return node.attribute_value("label_suffix", Label()) == elem.label;
	}
};

#endif /* _MODEL__BLOCK_DEVICE_H_ */
