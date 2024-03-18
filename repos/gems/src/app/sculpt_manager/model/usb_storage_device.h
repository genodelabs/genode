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
	void discard_usb_block_drv()
	{
		Storage_device::state = FAILED;

		/*
		 * Exclude device from set of inspected file systems. This is needed
		 * whenever the USB block driver fails sometime after an inspect button
		 * is activated.
		 */
		for_each_partition([&] (Partition &partition) {
			partition.file_system.inspected = false; });
	}

	bool discarded() const { return Storage_device::state == FAILED; }

	Label usb_block_drv_name() const { return label; }

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

	void gen_usb_policy(Xml_generator &xml) const
	{
		xml.node("policy", [&] {
			xml.attribute("label_prefix", label);
			xml.node("device", [&] {
				xml.attribute("name", label); }); });
	}

	static bool type_matches(Xml_node const &device)
	{
		bool storage_device = false;
		device.for_each_sub_node("config", [&] (Xml_node const &config) {
			config.for_each_sub_node("interface", [&] (Xml_node const &interface) {
				if (interface.attribute_value("class", 0u) == 0x8)
					storage_device = true; }); });

		return storage_device;
	}

	bool matches(Xml_node node) const
	{
		return node.attribute_value("name", Label()) == label;
	}
};


void Sculpt::Usb_storage_device::gen_usb_block_drv_start_content(Xml_generator &xml) const
{
	gen_common_start_content(xml, usb_block_drv_name(),
	                         Cap_quota{100}, Ram_quota{6*1024*1024},
	                         Priority::STORAGE);

	gen_named_node(xml, "binary", "usb_block_drv");

	xml.node("config", [&] () {
		xml.attribute("report",    "yes");
		xml.attribute("writeable", "yes");
	});

	gen_provides<Block::Session>(xml);

	xml.node("route", [&] () {
		gen_service_node<Usb::Session>(xml, [&] () {
			xml.node("child", [&] () {
				xml.attribute("name", "usb"); }); });

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

#endif /* _MODEL__BLOCK_DEVICE_H_ */
