/*
 * \brief  Handle ACPI LID device
 * \author Alexander Boettcher
 *
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

class Lid : Acpica::Callback<Lid> {

	private:

		Acpica::Reportstate * _report;
		UINT64 _lid_state = 0;
		UINT64 _lid_count = 0;

	public:

		Lid(void * report)
			: _report(reinterpret_cast<Acpica::Reportstate *>(report))
		{
			if (_report)
				_report->add_notify(this);
		}

		void handle(ACPI_HANDLE lid, UINT32 value)
		{
			Acpica::Buffer<ACPI_OBJECT> onoff;
			ACPI_STATUS res = AcpiEvaluateObjectTyped(lid, ACPI_STRING("_LID"),
			                                          nullptr, &onoff,
			                                          ACPI_TYPE_INTEGER);
			if (ACPI_FAILURE(res)) {
				PERR("failed   - '%s' res=0x%x _PSR", __func__, res);
				return;
			}

			PINF("%s - lid (%u)",
			     onoff.object.Integer.Value ? "open    " : "closed  ",
			     value);

			_lid_state = onoff.object.Integer.Value;
			_lid_count++;

			if (_report)
				_report->lid_event();
		}

		static ACPI_STATUS detect(ACPI_HANDLE lid, UINT32, void * report, void **)
		{
			Lid * obj = new (Genode::env()->heap()) Lid(report);

			ACPI_STATUS res = AcpiInstallNotifyHandler (lid, ACPI_DEVICE_NOTIFY,
			                                            handler, obj);
			if (ACPI_FAILURE(res)) {
				PDBG("failed   - %s res=0x%x LID adapter", __func__, res);
				delete obj;
				return AE_OK;
			}

			PINF("detected - lid");

			handler(lid, 0, obj);

			return AE_OK;
		}

		void generate(Genode::Xml_generator &xml)
		{
			xml.node("lid", [&] {
				xml.attribute("value", _lid_state);
				xml.attribute("count", _lid_count);
				if (_lid_state)
					xml.append("open");
				else
					xml.append("closed");
			});
		}
};
