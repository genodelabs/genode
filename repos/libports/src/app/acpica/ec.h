/*
 * \brief  Handle ACPI Embedded Controller devices
 * \author Alexander Boettcher
 *
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */


#include <util/mmio.h>

class Ec : Acpica::Callback<Ec> {

	private:
		unsigned short ec_port_cmdsta;
		unsigned short ec_port_data;

		Genode::Io_port_connection * ec_cmdsta = nullptr;
		Genode::Io_port_connection * ec_data = nullptr;;

		ACPI_HANDLE gpe_block;

		Acpica::Reportstate * _report;

		/* 12.2.1 Embedded Controller Status, EC_SC (R) */
		struct State : Genode::Register<8> {
			struct Out_ful: Bitfield<0,1> { };
			struct In_ful : Bitfield<1,1> { };
			struct Sci_evt: Bitfield<5,1> { };
		};

		/* 12.3. Embedded Controller Command Set */
		enum { RD_EC = 0x80, WR_EC = 0x81, QR_EC = 0x84 };

		/* track data items reported by controller */
		struct Data : Genode::List<Data>::Element {
			Genode::uint64_t count;
			Genode::uint8_t data;
			bool triggered;

			Data(Genode::uint8_t d) : count(0), data(d), triggered(false) { }
		};
		Genode::List<Data> _list_data;

	public:

		Ec(void * report)
		:
			_report(reinterpret_cast<Acpica::Reportstate *>(report))
		{ }

		static UINT32 handler_gpe(ACPI_HANDLE dev, UINT32 gpe, void *context)
		{
			Ec * ec = reinterpret_cast<Ec *>(context);

			ACPI_GPE_EVENT_INFO * ev = AcpiEvGetGpeEventInfo(ec->gpe_block, gpe);
			if (!ev || !ec->ec_cmdsta || !ec->ec_data) {
				PERR("unknown GPE 0x%x", gpe);
				return AE_OK; /* GPE is disabled and must be enabled explicitly */
			}

			if ((ACPI_GPE_DISPATCH_TYPE (ev->Flags) != ACPI_GPE_DISPATCH_HANDLER) ||
			    !ev->Dispatch.Handler) {
				PERR("unknown dispatch type, GPE 0x%x, flags=0x%x type=0x%x",
				     gpe, ev->Flags, ACPI_GPE_DISPATCH_TYPE (ev->Flags));
				return AE_OK; /* GPE is disabled and must be enabled explicitly */
			}

			State::access_t state = ec->ec_cmdsta->inb(ec->ec_port_cmdsta);

			if (!State::Sci_evt::get(state)) {
				PERR("unknown status 0x%x", state);
				return ACPI_REENABLE_GPE; /* gpe is acked and re-enabled */
			}

			ec->ec_cmdsta->outb(ec->ec_port_cmdsta, QR_EC);
			do {
				state = ec->ec_cmdsta->inb(ec->ec_port_cmdsta);
			} while (!(State::Out_ful::get(state)));

			unsigned cnt = 0;
			unsigned char data;
			do {
				data = ec->ec_data->inb(ec->ec_port_data);
				state = ec->ec_cmdsta->inb(ec->ec_port_cmdsta);

				if (!ec->_report)
					PINF("ec event - status 0x%x data 0x%x round=%u", state,
					     data, ++cnt);
			} while (State::Out_ful::get(state));

			if (ec->_report) {
				Data * data_obj = ec->_list_data.first();
				for (; data_obj; data_obj = data_obj->next()) {
					if (data_obj->data == data)
						break;
				}

				if (!data_obj) {
					data_obj = new (Genode::env()->heap()) Data(data);
					ec->_list_data.insert(data_obj);
				}
				data_obj->count ++;
				data_obj->triggered = true;

				ec->_report->ec_event();
			}

			return ACPI_REENABLE_GPE; /* gpe is acked and re-enabled */
		}

		static ACPI_STATUS detect_io_ports(ACPI_RESOURCE *resource,
		                                   void *context)
		{
			Ec * ec = reinterpret_cast<Ec *>(context);

			if (resource->Type == ACPI_RESOURCE_TYPE_END_TAG)
				return AE_OK;

			if (resource->Type != ACPI_RESOURCE_TYPE_IO) {
				PWRN("unknown resource type %u", resource->Type);
				return AE_OK;
			}
/*
			PDBG("TYPE IO: IoDecode 0x%x, alignment 0x%x, AddressLen 0x%x "
			      "min 0x%x max 0x%x",
			      resource->Data.Io.IoDecode, resource->Data.Io.Alignment,
			      resource->Data.Io.AddressLength, resource->Data.Io.Minimum,
			      resource->Data.Io.Maximum);
*/
			/* first port is data, second is status/cmd */
			if (resource->Data.Io.AddressLength != 1)
				PERR("unsupported address length of %u",
				     resource->Data.Io.AddressLength);

			if (!ec->ec_data) {
				ec->ec_port_data = resource->Data.Io.Minimum;
				ec->ec_data = new (Genode::env()->heap()) Genode::Io_port_connection(ec->ec_port_data, 1);
			} else
			if (!ec->ec_cmdsta) {
				ec->ec_port_cmdsta = resource->Data.Io.Minimum;
				ec->ec_cmdsta = new (Genode::env()->heap()) Genode::Io_port_connection(ec->ec_port_cmdsta, 1);
			} else
				PERR("unknown io_port");

			return AE_OK;
		}

		static ACPI_STATUS handler_ec(UINT32 function,
		                              ACPI_PHYSICAL_ADDRESS phys_addr,
		                              UINT32 bitwidth, UINT64 *value, void *,
		                              void *ec_void)
		{
			unsigned const bytes = bitwidth / 8;
			/* bitwidth can be larger than 64bit - use char array */
			unsigned char *result = reinterpret_cast<unsigned char *>(value);

			if (bytes * 8 != bitwidth) {
				PERR("unsupport bit width of %u", bitwidth);
				return AE_BAD_PARAMETER;
			}

			Ec * ec = reinterpret_cast<Ec *>(ec_void);

			switch (function & ACPI_IO_MASK) {
			case ACPI_READ:
				for (unsigned i = 0; i < bytes; i++) {
					State::access_t state;

					/* write command */
					ec->ec_cmdsta->outb(ec->ec_port_cmdsta, RD_EC);
					do {
						state = ec->ec_cmdsta->inb(ec->ec_port_cmdsta);
					} while (State::In_ful::get(state));

					/* write address */
					ec->ec_data->outb(ec->ec_port_data, phys_addr + i);
					do {
						state = ec->ec_cmdsta->inb(ec->ec_port_cmdsta);
					} while (!(State::Out_ful::get(state)));

					/* read value */
					result[i] = ec->ec_data->inb(ec->ec_port_data);
				}
				return AE_OK;
			case ACPI_WRITE:
				for (unsigned i = 0; i < bytes; i++) {
					State::access_t state;

					/* write command */
					ec->ec_cmdsta->outb(ec->ec_port_cmdsta, WR_EC);
					do {
						state = ec->ec_cmdsta->inb(ec->ec_port_cmdsta);
					} while (State::In_ful::get(state));

					/* write address */
					ec->ec_data->outb(ec->ec_port_data, phys_addr + i);
					do {
						state = ec->ec_cmdsta->inb(ec->ec_port_cmdsta);
					} while (State::In_ful::get(state));

					/* write value */
					ec->ec_data->outb(ec->ec_port_data, result[i]);
					do {
						state = ec->ec_cmdsta->inb(ec->ec_port_cmdsta);
					} while (State::In_ful::get(state));
				}
				return AE_OK;
			}

			return AE_BAD_PARAMETER;
		}

		static ACPI_STATUS detect(ACPI_HANDLE ec, UINT32, void *report, void **)
		{
			Ec *ec_obj = new (Genode::env()->heap()) Ec(report);

			ACPI_STATUS res = AcpiWalkResources(ec, ACPI_STRING("_CRS"),
			                                    Ec::detect_io_ports, ec_obj);
			if (ACPI_FAILURE(res)) {
				PERR("failed   - '%s' _CRS res=0x%x", __func__, res);
				return AE_OK;
			}

			res = AcpiInstallAddressSpaceHandler(ec, ACPI_ADR_SPACE_EC,
			                                     handler_ec, nullptr,
			                                     ec_obj);
			if (ACPI_FAILURE(res)) {
				PERR("failed   - '%s' spacehandler res=0x%x", __func__, res);
				return AE_OK;
			}

			Acpica::Buffer<ACPI_OBJECT> sta;
			res = AcpiEvaluateObjectTyped(ec, ACPI_STRING("_GPE"), nullptr,
			                              &sta, ACPI_TYPE_INTEGER);
			if (ACPI_FAILURE(res)) {
				PERR("failed   - '%s' _STA res=0x%x", __func__, res);
				return AE_OK;
			}

			UINT32 gpe_to_enable = sta.object.Integer.Value;

			/* if ec_obj->gpe_block stays null - it's GPE0/GPE1 */
			res = AcpiGetGpeDevice(gpe_to_enable, &ec_obj->gpe_block);
			if (ACPI_FAILURE(res)) {
				PERR("failed   - '%s' get_device res=0x%x", __func__, res);
				return AE_OK;
			}

			res = AcpiInstallGpeHandler(ec_obj->gpe_block, gpe_to_enable,
			                            ACPI_GPE_LEVEL_TRIGGERED, handler_gpe,
			                            ec_obj);
			if (ACPI_FAILURE(res)) {
				PERR("failed   - '%s' install_device res=0x%x", __func__, res);
				return AE_OK;
			}

			res = AcpiEnableGpe (ec_obj->gpe_block, gpe_to_enable);
			if (ACPI_FAILURE(res)) {
				PERR("failed   - '%s' enable_gpe res=0x%x", __func__, res);
				return AE_OK;
			}

			PINF("detected - ec");

			if (ec_obj->_report)
				ec_obj->_report->add_notify(ec_obj);

			return AE_OK;
		}

		void generate(Genode::Xml_generator &xml)
		{
			Data * data_obj = _list_data.first();
			for (; data_obj; data_obj = data_obj->next()) {
				xml.node("data", [&] {
					xml.attribute("value", data_obj->data);
					xml.attribute("count", data_obj->count);
					if (data_obj->triggered) {
						xml.append("triggered");
						data_obj->triggered = false;
					}
				});
			}
		}
};
