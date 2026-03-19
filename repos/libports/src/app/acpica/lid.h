/*
 * \brief  Handle ACPI LID device
 * \author Alexander Boettcher
 *
 */

/*
 * Copyright (C) 2016-2026 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

class Lid : Acpica::Callback<Lid> {

	private:

		Acpica::Reportstate * _report;
		UINT64 _lid_state = 0;
		UINT64 _lid_count = 0;

		ACPI_HANDLE const _lid;

	public:

		Lid(Acpica::Reportstate * report, ACPI_HANDLE lid)
		: _report(report), _lid(lid)
		{
			if (_report)
				_report->add_notify(this);
		}

		void trigger_update() { handle(_lid, 0); }

		void handle(ACPI_HANDLE lid, UINT32 value)
		{
			Acpica::Buffer<ACPI_OBJECT> onoff;
			ACPI_STATUS res = AcpiEvaluateObjectTyped(lid, ACPI_STRING("_LID"),
			                                          nullptr, &onoff,
			                                          ACPI_TYPE_INTEGER);
			if (ACPI_FAILURE(res)) {
				Genode::error("failed   - '", __func__, "' "
				              "res=", Genode::Hex(res), " _PSR");
				return;
			}

			Genode::log(onoff.object.Integer.Value ? "open    " : "closed  ",
			            " - lid (", value, ")");

			_lid_state = onoff.object.Integer.Value;
			_lid_count++;

			if (_report)
				_report->lid_event();
		}

		static ACPI_STATUS detect(ACPI_HANDLE lid, UINT32, void * m, void **)
		{
			Acpica::Main * main = reinterpret_cast<Acpica::Main *>(m);
			Lid * obj = new (main->heap) Lid(main->report, lid);

			ACPI_STATUS res = AcpiInstallNotifyHandler (lid, ACPI_DEVICE_NOTIFY,
			                                            handler, obj);
			if (ACPI_FAILURE(res)) {
				Genode::log("failed   - ", __func__, " "
				            "res=", Genode::Hex(res), " LID adapter");
				delete obj;
				return AE_OK;
			}

			Genode::log("detected - lid");

			handler(lid, 0, obj);

			return AE_OK;
		}

		void generate(Genode::Generator &g)
		{
			g.node("lid", [&] {
				g.attribute("value", _lid_state);
				g.attribute("count", _lid_count);
				if (_lid_state)
					g.append_quoted("open");
				else
					g.append_quoted("closed");
			});
		}
};
