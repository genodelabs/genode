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

	using Usb_storage_devices = List_model<Usb_storage_device>;
};


struct Sculpt::Usb_storage_device : List_model<Usb_storage_device>::Element,
                                    Storage_device
{
	/**
	 * Information that is reported asynchronously by 'usb_block'
	 */
	struct Driver_info
	{
		using Vendor  = String<28>;
		using Product = String<48>;

		Vendor   const vendor;
		Product  const product;

		Driver_info(Node const &device)
		:
			vendor (device.attribute_value("vendor",  Vendor())),
			product(device.attribute_value("product", Product()))
		{ }
	};

	Env &_env;

	/* information provided asynchronously by usb_block */
	Constructible<Driver_info> driver_info { };

	Rom_handler<Usb_storage_device> _report {
		_env, String<80>("report -> runtime/", driver, "/devices").string(),
		*this, &Usb_storage_device::_handle_report };

	void _handle_report(Node const &) { _action.storage_device_discovered(); }

	void process_report()
	{
		_report.with_node([&] (Node const &report) {
			report.with_optional_sub_node("device", [&] (Node const &device) {

				capacity = Capacity { device.attribute_value("block_count", 0ULL)
				                    * device.attribute_value("block_size",  0ULL) };

				driver_info.construct(device);
			});
		});
	}

	bool usb_block_needed() const
	{
		bool needed = false;
		for_each_partition([&] (Partition const &partition) {
			needed |= partition.check_in_progress
			       || partition.format_in_progress
			       || partition.file_system.inspected
			       || partition.relabel_in_progress()
			       || partition.expand_in_progress(); });

		return needed || Storage_device::state == UNKNOWN;
	}

	/**
	 * Release USB device
	 *
	 * This method is called as response to a failed USB-block-driver
	 * initialization.
	 */
	void discard_usb_block()
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

	static Driver _driver(Node const &node)
	{
		return node.attribute_value("name", Driver());
	}

	Usb_storage_device(Env &env, Allocator &alloc, Node const &node,
	                   Storage_device::Action &action)
	:
		Storage_device(env, alloc, _driver(node), Port { }, Capacity{0}, action),
		_env(env)
	{ }

	inline void gen_usb_block_start_content(Generator &g) const;

	void gen_usb_policy(Generator &g) const
	{
		g.node("policy", [&] {
			g.attribute("label_prefix", driver);
			g.node("device", [&] {
				g.attribute("name", driver); }); });
	}

	static bool type_matches(Node const &device)
	{
		bool storage_device = false;
		device.for_each_sub_node("config", [&] (Node const &config) {
			config.for_each_sub_node("interface", [&] (Node const &interface) {
				if (interface.attribute_value("class", 0u) == 0x8)
					storage_device = true; }); });

		return storage_device;
	}

	bool matches(Node const &node) const { return _driver(node) == driver; }
};


void Sculpt::Usb_storage_device::gen_usb_block_start_content(Generator &g) const
{
	gen_common_start_content(g, driver, Cap_quota{100}, Ram_quota{6*1024*1024},
	                         Priority::STORAGE);

	gen_named_node(g, "binary", "usb_block");

	g.node("config", [&] {
		g.attribute("report",    "yes");
		g.attribute("writeable", "yes");
	});

	gen_provides<Block::Session>(g);

	g.tabular_node("route", [&] {
		gen_service_node<Usb::Session>(g, [&] {
			g.node("child", [&] {
				g.attribute("name", "usb"); }); });

		gen_parent_rom_route(g, "usb_block");
		gen_parent_rom_route(g, "ld.lib.so");
		gen_parent_route<Cpu_session>    (g);
		gen_parent_route<Pd_session>     (g);
		gen_parent_route<Log_session>    (g);
		gen_parent_route<Timer::Session> (g);

		gen_service_node<Report::Session>(g, [&] {
			g.node("parent", [&] { }); });
	});
}

#endif /* _MODEL__BLOCK_DEVICE_H_ */
