/*
 * \brief  Handle fixed ACPI events, e.g. power button and sleep button
 * \author Alexander Boettcher
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

class Fixed : Acpica::Callback<Fixed> {

	private:

		Acpica::Reportstate * _report;

		UINT64 _power_button_count = 0;
		UINT64 _sleep_button_count = 0;
		bool _power_button_pressed = false;
		bool _sleep_button_pressed = false;

	public:

		Fixed(void * report)
			: _report(reinterpret_cast<Acpica::Reportstate *>(report))
		{
			if (_report)
				_report->add_notify(this);
		}

		static
		UINT32 handle_power_button(void *context)
		{
			Fixed * me = reinterpret_cast<Fixed *>(context);

			me->_power_button_count++;

			if (me->_report) {
				me->_power_button_pressed = true;
				me->_report->fixed_event();
			}

			return AE_OK;
		}

		static
		UINT32 handle_sleep_button(void *context)
		{
			Fixed * me = reinterpret_cast<Fixed *>(context);

			me->_sleep_button_count++;

			if (me->_report) {
				me->_sleep_button_pressed = true;
				me->_report->fixed_event();
			}

			return AE_OK;
		}

		void generate(Genode::Xml_generator &xml)
		{
			if (_power_button_count)
				xml.node("power_button", [&] {
					xml.attribute("value", _power_button_pressed);
					xml.attribute("count", _power_button_count);
					if (_power_button_pressed) {
						_power_button_pressed = false;
						xml.append("pressed");
					}
				});

			if (_sleep_button_count)
				xml.node("sleep_button", [&] {
					xml.attribute("value", _sleep_button_pressed);
					xml.attribute("count", _sleep_button_count);
					if (_sleep_button_pressed) {
						_sleep_button_pressed = false;
						xml.append("pressed");
					}
				});
		}
};
