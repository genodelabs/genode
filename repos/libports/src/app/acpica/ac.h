/*
 * \brief  Handle ACPI AC adapter devices
 * \author Alexander Boettcher
 *
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

class Ac : Acpica::Callback<Ac> {

	private:

		Acpica::Reportstate * _report;
		UINT64 _ac_state = 0;
		UINT64 _ac_count = 0;

	public:

		Ac(void * report)
			: _report(reinterpret_cast<Acpica::Reportstate *>(report))
		{
			if (_report)
				_report->add_notify(this);
		}

		void handle(ACPI_HANDLE ac, UINT32 value)
		{
			Acpica::Buffer<ACPI_OBJECT> onoff;
			ACPI_STATUS res = AcpiEvaluateObjectTyped(ac, ACPI_STRING("_PSR"),
			                                          nullptr, &onoff,
			                                          ACPI_TYPE_INTEGER);
			if (ACPI_FAILURE(res)) {
				PDBG("failed   - res=0x%x _PSR", res);
				return;
			}

			_ac_state = onoff.object.Integer.Value;
			_ac_count++;

			PINF("%s - ac (%u)",
			     _ac_state == 0 ? "offline " :
			     _ac_state == 1 ? "online  " : "unknown ",
			     value);

			if (_report)
				_report->ac_event();
		}

		static ACPI_STATUS detect(ACPI_HANDLE ac, UINT32, void * report, void **)
		{
			Ac * obj = new (Genode::env()->heap()) Ac(report);

			ACPI_STATUS res = AcpiInstallNotifyHandler (ac, ACPI_DEVICE_NOTIFY,
			                                            handler, obj);
			if (ACPI_FAILURE(res)) {
				PERR("failed   - '%s' res=0x%x", __func__, res);
				delete obj;
				return AE_OK;
			}

			PINF("detected - ac");

			handler(ac, 0, obj);

			return AE_OK;
		}

		void generate(Genode::Xml_generator &xml)
		{
			xml.attribute("value", _ac_state);
			xml.attribute("count", _ac_count);

			if (_ac_state == 0)
				xml.append("offline");
			else if (_ac_state == 1)
				xml.append("online");
			else
				xml.append("unknown");
		}
};
