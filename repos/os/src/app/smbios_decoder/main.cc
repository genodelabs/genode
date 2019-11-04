/*
 * \brief  Decode information from SMBIOS table and report it as XML
 * \author Martin Stein
 * \date   2019-07-04
 *
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <smbios/smbios.h>
#include <base/attached_rom_dataspace.h>
#include <os/reporter.h>
#include <base/component.h>

using namespace Genode;


class Table
{
	private:

		/* two 0..255 numbers, 1 dot, terminating null*/
		using Version_2_string = String<3 * 2 + 1 + 1>;

		/* address value in hex. 2 chars prefix, terminating null */
		using Addr_string = String<sizeof(addr_t) * 2 + 2 + 1>;

		/* 64bit value, 2 char unit, terminating null */
		using Size_string = String<20 + 2 + 1>;

		/* 16 two-digit hex values, 4 hyphen, terminating null */
		using Uuid_string = String<2 * 16 + 4 + 1>;

		/* 2 digit hex value with padding but w/o prefix */
		struct Uuid_hex : Hex
		{
			Uuid_hex(char digit) : Hex(digit, Hex::OMIT_PREFIX, Hex::PAD) { }
		};

		addr_t  const _base;
		addr_t  const _end;
		addr_t  const _verbose;
		uint8_t       _version_major { (uint8_t)~0 };
		uint8_t       _version_minor { (uint8_t)~0 };

		void _warn(char const *msg) const;

		const char *_string_set_item(Smbios_structure const &header,
		                             uint8_t                 idx) const;

		char const *_bios_character_0(uint8_t idx) const;

		char const *_system_wake_up_type(uint8_t idx) const;

		const char *_base_board_type(uint8_t idx) const;

		char const *_base_board_feature(uint8_t idx) const;

		void _report_base_board_features(Reporter::Xml_generator &xml,
		                                 uint8_t                  code) const;

		void _report_base_board_handles(Reporter::Xml_generator &xml,
		                                uint8_t                  count,
		                                uint8_t           const *data) const;

		void _report_base_board(Reporter::Xml_generator &xml,
		                        Smbios_structure  const &header) const;

		void _report_bios_character_0(Reporter::Xml_generator &xml,
		                              uint64_t                 code) const;

		char const *_bios_character_1(uint8_t idx) const;

		void _report_bios_character_1(Reporter::Xml_generator &xml,
		                              uint8_t                  code) const;

		char const *_bios_character_2(uint8_t idx) const;

		void _report_bios_character_2(Reporter::Xml_generator &xml,
		                              uint8_t                  code) const;

		void _report_bios_rom_size(Reporter::Xml_generator &xml,
		                           uint8_t                  code_1,
		                           uint16_t                 code_2) const;

		void _report_string(Reporter::Xml_generator &xml,
		                    char              const *type,
		                    char              const *value) const;

		void _report_string_set_item(Reporter::Xml_generator &xml,
		                             Smbios_structure  const &header,
		                             char              const *type,
		                             unsigned                 idx) const;

		void _report_system_uuid(Reporter::Xml_generator &xml,
		                         uint8_t const           *data) const;

		void _report_system(Reporter::Xml_generator &xml,
		                    Smbios_structure  const &header) const;

		void _report_bios(Reporter::Xml_generator &xml,
		                  Smbios_structure  const &header) const;

		void _report_smbios_struct(Reporter::Xml_generator &xml,
		                           Smbios_structure  const &smbios_struct) const;

		void _report_smbios_structs(Reporter::Xml_generator &xml,
		                            Dmi_entry_point const   &smbios_ep) const;

		void _report_dmi_ep(Reporter::Xml_generator &xml,
		                    Dmi_entry_point const   &ep) const;

		void _report_smbios_ep(Reporter::Xml_generator  &xml,
		                       Smbios_entry_point const &smbios_ep) const;

	public:

		Table(addr_t base,
		      size_t size,
		      bool   verbose)
		:
			_base    { base },
			_end     { base + size },
			_verbose { verbose }
		{ }

		void report(Reporter::Xml_generator &xml) const;
};


class Main
{
	private:

		Env                    &_env;
		Attached_rom_dataspace  _config_ds     { _env, "config" };
		Xml_node                _config        { _config_ds.xml() };
		bool             const  _verbose       { _config.attribute_value("verbose", false) };
		Expanding_reporter      _reporter      { _env, "result", "result" };
		Attached_rom_dataspace  _table_ds      { _env, "smbios_table" };
		Signal_handler<Main>    _table_ds_sigh { _env.ep(), *this, &Main::_handle_table_ds };
		Constructible<Table>    _table         { };

		void _handle_table_ds();

	public:

		Main(Env &env);
};


/***************
 ** Component **
 ***************/

void Component::construct(Genode::Env &env)
{
	static Main main { env };
}


/**********
 ** Main **
 **********/

Main::Main(Env &env) : _env { env }
{
	_table_ds.sigh(_table_ds_sigh);
	_handle_table_ds();
}


void Main::_handle_table_ds()
{
	_table_ds.update();
	if (!_table_ds.valid()) {
		return;
	}
	_table.construct((addr_t)_table_ds.local_addr<int>(), _table_ds.size(), _verbose);
	_reporter.generate([&] (Reporter::Xml_generator &xml) {
		_table->report(xml);
	});
}


/***********
 ** Table **
 ***********/

void Table::_report_string(Reporter::Xml_generator &xml,
                           char              const *type,
                           char              const *value) const
{
	xml.node(type, [&] () { xml.attribute("value", value); });
}


const char *Table::_string_set_item(Smbios_structure const &header,
                                    uint8_t                 idx) const
{
	if (idx == 0) {
		return "[not specified]";
	}
	uint8_t const *data { (uint8_t *)&header };
	char const *result = (char *)data + header.length;
	while (idx > 1 && *result) {
		result += strlen(result);
		result++;
		idx--;
	}
	if (!*result) {
		return "[bad index]";
	}
	return result;
}


void Table::_warn(char const *msg) const
{
	if (!_verbose) {
		return;
	}
	warning(msg);
}


char const *Table::_system_wake_up_type(uint8_t idx) const
{
	switch (idx) {
	case 0:  return "Reserved";
	case 1:  return "Other";
	case 2:  return "Unknown";
	case 3:  return "APM Timer";
	case 4:  return "Modem Ring";
	case 5:  return "LAN Remote";
	case 6:  return "Power Switch";
	case 7:  return "PCI PME#";
	case 8:  return "AC Power Restored";
	default: return "[out of spec]";
	}
}


char const *Table::_bios_character_0(uint8_t idx) const
{
	switch (idx) {
	case  4: return "ISA is supported";
	case  5: return "MCA is supported";
	case  6: return "EISA is supported";
	case  7: return "PCI is supported";
	case  8: return "PC Card (PCMCIA) is supported";
	case  9: return "PNP is supported";
	case 10: return "APM is supported";
	case 11: return "BIOS is upgradeable";
	case 12: return "BIOS shadowing is allowed";
	case 13: return "VLB is supported";
	case 14: return "ESCD support is available";
	case 15: return "Boot from CD is supported";
	case 16: return "Selectable boot is supported";
	case 17: return "BIOS ROM is socketed";
	case 18: return "Boot from PC Card (PCMCIA) is supported";
	case 19: return "EDD is supported";
	case 20: return "Japanese floppy for NEC 9800 1.2 MB is supported (int 13h)";
	case 21: return "Japanese floppy for Toshiba 1.2 MB is supported (int 13h)";
	case 22: return "5.25&quot;/360 kB floppy services are supported (int 13h)";
	case 23: return "5.25&quot;/1.2 MB floppy services are supported (int 13h)";
	case 24: return "3.5&quot;/720 kB floppy services are supported (int 13h)";
	case 25: return "3.5&quot;/2.88 MB floppy services are supported (int 13h)";
	case 26: return "Print screen service is supported (int 5h)";
	case 27: return "8042 keyboard services are supported (int 9h)";
	case 28: return "Serial services are supported (int 14h)";
	case 29: return "Printer services are supported (int 17h)";
	case 30: return "CGA/mono video services are supported (int 10h)";
	case 31: return "NEC PC-98";
	default: return "[bad index]";
	}
}


char const *Table::_bios_character_1(uint8_t idx) const
{
	switch (idx) {
	case  0: return "ACPI is supported";
	case  1: return "USB legacy is supported";
	case  2: return "AGP is supported";
	case  3: return "I2O boot is supported";
	case  4: return "LS-120 boot is supported";
	case  5: return "ATAPI Zip drive boot is supported";
	case  6: return "IEEE 1394 boot is supported";
	case  7: return "Smart battery is supported";
	default: return "[bad index]";
	}
}


char const *Table::_bios_character_2(uint8_t idx) const
{
	switch (idx) {
	case  0: return "BIOS boot specification is supported";
	case  1: return "Function key-initiated network boot is supported";
	case  2: return "Targeted content distribution is supported";
	case  3: return "UEFI is supported";
	case  4: return "System is a virtual machine";
	default: return "[bad index]";
	}
}


char const *Table::_base_board_feature(uint8_t idx) const
{
	switch (idx) {
	case  0: return "Board is a hosting board";
	case  1: return "Board requires at least one daughter board";
	case  2: return "Board is removable";
	case  3: return "Board is replaceable";
	case  4: return "Board is hot swappable";
	default: return "[bad index]";
	}
}


void Table::_report_base_board_features(Reporter::Xml_generator &xml,
                                        uint8_t                  code) const
{
	if ((code & 0x1f) == 0) {
		_report_string(xml, "feature", "[none]");
		return;
	}
	for (uint8_t idx = 0; idx < 5; idx++) {
		if (code & (1 << idx)) {
			_report_string(xml, "feature", _base_board_feature(idx));
		}
	}
}


void Table::_report_bios_character_0(Reporter::Xml_generator &xml,
                                     uint64_t                 code) const
{
	if (code & (1 << 3)) {
		xml.node("characteristic", [&] () {
			xml.attribute("value", "BIOS characteristics not supported");
		});
		return;
	}
	for (uint8_t idx = 4; idx <= 31; idx++) {
		if ((code & (1 << idx)) == 0) {
			continue;
		}
		xml.node("characteristic", [&] () {
			xml.attribute("value", _bios_character_0(idx));
		});
	}
}


void Table::_report_bios_character_1(Reporter::Xml_generator &xml,
                                     uint8_t                  code) const
{
	for (uint8_t idx = 0; idx <= 7; idx++) {
		if ((code & (1 << idx)) == 0) {
			continue;
		}
		xml.node("characteristic", [&] () {
			xml.attribute("value", _bios_character_1(idx));
		});
	}
}


void Table::_report_bios_character_2(Reporter::Xml_generator &xml,
                                     uint8_t                  code) const
{
	for (uint8_t idx = 0; idx <= 4; idx++) {
		if ((code & (1 << idx)) == 0) {
			continue;
		}
		xml.node("characteristic", [&] () {
			xml.attribute("value", _bios_character_2(idx));
		});
	}
}


void Table::_report_bios_rom_size(Reporter::Xml_generator &xml,
                                  uint8_t                  code_1,
                                  uint16_t                 code_2) const
{
	xml.node("rom-size", [&] () {
		if (code_1 != 0xff) {
			xml.attribute("value", Size_string(((size_t)code_1 + 1) << 6, " KB"));
			return;
		}
		switch (code_2 >> 14) {
		case  0: xml.attribute("value", Size_string(code_2 & 0x3fff, " MB")); return;
		case  1: xml.attribute("value", Size_string(code_2 & 0x3fff, " GB")); return;
		default: xml.attribute("value", "[bad unit]");                       return;
		}
	});
}


void Table::_report_string_set_item(Reporter::Xml_generator &xml,
                                    Smbios_structure  const &header,
                                    char              const *type,
                                    unsigned                 idx) const
{
	uint8_t const *data { (uint8_t *)&header };
	_report_string(xml, type, _string_set_item(header, data[idx]));
}


void Table::_report_bios(Reporter::Xml_generator &xml,
                         Smbios_structure  const &header) const
{
	uint8_t const *data { (uint8_t *)&header };
	xml.attribute("description", "BIOS Information");
	if (header.length < 18) {
		_warn("SMBIOS BIOS structure has bad length");
		return;
	}
	_report_string_set_item(xml, header, "vendor",       4);
	_report_string_set_item(xml, header, "version",      5);
	_report_string_set_item(xml, header, "release-date", 8);
	{
		addr_t const code { *(uint16_t const *)(data + 6) };
		if (code) {
			xml.node("address", [&] () {
				xml.attribute("value", Addr_string(Hex(code << 4))); });

			xml.node("runtime-size", [&] () {
				xml.attribute("value", (0x10000 - code) << 4); });
		}
	}
	{
		uint16_t code_2;
		if (header.length < 26) {
			code_2 = 16;
		} else {
			code_2 = *(uint16_t const *)(data + 24);
		}
		_report_bios_rom_size(xml, data[9], code_2);
	}
	_report_bios_character_0(xml, *(uint64_t const *)(data + 10));
	if (header.length < 0x13) {
		return; }

	_report_bios_character_1(xml, data[0x12]);
	if (header.length < 0x14) {
		return; }

	_report_bios_character_2(xml, data[0x13]);
	if (header.length < 0x18) {
		return; }

	if (data[20] != 0xff && data[21] != 0xff) {
		xml.node("bios-revision", [&] () {
			xml.attribute("value", Version_2_string(data[20], ".", data[21]));
		});
	}
	if (data[22] != 0xff && data[23] != 0xff) {
		xml.node("firmware-revision", [&] () {
			xml.attribute("value", Version_2_string(data[22], ".", data[23]));
		});
	}
}


void Table::_report_system_uuid(Reporter::Xml_generator &xml,
                                uint8_t const           *data) const
{
	bool only_zeros { true };
	bool only_ones  { true };
	for (unsigned off = 0; off < 16 && (only_zeros || only_ones); off++)
	{
		if (data[off] != 0x00) {
			only_zeros = false;
		}
		if (data[off] != 0xff) {
			only_ones = false;
		}
	}

	xml.node("uuid", [&] () {
		if (only_ones) {
			xml.attribute("value", "[not present]");
			return;
		}
		if (only_zeros) {
			xml.attribute("value", "[not settable]");
			return;
		}
		if ( _version_major >  2 ||
		    (_version_major == 2 && _version_minor >= 6))
		{
			xml.attribute("value", Uuid_string(
				Uuid_hex(data[3]), Uuid_hex(data[2]),
				Uuid_hex(data[1]), Uuid_hex(data[0]),
				"-",
				Uuid_hex(data[5]), Uuid_hex(data[4]),
				"-",
				Uuid_hex(data[7]), Uuid_hex(data[6]),
				"-",
				Uuid_hex(data[8]), Uuid_hex(data[9]),
				"-",
				Uuid_hex(data[10]), Uuid_hex(data[11]),
				Uuid_hex(data[12]), Uuid_hex(data[13]),
				Uuid_hex(data[14]), Uuid_hex(data[15])));

		} else {
			xml.attribute("value", Uuid_string(
				Uuid_hex(data[0]), Uuid_hex(data[1]),
				Uuid_hex(data[2]), Uuid_hex(data[3]),
				"-",
				Uuid_hex(data[4]), Uuid_hex(data[5]),
				"-",
				Uuid_hex(data[6]), Uuid_hex(data[7]),
				"-",
				Uuid_hex(data[8]), Uuid_hex(data[9]),
				"-",
				Uuid_hex(data[10]), Uuid_hex(data[11]),
				Uuid_hex(data[12]), Uuid_hex(data[13]),
				Uuid_hex(data[14]), Uuid_hex(data[15])));
		}
	});
}


const char *Table::_base_board_type(uint8_t idx) const
{
	switch (idx) {
	case  1: return "Unknown";
	case  2: return "Other";
	case  3: return "Server Blade";
	case  4: return "Connectivity Switch";
	case  5: return "System Management Module";
	case  6: return "Processor Module";
	case  7: return "I/O Module";
	case  8: return "Memory Module";
	case  9: return "Daughter Board";
	case 10: return "Motherboard";
	case 11: return "Processor+Memory Module";
	case 12: return "Processor+I/O Module";
	case 13: return "Interconnect Board";
	default: return "[out of spec]";
	}
}


void Table::_report_base_board_handles(Reporter::Xml_generator &xml,
                                       uint8_t                  count,
                                       uint8_t           const *data) const
{
	for (uint8_t idx = 0; idx < count; idx++) {
		xml.node("contained-object-handle", [&] () {
			uint8_t const *value { data + sizeof(uint16_t) * idx };
			xml.attribute("value", Addr_string(*(uint16_t const *)value));
		});
	}
}


void Table::_report_base_board(Reporter::Xml_generator &xml,
                               Smbios_structure  const &header) const
{
	xml.attribute("name", "Base Board Information");
	if (header.length < 8) {
		return; }

	_report_string_set_item(xml, header, "manufacturer",  4);
	_report_string_set_item(xml, header, "product-name",  5);
	_report_string_set_item(xml, header, "version",       6);
	_report_string_set_item(xml, header, "serial-number", 7);
	if (header.length < 9) {
		return; }

	_report_string_set_item(xml, header, "asset-tag", 8);
	if (header.length < 10) {
		return; }

	uint8_t const *data { (uint8_t *)&header };
	_report_base_board_features(xml, data[9]);
	if (header.length < 14) {
		return; }

	_report_string_set_item(xml, header, "location-in-chassis", 10);

	xml.node("chassis-handle", [&] () {
		xml.attribute("value", *(uint16_t const *)(data + 11)); });

	_report_string(xml, "type", _base_board_type(data[13]));
	if (header.length < 15) {
		return; }

	if (header.length < 15 + data[14] * sizeof(uint16_t)) {
		return; }

	_report_base_board_handles(xml, data[14], data + 15);
}


void Table::_report_system(Reporter::Xml_generator &xml,
                           Smbios_structure  const &header) const
{
	uint8_t const *data { (uint8_t *)&header };
	xml.attribute("description", "System Information");
	if (header.length < 8) {
		return; }

	_report_string_set_item(xml, header, "manufacturer",  4);
	_report_string_set_item(xml, header, "product-name",  5);
	_report_string_set_item(xml, header, "version",       6);
	_report_string_set_item(xml, header, "serial-number", 7);
	if (header.length < 25) {
		return; }

	_report_system_uuid(xml, data + 8);
	_report_string(xml, "wake-up-type", _system_wake_up_type(data[24]));
	if (header.length < 27) {
		return; }

	_report_string_set_item(xml, header, "sku-number", 25);
	_report_string_set_item(xml, header, "family",     26);
}


void Table::_report_smbios_struct(Reporter::Xml_generator &xml,
                                  Smbios_structure  const &smbios_struct) const
{
	xml.node("structure", [&] () {

		xml.attribute("type",   smbios_struct.type);
		xml.attribute("length", smbios_struct.length);
		xml.attribute("handle", smbios_struct.handle);

		switch (smbios_struct.type) {
		case Smbios_structure::BIOS:       _report_bios      (xml, smbios_struct); break;
		case Smbios_structure::SYSTEM:     _report_system    (xml, smbios_struct); break;
		case Smbios_structure::BASE_BOARD: _report_base_board(xml, smbios_struct); break;
		default: _warn("structure type not supported");                            break;
		}
	});
}


void Table::_report_smbios_structs(Reporter::Xml_generator &xml,
                                   Dmi_entry_point   const &ep) const
{
	Smbios_structure *smbios_struct { (Smbios_structure *)(
		(addr_t)&ep + ep.LENGTH) };

	for (uint16_t idx = 0; idx < ep.nr_of_structs; idx++) {

		if ((addr_t)smbios_struct + sizeof(*smbios_struct) > _end) {
			_warn("SMBIOS structure header exceeds ROM");
			break;
		} else if ((addr_t)smbios_struct + smbios_struct->length > _end) {
			_warn("SMBIOS structure body exceeds ROM");
			break;
		}
		_report_smbios_struct(xml, *smbios_struct);

		/* seek next SMBIOS structure */
		bool           next_exceeds_rom { false };
		uint8_t const *next             { (uint8_t *)(
			(addr_t)smbios_struct + smbios_struct->length) };

		while (1) {
			if ((addr_t)next + 2 * sizeof(*next) > _end) {
				next_exceeds_rom = true;
				break;
			}
			if (next[0] == 0 && next[1] == 0) {
				next += 2;
				break;
			}
			next++;
		}
		if (next_exceeds_rom) {
			_warn("SMBIOS structure string-set exceeds ROM");
			break;
		}
		smbios_struct = (Smbios_structure *)next;
	}
}


void Table::_report_smbios_ep(Reporter::Xml_generator  &xml,
                              Smbios_entry_point const &smbios_ep) const
{
	/* fix weird versions reported by some systems */
	uint8_t _version_major = smbios_ep.version_major;
	uint8_t _version_minor = smbios_ep.version_minor;
	if ((_version_major == 2 && _version_minor == 31) ||
	    (_version_major == 2 && _version_minor == 33))
	{
		_warn("fixed weird SMBIOS version");
		_version_major = 2;
		_version_minor = 3;

	} else if (_version_major == 2 && _version_minor == 51) {

		_warn("fixed weird SMBIOS version");
		_version_major = 2;
		_version_minor = 6;
	}
	xml.node("smbios", [&] () {
		xml.attribute("version",
			Version_2_string(_version_major, ".", _version_minor));

		xml.attribute("structures",      smbios_ep.nr_of_structs);
		xml.attribute("structures-size", smbios_ep.struct_table_length);

		_report_smbios_structs(xml, smbios_ep.dmi_ep());
	});
}


void Table::_report_dmi_ep(Reporter::Xml_generator &xml,
                           Dmi_entry_point   const &ep) const
{
	/* fix weird versions reported by some systems */
	uint8_t ver_maj { (uint8_t)(ep.bcd_revision >> 4) };
	uint8_t ver_min { (uint8_t)(ep.bcd_revision & 0xf) };
	xml.node("dmi", [&] () {
		xml.attribute("version",
			Version_2_string(ver_maj, ".", ver_min));

		xml.attribute("structures",      ep.nr_of_structs);
		xml.attribute("structures-size", ep.struct_table_length);

		_report_smbios_structs(xml, ep);
	});
}


void Table::report(Reporter::Xml_generator &xml) const
{

	/* check if entry point is valid and of which type it is */
	char const *const anchor_string { (char *)_base };
	if (((addr_t)anchor_string + 5 * sizeof(*anchor_string)) > _end) {
		_warn("anchor string of entry point exceeds ROM");
	} else if (String<5>(anchor_string) == "_SM_") {

		/* it's an SMBIOS entry point */
		Smbios_entry_point const &smbios_ep {
			*(Smbios_entry_point *)anchor_string };

		/* check if SMBIOS entry point is valid */
		if ((addr_t)&smbios_ep + sizeof(smbios_ep) > _end) {
			_warn("SMBIOS entry point exceeds ROM");
		} else if (!smbios_ep.length_valid()) {
			_warn("SMBIOS entry point has bad length");
		} else if (!smbios_ep.checksum_correct()) {
			_warn("SMBIOS entry point has bad checksum");
		} else if (String<6>((char const *)&smbios_ep.interm_anchor_string) !=
		           "_DMI_")
		{
			_warn("SMBIOS entry point has bad intermediate anchor string");
		} else if (!smbios_ep.interm_checksum_correct()) {
			_warn("SMBIOS entry point has bad intermediate checksum");
		} else {

			/* report information from SMBIOS entry point */
			_report_smbios_ep(xml, smbios_ep);
		}
	} else if (String<6>(anchor_string) == "_SM3_") {
		_warn("SMBIOS3 entry point found, not supported");

	} else if (String<6>(anchor_string) == "_DMI_") {

		Dmi_entry_point const &ep { *(Dmi_entry_point *)anchor_string };

		if (!ep.checksum_correct()) {
			warning("DMI entry point has bad checksum");
		} else {
			_report_dmi_ep(xml, ep);
		}
	} else {
		_warn("entry point has bad anchor string");
	}
}
