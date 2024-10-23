/*
 * \brief  Sculpt dynamic drivers management
 * \author Norman Feske
 * \date   2024-03-25
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <block_session/block_session.h>
#include <capture_session/capture_session.h>
#include <event_session/event_session.h>
#include <gpu_session/gpu_session.h>
#include <i2c_session/i2c_session.h>
#include <io_port_session/io_port_session.h>
#include <pin_control_session/pin_control_session.h>
#include <pin_state_session/pin_state_session.h>
#include <timer_session/connection.h>
#include <uplink_session/uplink_session.h>
#include <vm_session/vm_session.h>

/* local includes */
#include <drivers.h>
#include <driver/ahci.h>
#include <driver/fb.h>
#include <driver/mmc.h>
#include <driver/nvme.h>
#include <driver/ps2.h>
#include <driver/touch.h>
#include <driver/usb.h>
#include <driver/wifi.h>
#include <driver/nic.h>

class Sculpt::Drivers::Instance : Noncopyable,
                                  Nvme_driver::Action,
                                  Mmc_driver::Action,
                                  Ahci_driver::Action,
                                  Usb_driver::Action, Usb_driver::Info
{
	private:

		using Action = Drivers::Action;
		using Info   = Drivers::Info;

		Env        &_env;
		Children   &_children;
		Info const &_info;
		Action     &_action;

		Board_info _board_info { };
		Resumed    _resumed    { };

		Attached_rom_dataspace const _platform { _env, "platform_info" };

		Rom_handler<Instance> _devices {
			_env, "report -> drivers/devices", *this, &Instance::_handle_devices };

		void _handle_devices(Xml_node const &devices)
		{
			_board_info.detected = Board_info::Detected::from_xml(devices,
			                                                      _platform.xml());
			_board_info.used = Board_info::Used::from_xml(devices);

			_resumed = { devices.attribute_value("resumed", 0u) };

			/*
			 * The decision which fb driver to start might depend (e.g. on PC)
			 * on information about available devices from both the devices ROM
			 * and the platform info ROM, so we skip the update if the devices
			 * ROM is not ready yet.
			 *
			 * On SoC platforms this information is provided by the static
			 * Board_info::Soc configuration and gets evluated instead to make
			 * the decision.
			 */
			if (!devices.has_type("empty") || _board_info.soc.fb)
				_fb_driver.update(_children, _board_info, _platform.xml());

			_ps2_driver  .update(_children, _board_info);
			_touch_driver.update(_children, _board_info);
			_ahci_driver .update(_children, _board_info);
			_nvme_driver .update(_children, _board_info);
			_mmc_driver  .update(_children, _board_info);
			_wifi_driver .update(_children, _board_info);
			_nic_driver  .update(_children, _board_info);

			_action.handle_device_plug_unplug();
		}

		void _handle_devices()
		{
			_devices.with_xml([&] (Xml_node const &devices) {
				_handle_devices(devices); });
		}

		Ps2_driver   _ps2_driver   { };
		Touch_driver _touch_driver { };
		Fb_driver    _fb_driver    { _env, _action };
		Usb_driver   _usb_driver   { _env, *this, *this };
		Ahci_driver  _ahci_driver  { _env, *this };
		Nvme_driver  _nvme_driver  { _env, *this };
		Mmc_driver   _mmc_driver   { _env, *this };
		Wifi_driver  _wifi_driver  { };
		Nic_driver   _nic_driver   { };

		void gen_usb_storage_policies(Xml_generator &xml) const override
		{
			_info.gen_usb_storage_policies(xml);
		}

		void handle_usb_plug_unplug() override { _action.handle_device_plug_unplug(); }
		void handle_ahci_discovered() override { _action.handle_device_plug_unplug(); }
		void handle_mmc_discovered()  override { _action.handle_device_plug_unplug(); }
		void handle_nvme_discovered() override { _action.handle_device_plug_unplug(); }

	public:

		Instance(Env &env, Children &children, Info const &info, Action &action)
		:
			_env(env), _children(children), _info(info), _action(action)
		{ }

		void update_usb() { _usb_driver.update(_children, _board_info); }

		void update_soc(Board_info::Soc soc)
		{
			_board_info.soc = soc;
			_handle_devices();
		}

		void update_options(Board_info::Options const options)
		{
			if (options != _board_info.options) {
				_board_info.options = options;
				_handle_devices();
				_usb_driver.update(_children, _board_info);
			}
		}

		void gen_start_nodes(Xml_generator &xml) const
		{
			_ps2_driver  .gen_start_node (xml);
			_touch_driver.gen_start_node (xml);
			_fb_driver   .gen_start_nodes(xml);
			_usb_driver  .gen_start_nodes(xml);
			_ahci_driver .gen_start_node (xml);
			_nvme_driver .gen_start_node (xml);
			_mmc_driver  .gen_start_node (xml);
			_wifi_driver .gen_start_node (xml);
			_nic_driver  .gen_start_node (xml);
		}

		void with(With_storage_devices::Callback const &fn) const
		{
			_usb_driver.with_devices([&] (Xml_node const &usb_devices) {
				_ahci_driver.with_ports([&] (Xml_node const &ahci_ports) {
					_nvme_driver.with_namespaces([&] (Xml_node const &nvme_namespaces) {
						_mmc_driver.with_devices([&] (Xml_node const &mmc_devices) {
							fn( { .usb  = usb_devices,
							      .ahci = ahci_ports,
							      .nvme = nvme_namespaces,
							      .mmc  = mmc_devices }); }); }); }); });
		}

		void with(With_board_info::Callback    const &fn) const { fn(_board_info); }
		void with_platform_info(With_xml::Callback const &fn) const { fn(_platform.xml()); }
		void with_fb_connectors(With_xml::Callback const &fn) const { _fb_driver.with_connectors(fn); }

		bool suspend_supported() const
		{
			return _fb_driver.suspend_supported(_board_info)
			   && _usb_driver.suspend_supported();
		}

		bool ready_for_suspend() const { return !_board_info.used.any(); }

		Resumed resumed() const { return _resumed; }
};


using namespace Sculpt;


Drivers::Instance &Drivers::_construct_instance(auto &&... args)
{
	static bool called_once;
	if (called_once)
		error("unexpected attempt to construct multiple 'Drivers' instances");
	called_once = true;

	static Drivers::Instance instance { args... };
	return instance;
}


Sculpt::Drivers::Drivers(Env &env, Children &children, Info const &info, Action &action)
:
	_instance(_construct_instance(env, children, info, action))
{ }

void Drivers::_with(With_storage_devices::Callback   const &fn) const { _instance.with(fn); }
void Drivers::_with(With_board_info::Callback        const &fn) const { _instance.with(fn); }
void Drivers::_with_platform_info(With_xml::Callback const &fn) const { _instance.with_platform_info(fn); }
void Drivers::_with_fb_connectors(With_xml::Callback const &fn) const { _instance.with_fb_connectors(fn); }

void Drivers::update_usb    ()                        { _instance.update_usb(); }
void Drivers::update_soc    (Board_info::Soc     soc) { _instance.update_soc(soc); }
void Drivers::update_options(Board_info::Options opt) { _instance.update_options(opt); }

void Drivers::gen_start_nodes(Xml_generator &xml) const { _instance.gen_start_nodes(xml); }

bool Drivers::suspend_supported() const { return _instance.suspend_supported(); };
bool Drivers::ready_for_suspend() const { return _instance.ready_for_suspend(); };

Drivers::Resumed Drivers::resumed() const { return _instance.resumed(); };
