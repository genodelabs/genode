/*
 * \brief  Platform driver for PC
 * \author Stefan Kalkowski
 * \date   2022-10-05
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>

#include <common.h>
#include <intel/io_mmu.h>

namespace Driver { struct Main; };

struct Driver::Main
{
	Env                  & _env;
	Attached_rom_dataspace _config_rom     { _env, "config"        };
	Attached_rom_dataspace _acpi_rom       { _env, "acpi"          };
	Attached_rom_dataspace _system_rom     { _env, "system"        };
	Attached_rom_dataspace _sleep_rom      { _env, "sleep_states"  };
	Common                 _common         { _env, _config_rom     };
	Signal_handler<Main>   _config_handler { _env.ep(), *this,
	                                         &Main::_handle_config };
	Signal_handler<Main>   _system_handler { _env.ep(), *this,
	                                         &Main::_system_update };

	Intel::Io_mmu_factory  _intel_iommu    { _env, _common.io_mmu_factories() };

	void _handle_config();
	void _suspend(String<8>);
	void _reset();
	void _system_update();

	Main(Genode::Env & e)
	: _env(e)
	{
		_config_rom.sigh(_config_handler);
		_acpi_rom  .sigh(_system_handler);
		_system_rom.sigh(_system_handler);
		_sleep_rom .sigh(_system_handler);

		_handle_config();
		_system_update();

		_common.acquire_io_mmu_devices();
		_common.announce_service();
	}
};


void Driver::Main::_handle_config()
{
	_config_rom.update();

	if (_config_rom.valid())
		_common.handle_config(_config_rom.xml());
}


void Driver::Main::_reset()
{
	_acpi_rom.update();

	if (!_acpi_rom.valid())
		return;

	_acpi_rom.xml().with_optional_sub_node("reset", [&] (Xml_node reset)
	{
		uint16_t const io_port = reset.attribute_value<uint16_t>("io_port", 0);
		uint8_t  const value   = reset.attribute_value<uint8_t>("value",    0);

		log("trigger reset by writing value ", value, " to I/O port ", Hex(io_port));

		try {
			Io_port_connection reset_port { _env, io_port, 1 };
			reset_port.outb(io_port, value);
		} catch (...) {
			error("unable to access reset I/O port ", Hex(io_port)); }
	});
}


void Driver::Main::_system_update()
{
	_system_rom.update();

	if (!_system_rom.valid())
		return;

	auto const state = _system_rom.xml().attribute_value("state", String<16>());

	if (state == "reset")
		_reset();

	if (state == "suspend") {
		/* save IOMMU state */
		_common.io_mmu_devices().for_each([&] (Driver::Io_mmu & io_mmu) {
			io_mmu.suspend();
		});

		try { _suspend("S3"); } catch (...) { error("suspend failed"); }

		/* re-initialise IOMMU independent of result */
		_common.io_mmu_devices().for_each([&] (Driver::Io_mmu & io_mmu) {
			io_mmu.resume();
		});
		/* report independent of result */
		_common.report_resume();
	}
}


void Driver::Main::_suspend(String<8> suspend_mode)
{
	_sleep_rom .update();

	if (!_sleep_rom.valid())
		return;

	struct Client: Genode::Rpc_client<Pd_session::System_control>
	{
		explicit Client(Genode::Capability<Pd_session::System_control> cap)
		: Rpc_client<Pd_session::System_control>(cap) { }

		Pd_session::Managing_system_state system_control(Pd_session::Managing_system_state const &state) override {
			return call<Rpc_system_control>(state); }
	} system_control { _env.pd().system_control_cap(Affinity::Location()) };

	_sleep_rom.xml().with_sub_node(suspend_mode.string(), [&] (auto const &node) {

		auto const typea = "SLP_TYPa";
		auto const typeb = "SLP_TYPb";
		auto const valid = node.attribute_value("supported", false) &&
		                   node.has_attribute(typea) &&
		                   node.has_attribute(typeb);

		if (!valid)
			return;

		auto s3_sleep_typea = uint8_t(node.attribute_value(typea, 0u));
		auto s3_sleep_typeb = uint8_t(node.attribute_value(typeb, 0u));

		Pd_session::Managing_system_state in { }, out { };

		in.trapno = Pd_session::Managing_system_state::ACPI_SUSPEND_REQUEST;
		in.ip     = s3_sleep_typea;
		in.sp     = s3_sleep_typeb;

		out = system_control.system_control(in);

		if (!out.trapno)
			error(suspend_mode, " suspend failed");
		else
			log("resumed from ", suspend_mode);
	}, [&] {
		warning(suspend_mode, " not supported");
	});
}


void Component::construct(Genode::Env &env) {
	static Driver::Main main(env); }
