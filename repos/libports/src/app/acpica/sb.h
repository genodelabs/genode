/*
 * \brief  Handle ACPI Smart Battery Subsystem devices
 * \author Alexander Boettcher
 *
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

class Battery : Acpica::Callback<Battery> {

	private:

		Acpica::Reportstate * _report;
		ACPI_HANDLE _sb;

	public:

		Battery(void * report, ACPI_HANDLE sb)
		: _report(reinterpret_cast<Acpica::Reportstate *>(report)), _sb(sb)
		{
			if (_report)
				_report->add_notify(this);
		}

		void handle(ACPI_HANDLE sb, UINT32 value)
		{
			if (_report)
				_report->battery_event();
		}

		static ACPI_STATUS detect(ACPI_HANDLE sb, UINT32, void *report, void **)
		{
			Battery * dev_obj = new (Genode::env()->heap()) Battery(report, sb);

			ACPI_STATUS res = AcpiInstallNotifyHandler (sb, ACPI_DEVICE_NOTIFY,
			                                            handler, dev_obj);
			if (ACPI_FAILURE(res)) {
				Genode::error("failed   - '", __func__, "' "
				              "res=", Genode::Hex(res));
				delete dev_obj;
				return AE_OK;
			}

			Acpica::Buffer<char [8]> battery_name;
			AcpiGetName (sb, ACPI_SINGLE_NAME, &battery_name);
			if (ACPI_FAILURE(res)) {
				Genode::error("failed   - '", __func__, "' battery name "
				              "res=", Genode::Hex(res));
				return AE_OK;
			}

			Acpica::Buffer<ACPI_OBJECT> sta;
			res = AcpiEvaluateObjectTyped(sb, ACPI_STRING("_STA"), nullptr, &sta,
			                              ACPI_TYPE_INTEGER);
			if (ACPI_FAILURE(res)) {
				Genode::error("failed   - '", __func__, "' _STA "
				              "res=", Genode::Hex(res));
				return AE_OK;
			}

			/* ACPI spec - 10.2.2.1 _BIF (Battery Information) */
			Acpica::Buffer<char [512]> battery;
			res = AcpiEvaluateObjectTyped(sb, ACPI_STRING("_BIF"),
			                              nullptr, &battery,
			                              ACPI_TYPE_PACKAGE);
			ACPI_OBJECT * obj = reinterpret_cast<ACPI_OBJECT *>(battery.object);
			if (ACPI_FAILURE(res) || !obj) {
				Genode::error("failed   - '", __func__, "' _BIF "
				              "res=", Genode::Hex(res));
				return AE_OK;
			}

			unsigned long long const val = sta.object.Integer.Value;

			if (obj->Package.Count < 13 ||
			    obj->Package.Elements[0].Type != ACPI_TYPE_INTEGER ||
			    obj->Package.Elements[12].Type != ACPI_TYPE_STRING ||
			    obj->Package.Elements[11].Type != ACPI_TYPE_STRING ||
			    obj->Package.Elements[10].Type != ACPI_TYPE_STRING ||
			    obj->Package.Elements[9].Type != ACPI_TYPE_STRING)
			{
				Genode::log("detected - battery "
				            "'", Genode::Cstring(battery_name.object), "' - "
				            "unknown state (", Genode::Hex(val),
				            val & ACPI_STA_BATTERY_PRESENT ? "" : "(not present)",
				            ")");
				return AE_OK;
			}

			using Genode::Cstring;
			Genode::log("detected - battery "
			            "'", Cstring(battery_name.object), "' "
			            "type='",   Cstring(obj->Package.Elements[11].String.Pointer), "' "
			            "OEM='",    Cstring(obj->Package.Elements[12].String.Pointer), "' "
			            "state=",   Genode::Hex(val),
			                        val & ACPI_STA_BATTERY_PRESENT ? "" : "(not present)", " "
			            "model='",  Cstring(obj->Package.Elements[10].String.Pointer), "' "
			            "serial='", Cstring(obj->Package.Elements[9].String.Pointer), "'");

			return AE_OK;
		}

		void generate(Genode::Xml_generator &xml)
		{
			info(xml);
			status(xml);
		}

		void info(Genode::Xml_generator &xml)
		{
			/* ACPI spec - 10.2.2.1 _BIF (Battery Information) */
			Acpica::Buffer<char [512]> battery;
			ACPI_STATUS res = AcpiEvaluateObjectTyped(_sb, ACPI_STRING("_BIF"),
			                                          nullptr, &battery,
			                                          ACPI_TYPE_PACKAGE);
			ACPI_OBJECT * obj = reinterpret_cast<ACPI_OBJECT *>(battery.object);
			if (ACPI_FAILURE(res) || !obj || obj->Package.Count != 13) {
				Genode::error("failed   - '", __func__, "' _BIF res=", Genode::Hex(res));
				return; 
			}

			Acpica::Buffer<char [8]> battery_name;
			AcpiGetName (_sb, ACPI_SINGLE_NAME, &battery_name);
			if (ACPI_FAILURE(res))
				xml.node("name", [&] { xml.append("unknown"); });
			else
				xml.node("name", [&] { xml.append(battery_name.object); });

			const char * node_name[] = {
				"powerunit", "design_capacity", "last_full_capacity",
				"technology", "voltage", "warning_capacity", "low_capacity",
				"granularity1", "granularity2", "serial", "model", "type",
				"oem"
			};

			if (sizeof(node_name) / sizeof(node_name[0]) != obj->Package.Count)
				return;

			for (unsigned i = 0; i < 9; i++) {
				ACPI_OBJECT * v = &obj->Package.Elements[i];

				xml.node(node_name[i], [&] {
					if (v->Type != ACPI_TYPE_INTEGER) {
						xml.append("unknown");
						return;
					}

					xml.attribute("value", v->Integer.Value);

					if (i == 0)
						xml.append(v->Integer.Value == 0 ? "mW/mWh" :
						           v->Integer.Value == 1 ? "mA/mAh" :
						                                   "unknown");
					if (i == 3)
						xml.append(v->Integer.Value == 0 ? "primary" :
						           v->Integer.Value == 1 ? "secondary" :
						                                   "unknown");
				});
			}

			for (unsigned i = 9; i < obj->Package.Count; i++) {
				ACPI_OBJECT * v = &obj->Package.Elements[i];

				xml.node(node_name[i], [&] {

					if (v->Type != ACPI_TYPE_STRING)
						return;

					xml.append(v->String.Pointer);
				});
			}
		}

		void status(Genode::Xml_generator &xml)
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
				xml.node("status", [&] { xml.append("unknown"); });
			} else
				xml.node("status", [&] {
					xml.attribute("value", sta.object.Integer.Value);
					/* see "6.3.7 _STA" for more human readable decoding */
					if (!(sta.object.Integer.Value & ACPI_STA_BATTERY_PRESENT))
						xml.append("battery not present");
				});

			const char * node_name[] = {
				"state", "present_rate", "remaining_capacity",
				"present_voltage"
			};

			if (sizeof(node_name) / sizeof(node_name[0]) != obj->Package.Count)
				return;

			for (unsigned i = 0; i < obj->Package.Count; i++) {
				ACPI_OBJECT * v = &obj->Package.Elements[i];

				xml.node(node_name[i], [&] {
					if (v->Type != ACPI_TYPE_INTEGER) {
						xml.append("unknown");
						return;
					}

					xml.attribute("value", v->Integer.Value);

					if (i != 0)
						return;

					if (v->Integer.Value & 0x1) xml.append("discharging");
					if (v->Integer.Value & 0x2) xml.append("charging");
					if (v->Integer.Value & 0x4) xml.append("critical low");
				});
			}
		}
};
