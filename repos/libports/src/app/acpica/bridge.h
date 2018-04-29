/*
 * \brief  Handle PCI Root bridge
 * \author Alexander Boettcher
 *
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

class Bridge {

	private:

		ACPI_HANDLE _bridge;

		unsigned bdf_bridge(ACPI_HANDLE bridge)
		{
			/* address (high word = device, low word = function) (6.1.1) */
			unsigned bridge_adr = 0;
			/* Base bus number (6.5.5) */
			unsigned bridge_bbn = 0;
			/* Segment object located under host bridge (6.5.6) */
			unsigned bridge_seg = 0;

			Acpica::Buffer<ACPI_OBJECT> adr;
			ACPI_STATUS res = AcpiEvaluateObjectTyped(bridge,
			                                          ACPI_STRING("_ADR"),
			                                          nullptr, &adr,
			                                          ACPI_TYPE_INTEGER);

			if (res != AE_OK) {
				Genode::error("could not detect address of bridge - ", res);
				return 0;
			} else
				bridge_adr = adr.object.Integer.Value;

			Acpica::Buffer<ACPI_OBJECT> bbn;
			res = AcpiEvaluateObjectTyped(bridge, ACPI_STRING("_BBN"),
			                              nullptr, &bbn, ACPI_TYPE_INTEGER);
			if (res != AE_OK) {
				Genode::warning("_BBN missing for bridge");
			} else
				bridge_bbn = bbn.object.Integer.Value;

			Acpica::Buffer<ACPI_OBJECT> seg;
			res = AcpiEvaluateObjectTyped(bridge, ACPI_STRING("_SEG"),
			                              nullptr, &seg, ACPI_TYPE_INTEGER);

			/* according to ACPI spec assume segment 0 if method unavailable */
			if (res == AE_OK)
				bridge_seg = seg.object.Integer.Value;

			unsigned const bridge_bdf = ((0xffffU & bridge_seg) << 16) |
			                            ((0x00ffU & bridge_bbn) << 8)  |
			                            (0xffU & ((bridge_adr >> 16) << 3)) |
			                            (bridge_adr & 0x7);

			return bridge_bdf;
		}

		void _gen_bridge(ACPI_HANDLE bridge, Genode::Xml_generator &xml,
		                 unsigned const bridge_bdf)
		{
			Acpica::Buffer<char [2 * 4096]> irqs;
			ACPI_STATUS res = AcpiGetIrqRoutingTable (bridge, &irqs);
			if (res != AE_OK) {
				Genode::error("buffer for PCI IRQ routing information to "
				              "small - ", irqs.Length, " required");
				return;
			}

			ACPI_PCI_ROUTING_TABLE *s = reinterpret_cast<ACPI_PCI_ROUTING_TABLE *>(irqs.Pointer);
			ACPI_PCI_ROUTING_TABLE *e = reinterpret_cast<ACPI_PCI_ROUTING_TABLE *>(reinterpret_cast<unsigned long>(&irqs) + irqs.Length);
			for (ACPI_PCI_ROUTING_TABLE *c = s; c < e && c->Length; ) {

				using Genode::Hex;
				using Genode::String;

				xml.node("routing", [&] () {
					xml.attribute("gsi", String<16>(Hex(c->SourceIndex)));
					xml.attribute("bridge_bdf", String<16>(Hex(bridge_bdf)));
					xml.attribute("device", String<16>(Hex((c->Address >> 16) & 0x1f)));
					xml.attribute("device_pin", String<16>(Hex(c->Pin)));
				});

				c = reinterpret_cast<ACPI_PCI_ROUTING_TABLE *>(reinterpret_cast<unsigned long>(c) + c->Length);
			}
		}

		void _sub_bridges(ACPI_HANDLE handle, Genode::Xml_generator &xml)
		{
			ACPI_STATUS res = AcpiEvaluateObject(handle, ACPI_STRING("_PRT"),
			                                     nullptr, nullptr);

			if (res != AE_OK)
				return;

			/* got another bridge, generate irq routing information to xml */
			Bridge::_gen_bridge(handle, xml, bdf_bridge(handle));

			ACPI_HANDLE child = nullptr;

			/* lookup next bridge behind the bridge */
			while (AE_OK == (res = AcpiGetNextObject(ACPI_TYPE_DEVICE, handle,
			                                         child, &child)))
			{
				_sub_bridges(child, xml);
			}
		}

	public:

		Bridge(void *, ACPI_HANDLE bridge)
		:
			 _bridge(bridge)
		{ }

		static ACPI_STATUS detect(ACPI_HANDLE bridge, UINT32, void * m,
		                          void **return_bridge);

		void generate(Genode::Xml_generator &xml)
		{
			unsigned const root_bridge_bdf = bdf_bridge(_bridge);

			xml.node("root_bridge", [&] () {
				xml.attribute("bdf", Genode::String<8>(Genode::Hex(root_bridge_bdf)));
			});

			/* irq routing information of this (pci root) bridge */
			_gen_bridge(_bridge, xml, root_bridge_bdf);

			/* lookup all pci-to-pci bridges and add irq routing information */
			_sub_bridges(_bridge, xml);
		}
};
