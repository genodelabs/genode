/*
 * \brief  Support some Fujitsu ACPI devices
 * \author Alexander Boettcher
 * \date   2021-01-06
 */

/*
 * Copyright (C) 2020-2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

class Fuj02e3 : public Acpica::Reporter, Acpica::Callback<Fuj02e3>
{
	private:

		enum {
			HID_FUJITSU_NOTIFY               = 0x80,
		};

		enum Softkeys {
			HID_FUJITSU_FLAG_RFKILL          = 1u << 5,
			HID_FUJITSU_FLAG_TOUCHPAD_TOGGLE = 1u << 26,
			HID_FUJITSU_FLAG_MICROFON_MUTE   = 1u << 29,

			HID_FUJITSU_FLAG_SOFTKEYS = HID_FUJITSU_FLAG_RFKILL |
			                            HID_FUJITSU_FLAG_TOUCHPAD_TOGGLE |
			                            HID_FUJITSU_FLAG_MICROFON_MUTE
		};

		enum Operation {
			HID_FUJITSU_FUNC_FLAGS  = 1u << 12,
			HID_FUJITSU_FUNC_BUTTON = HID_FUJITSU_FUNC_FLAGS | 2,
		};

		Acpica::Reportstate *_report;
		Genode::uint64_t     _features { 0 };

		struct Data
		{
			Genode::uint64_t count;
			Genode::uint64_t data;
			bool triggered;
		} _data[3] { };

		template <typename RESULT>
		ACPI_STATUS _call_acpi_function(ACPI_HANDLE const hid,
		                                enum Operation const function,
		                                RESULT &result, unsigned const op,
		                                unsigned const feature,
		                                unsigned const state)
		{
			ACPI_OBJECT_LIST para_in;
			ACPI_OBJECT      values[5];

			values[0].Type = ACPI_TYPE_INTEGER;
			values[0].Integer.Value = function;
			values[1].Type = ACPI_TYPE_INTEGER;
			values[1].Integer.Value = op;
			values[2].Type = ACPI_TYPE_INTEGER;
			values[2].Integer.Value = feature;
			values[3].Type = ACPI_TYPE_INTEGER;
			values[3].Integer.Value = state;
			values[4].Type = 0;
			values[4].Integer.Value = 0;

			para_in.Count   = 4;
			para_in.Pointer = values;

			return AcpiEvaluateObjectTyped(hid, ACPI_STRING("FUNC"),
			                               &para_in, &result,
			                               ACPI_TYPE_INTEGER);
		}

		template <typename RESULT>
		ACPI_STATUS device_features(ACPI_HANDLE const hid, RESULT &result)
		{
			return _call_acpi_function(hid, HID_FUJITSU_FUNC_FLAGS, result,
			                           0, 0, 0);
		}

		template <typename RESULT>
		ACPI_STATUS soft_keys(ACPI_HANDLE const hid, RESULT &result)
		{
			return _call_acpi_function(hid, HID_FUJITSU_FUNC_FLAGS, result,
			                           1, 0, 0);
		}

		template <typename RESULT>
		ACPI_STATUS read_button(ACPI_HANDLE const hid, RESULT &result)
		{
			return _call_acpi_function(hid, HID_FUJITSU_FUNC_BUTTON, result,
			                           1, 0, 0);
		}

	public:

		Fuj02e3(void *report)
		: _report(reinterpret_cast<Acpica::Reportstate *>(report))
		{
			if (_report)
				_report->add_notify(this);
		}

		void handle(ACPI_HANDLE const hid, UINT32 const value)
		{
			if (value != HID_FUJITSU_NOTIFY)
				return;

			Acpica::Buffer<ACPI_OBJECT> irb;
			ACPI_STATUS res = read_button(hid, irb);
			if (ACPI_SUCCESS(res) && irb.object.Integer.Value != 0)
				Genode::error("not implemented - irb value=",
				              Genode::Hex(irb.object.Integer.Value));

			if (_features & HID_FUJITSU_FLAG_SOFTKEYS) {
				Acpica::Buffer<ACPI_OBJECT> feature;
				ACPI_STATUS res = soft_keys(hid, feature);

				if (ACPI_SUCCESS(res))
				{
					UINT64 const value = feature.object.Integer.Value;

					if (value & HID_FUJITSU_FLAG_RFKILL) {
						_data[0].data = HID_FUJITSU_FLAG_RFKILL;
						_data[0].triggered = true;
						_data[0].count ++;
					}

					if (value & HID_FUJITSU_FLAG_TOUCHPAD_TOGGLE) {
						_data[1].data = HID_FUJITSU_FLAG_TOUCHPAD_TOGGLE;
						_data[1].triggered = true;
						_data[1].count ++;
					}

					if (value & HID_FUJITSU_FLAG_MICROFON_MUTE) {
						_data[2].data = HID_FUJITSU_FLAG_MICROFON_MUTE;
						_data[2].triggered = true;
						_data[2].count ++;
					}

					if (_report && (value & HID_FUJITSU_FLAG_SOFTKEYS))
						_report->hid_event();
				}
			}
		}

		static ACPI_STATUS detect(ACPI_HANDLE hid, UINT32, void *m, void **)
		{
			Acpica::Main *main = reinterpret_cast<Acpica::Main *>(m);
			Fuj02e3 *obj = new (main->heap) Fuj02e3(main->report);

			ACPI_STATUS res = AcpiInstallNotifyHandler(hid, ACPI_DEVICE_NOTIFY,
			                                           handler, obj);
			if (ACPI_FAILURE(res)) {
				Genode::log("failed   - ", __func__, " "
				            "res=", Genode::Hex(res), " Fujitsu adapter");
				delete obj;
				return AE_OK;
			}

			Genode::log("detected - Fujitsu HID");

			Acpica::Buffer<ACPI_OBJECT> features;
			res = obj->device_features(hid, features);
			if (ACPI_FAILURE(res)) {
				Genode::error("failed   - '", __func__, "' "
				              "res=", Genode::Hex(res), " features");
			}

			obj->_features = features.object.Integer.Value;

			return AE_OK;
		}

		void generate(Genode::Xml_generator &xml) override
		{
			xml.node("hid", [&] {
				xml.attribute("device", "Fuj02e3");

				for (unsigned i = 0; i < sizeof(_data) / sizeof(_data[0]); i++) {

					xml.node("data", [&] {
						xml.attribute("value", Genode::String<12>(Genode::Hex(_data[i].data)));
						xml.attribute("count", _data[i].count);
						if (_data[i].triggered) {
							xml.append("triggered");
							_data[i].triggered = false;
						}
					});
				}
			});
		}
};
