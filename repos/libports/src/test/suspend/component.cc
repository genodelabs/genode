/*
 * \brief  Test to trigger ACPI S3 suspend via Pd::managing_system()
 * \author Alexander Boettcher
 * \date   2022-08-01
 *
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/log.h>


using namespace Genode;


class Suspend
{
	private:

		Env                    &_env;
		Attached_rom_dataspace  _system_rom    { _env, "system" };
		Attached_rom_dataspace  _sleep_support { _env, "sleep_states" };
		Signal_handler<Suspend> _handler       { _env.ep(), *this,
	                                             &Suspend::system_update };

		uint8_t s3_sleep_typea { };
		uint8_t s3_sleep_typeb { };
		bool    s3_sleep_valid { };

		void suspend()
		{
			/*
			 * S0: normal power on
			 * S1: low  wake latency sleeping - cpu caches off - no reset vector used on resume in kernel !
			 * S2: low  wake latency sleep    - start from reset vector
			 * S3: low  wake latency sleep    - some parts powered off -> "suspend to RAM"
			 * S4: long wake latency sleep    - "suspend to disk"
			 * S5: soft off state
			 */

			if (!s3_sleep_valid) {
				warning("suspend ... denied");
				return;
			}

			log("suspend S3 (", s3_sleep_typea, ",", s3_sleep_typeb, ") ...");

			Pd_session::Managing_system_state in, out;

			in.trapno = Pd_session::Managing_system_state::ACPI_SUSPEND_REQUEST;
			in.ip = s3_sleep_typea;
			in.sp = s3_sleep_typeb;

			out = _env.pd().managing_system (in);

			if (!out.trapno)
				log("suspend failed");
			else
				log("resumed from S3");
		}

		void system_update()
		{
			_system_rom.update();
			_sleep_support.update();

			if (_system_rom.valid()) {
				auto state = _system_rom.xml().attribute_value("state",
				                                               String<16>(""));

				log("system update requested to '", state, "'");

				if (state == "suspend")
					suspend();
			}

			if (_sleep_support.valid()) {
				_sleep_support.xml().with_optional_sub_node("S3", [&] (auto const &node) {
					auto const typea = "SLP_TYPa";
					auto const typeb = "SLP_TYPb";

					s3_sleep_valid = node.attribute_value("supported", false) &&
					                 node.has_attribute(typea) &&
					                 node.has_attribute(typeb);

					if (s3_sleep_valid) {
						unsigned tmpa = node.attribute_value(typea, 0u);
						unsigned tmpb = node.attribute_value(typeb, 0u);
						if (tmpa < 256)
							s3_sleep_typea = uint8_t(node.attribute_value(typea, 0u));
						else
							s3_sleep_valid = false;

						if (tmpb < 256)
							s3_sleep_typeb = uint8_t(node.attribute_value(typeb, 0u));
						else
							s3_sleep_valid = false;
					}
				});
			}
		}

	public:

		Suspend(Env &env) : _env(env)
		{
			_system_rom.sigh(_handler);
			_sleep_support.sigh(_handler);

			system_update();
		}
};

void Component::construct(Genode::Env &env)
{
	static Suspend suspend(env);
}
