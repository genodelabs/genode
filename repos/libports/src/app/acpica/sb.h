/*
 * \brief  Handle ACPI Smart Battery Subsystem devices
 * \author Alexander Boettcher
 *
 */

/*
 * Copyright (C) 2016-2026 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

class Battery : Acpica::Callback<Battery>
{
	private:

		Acpica::Reportstate * _report;
		ACPI_HANDLE const     _sb;

		/*
		 * ACPI spec - 10.2.2.1 _BIF - Battery Information
		 * ACPI spec - 10.2.2.2 _BIX - Battery Information - ACPI 4.0 +
		 */
		Acpica::Buffer<char [1024]> _battery;
		Genode::String<16>          _battery_name;

		enum Info_type { NO_INFO, BIF_INFO, BIX_INFO };
		Info_type const _info_type;

		Info_type _init_static_info()
		{
			Info_type type = NO_INFO;

			auto res = AcpiEvaluateObjectTyped(_sb, ACPI_STRING("_BIF"),
			                                   nullptr, &_battery,
			                                   ACPI_TYPE_PACKAGE);
			if (ACPI_FAILURE(res)) {
				res = AcpiEvaluateObjectTyped(_sb, ACPI_STRING("_BIX"),
				                              nullptr, &_battery,
				                              ACPI_TYPE_PACKAGE);
				if (!ACPI_FAILURE(res))
					type = BIX_INFO;
			} else
				type = BIF_INFO;

			auto obj = reinterpret_cast<ACPI_OBJECT *>(_battery.object);
			if (ACPI_FAILURE(res) || !obj) {
				Genode::error("failed   - '", __func__, "' _BIF/_BIX");
				return type;
			}

			Acpica::Buffer<char [16]> battery_name;
			res = AcpiGetName(_sb, ACPI_SINGLE_NAME, &battery_name);

			_battery_name = ACPI_FAILURE(res)
			              ? Genode::String<16>("unknown")
			              : Genode::String<16>(battery_name.object);

			Genode::log("detected - smart battery: ", _battery_name);

			return type;
		}

		void _info_bix(Genode::Generator &g)
		{
			g.node("name", [&] { g.append_quoted(_battery_name.string()); });

			const char * node_name[] = {
				"revision", "powerunit", "design_capacity", "last_full_capacity",
				"technology", "voltage", "warning_capacity", "low_capacity",
				"cycle_count", "accuracy", "max_sample_time", "min_sample_time",
				"max_aver_itvl", "min_aver_itvl",
				"granularity1", "granularity2", "model", "serial", "type",
				"oem", "swap_cap"
			};

			_info(g, 1, 4, node_name, sizeof(node_name) / sizeof(node_name[0]));
		}

		void _info_bif(Genode::Generator &g)
		{
			g.node("name", [&] { g.append_quoted(_battery_name.string()); });

			const char * node_name[] = {
				"powerunit", "design_capacity", "last_full_capacity",
				"technology", "voltage", "warning_capacity", "low_capacity",
				"granularity1", "granularity2", "serial", "model", "type",
				"oem"
			};

			_info(g, 0, 3, node_name, sizeof(node_name) / sizeof(node_name[0]));
		}

		void _info(Genode::Generator &g, unsigned power_pos, unsigned tech_pos,
		           char const ** node_names, unsigned const nodes)
		{
			auto const &obj = *reinterpret_cast<ACPI_OBJECT *>(_battery.object);

			if (nodes != obj.Package.Count)
				return;

			for (unsigned i = 0; i < obj.Package.Count; i++) {
				auto const &value = obj.Package.Elements[i];

				g.node(node_names[i], [&] {
					if (value.Type == ACPI_TYPE_STRING) {
						g.append_quoted(value.String.Pointer);
						return;
					}

					if (value.Type != ACPI_TYPE_INTEGER)
						return;

					g.attribute("value", value.Integer.Value);

					if (i == power_pos)
						g.append_quoted(value.Integer.Value == 0 ? "mW/mWh" :
						                value.Integer.Value == 1 ? "mA/mAh" :
						                                           "unknown");
					if (i == tech_pos)
						g.append_quoted(value.Integer.Value == 0 ? "primary" :
						                value.Integer.Value == 1 ? "secondary" :
						                                           "unknown");
				});
			}
		}

		void _status(Genode::Generator &g)
		{
			/* 10.2.2.6 _BST (Battery Status) */
			Acpica::Buffer<char [256]> dynamic;
			ACPI_STATUS res = AcpiEvaluateObjectTyped(_sb, ACPI_STRING("_BST"),
			                                          nullptr, &dynamic,
			                                          ACPI_TYPE_PACKAGE);
			ACPI_OBJECT * obj = reinterpret_cast<ACPI_OBJECT *>(dynamic.object);
			if (ACPI_FAILURE(res) || !obj ||
			    obj->Package.Count != 4) {
				Genode::error("failed   - '", __func__, "' _BST res=", Genode::Hex(res));
				return;
			}

			Acpica::Buffer<ACPI_OBJECT> sta;
			res = AcpiEvaluateObjectTyped(_sb, ACPI_STRING("_STA"), nullptr,
			                              &sta, ACPI_TYPE_INTEGER);
			if (ACPI_FAILURE(res)) {
				g.node("status", [&] { g.append_quoted("unknown"); });
			} else
				g.node("status", [&] {
					unsigned long long const val = sta.object.Integer.Value;
					g.attribute("value", val);
					/* see "6.3.7 _STA" for more human readable decoding */
					if (!(sta.object.Integer.Value & ACPI_STA_BATTERY_PRESENT))
						g.append_quoted("battery not present");
				});

			const char * node_name[] = {
				"state", "present_rate", "remaining_capacity",
				"present_voltage"
			};

			if (sizeof(node_name) / sizeof(node_name[0]) != obj->Package.Count)
				return;

			for (unsigned i = 0; i < obj->Package.Count; i++) {
				ACPI_OBJECT * v = &obj->Package.Elements[i];

				g.node(node_name[i], [&] {
					if (v->Type != ACPI_TYPE_INTEGER) {
						g.append_quoted("unknown");
						return;
					}

					g.attribute("value", v->Integer.Value);

					if (i != 0)
						return;

					if (v->Integer.Value & 0x1) g.append_quoted("discharging");
					if (v->Integer.Value & 0x2) g.append_quoted("charging");
					if (v->Integer.Value & 0x4) g.append_quoted("critical low");
				});
			}
		}

	public:

		Battery(Acpica::Reportstate * report, ACPI_HANDLE sb)
		:
			_report(report), _sb(sb), _info_type(_init_static_info())
		{
			if (_report)
				_report->add_notify(this);
		}

		static ACPI_STATUS detect(ACPI_HANDLE sb, UINT32, void *m, void **)
		{
			auto * main    = reinterpret_cast<Acpica::Main *>(m);
			auto * dev_obj = new (main->heap) Battery(main->report, sb);

			auto res = AcpiInstallNotifyHandler(sb, ACPI_DEVICE_NOTIFY,
				                                Acpica::Callback<Battery>::handler,
				                                dev_obj);
			if (ACPI_FAILURE(res)) {
				Genode::error("failed   - '", __func__, "' "
				              "res=", Genode::Hex(res));
				delete dev_obj;
			}

			return AE_OK;
		}

		/*
		 * Acpica::Callback<> interface
		 */

		void handle(ACPI_HANDLE sb, UINT32 value)
		{
			if (_report)
				_report->battery_event();
		}

		void generate(Genode::Generator &g)
		{
			if (_info_type == BIF_INFO) _info_bif(g);
			if (_info_type == BIX_INFO) _info_bix(g);

			_status(g);
		}
};
