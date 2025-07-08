/*
 * \brief  Generate reports of various ACPI devices, e.g.
 *         Lid, Embedded Controller (EC), AC Adapter,
 *         Smart Battery (SB) and ACPI fixed events (power, sleep button)
 * \author Alexander Boettcher
 */

/*
 * Copyright (C) 2016-2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

class Ac;
class Battery;
class Ec;
class Fixed;
class Lid;

namespace Acpica {
	class Reportstate;
	class Reporter;
};

class Acpica::Reporter : public Genode::List<Acpica::Reporter >::Element
{
	public:

		Reporter() { }

		virtual void generate(Genode::Generator &) = 0;
		virtual ~Reporter() { }
};


class Acpica::Reportstate {

	private:

		Genode::Expanding_reporter _reporter_lid;
		Genode::Expanding_reporter _reporter_ac;
		Genode::Expanding_reporter _reporter_sb;
		Genode::Expanding_reporter _reporter_ec;
		Genode::Expanding_reporter _reporter_fix;
		Genode::Expanding_reporter _reporter_hid;

		bool _changed_lid   = false;
		bool _changed_ac    = false;
		bool _changed_sb    = false;
		bool _changed_ec    = false;
		bool _changed_fixed = false;
		bool _changed_hid   = false;

		Genode::List<Callback<Battery> >  _list_sb;
		Genode::List<Callback<Ec> >       _list_ec;
		Genode::List<Callback<Ac> >       _list_ac;
		Genode::List<Acpica::Reporter>    _list_hid;
		Callback<Fixed>                 * _fixed;
		Callback<Lid>                   * _lid;

	public:

		Reportstate(Genode::Env &env)
		:
			_reporter_lid(env, "acpi_lid"),
			_reporter_ac (env, "acpi_ac"),
			_reporter_sb (env, "acpi_battery"),
			_reporter_ec (env, "acpi_ec"),
			_reporter_fix(env, "acpi_fixed"),
			_reporter_hid(env, "acpi_hid")
		{ }

		void add_notify(Acpica::Callback<Battery> * s) { _list_sb.insert(s); }
		void add_notify(Acpica::Callback<Fixed> * f)   { _fixed = f; }
		void add_notify(Acpica::Callback<Lid> * l)     { _lid = l; }
		void add_notify(Acpica::Callback<Ec> * e)      { _list_ec.insert(e); }
		void add_notify(Acpica::Callback<Ac> * a)      { _list_ac.insert(a); }
		void add_notify(Acpica::Reporter * r)          { _list_hid.insert(r); }

		void battery_event() { _changed_sb    = true; }
		void ec_event()      { _changed_ec    = true; }
		void fixed_event()   { _changed_fixed = true; }
		void lid_event()     { _changed_lid   = true; }
		void ac_event()      { _changed_ac    = true; battery_event(); }
		void hid_event()     { _changed_hid   = true; }

		bool generate_report(bool force = false)
		{
			bool const changed = _changed_sb  || _changed_ec ||
			                     _changed_fixed || _changed_lid || _changed_ac;

			if (_changed_lid || force) {
				_changed_lid = false;
				if (_lid)
					_reporter_lid.generate([&] (Generator &g) {
						_lid->generate(g); });
			}

			if (_changed_ac || force) {
				_changed_ac = false;
				_reporter_ac.generate([&] (Generator &g) {
					for (Callback<Ac> * ac = _list_ac.first(); ac; ac = ac->next())
						g.node("ac", [&] { ac->generate(g); }); });
			}

			if (_changed_ec || force) {
				_changed_ec = false;
				_reporter_ec.generate([&] (Generator &g) {
					for (Callback<Ec> * ec = _list_ec.first(); ec; ec = ec->next())
						g.node("ec", [&] { ec->generate(g); }); });
			}

			if (_changed_sb || force) {
				_changed_sb = false;
				_reporter_sb.generate([&] (Generator &g) {
					for (Callback<Battery> * sb = _list_sb.first(); sb; sb = sb->next())
						g.node("sb", [&] { sb->generate(g); }); });
			}

			if (_changed_fixed || force) {
				_changed_fixed = false;
				if (_fixed)
					_reporter_fix.generate([&] (Generator &g) {
						_fixed->generate(g); });
			}

			if (_changed_hid || force) {
				_changed_hid = false;

				if (_list_hid.first())
					_reporter_hid.generate([&] (Generator &g) {
						for (auto * hid = _list_hid.first(); hid; hid = hid->next())
							hid->generate(g); });
			}

			return changed;
		}
};
