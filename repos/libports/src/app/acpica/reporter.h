/*
 * \brief  Generate xml reports of various ACPI devices, e.g.
 *         Lid, Embedded Controller (EC), AC Adapter,
 *         Smart Battery (SB) and ACPI fixed events (power, sleep button)
 * \author Alexander Boettcher
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

class Ac;
class Battery;
class Ec;
class Fixed;
class Lid;

class Acpica::Reportstate {

	private:

		Genode::Reporter _reporter_lid;
		Genode::Reporter _reporter_ac;
		Genode::Reporter _reporter_sb;
		Genode::Reporter _reporter_ec;
		Genode::Reporter _reporter_fix;

		bool _changed_lid   = false;
		bool _changed_ac    = false;
		bool _changed_sb    = false;
		bool _changed_ec    = false;
		bool _changed_fixed = false;

		Genode::List<Callback<Battery> > _list_sb;
		Genode::List<Callback<Ec> > _list_ec;
		Genode::List<Callback<Ac> > _list_ac;
		Callback<Fixed> * _fixed;
		Callback<Lid> * _lid;

	public:

		Reportstate(Genode::Env &env)
		:
			_reporter_lid(env, "acpi_lid"),
			_reporter_ac (env, "acpi_ac"),
			_reporter_sb (env, "acpi_battery"),
			_reporter_ec (env, "acpi_ec"),
			_reporter_fix(env, "acpi_fixed")
		{ }

		void add_notify(Acpica::Callback<Battery> * s) { _list_sb.insert(s); }
		void add_notify(Acpica::Callback<Fixed> * f)   { _fixed = f; }
		void add_notify(Acpica::Callback<Lid> * l)     { _lid = l; }
		void add_notify(Acpica::Callback<Ec> * e)      { _list_ec.insert(e); }
		void add_notify(Acpica::Callback<Ac> * a)      { _list_ac.insert(a); }

		void enable() {
			_reporter_ac.enabled(true);
			_reporter_ec.enabled(true);
			_reporter_sb.enabled(true);
			_reporter_lid.enabled(true);
			_reporter_fix.enabled(true);
		}

		void battery_event() { _changed_sb    = true; }
		void ec_event()      { _changed_ec    = true; }
		void fixed_event()   { _changed_fixed = true; }
		void lid_event()     { _changed_lid   = true; }
		void ac_event()      { _changed_ac    = true; battery_event(); }

		bool generate_report(bool force = false)
		{
			bool const changed = _changed_sb  || _changed_ec ||
			                     _changed_fixed || _changed_lid || _changed_ac;

			if (_changed_lid || force) {
				_changed_lid = false;
				if (_lid)
					Genode::Reporter::Xml_generator xml(_reporter_lid, [&] () {
						_lid->generate(xml);
					});
			}

			if (_changed_ac || force) {
				_changed_ac = false;
				Genode::Reporter::Xml_generator xml(_reporter_ac, [&] () {
					for (Callback<Ac> * ac = _list_ac.first(); ac; ac = ac->next())
						xml.node("ac", [&] { ac->generate(xml); });
				});
			}

			if (_changed_ec || force) {
				_changed_ec = false;
				Genode::Reporter::Xml_generator xml(_reporter_ec, [&] () {
					for (Callback<Ec> * ec = _list_ec.first(); ec; ec = ec->next())
						xml.node("ec", [&] { ec->generate(xml); });
				});
			}

			if (_changed_sb || force) {
				_changed_sb = false;
				Genode::Reporter::Xml_generator xml(_reporter_sb, [&] () {
					for (Callback<Battery> * sb = _list_sb.first(); sb; sb = sb->next())
						xml.node("sb", [&] { sb->generate(xml); });
				});
			}

			if (_changed_fixed || force) {
				_changed_fixed = false;
				if (_fixed)
					Genode::Reporter::Xml_generator xml(_reporter_fix, [&] () {
						_fixed->generate(xml);
					});
			}

			return changed;
		}
};
