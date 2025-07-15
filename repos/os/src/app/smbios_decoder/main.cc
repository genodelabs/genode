/*
 * \brief  Decode information from SMBIOS table and report it
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
using namespace Genode::Smbios;

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

		Span const _mem;

		bool const _verbose;

		uint8_t _version_major { (uint8_t)~0 };
		uint8_t _version_minor { (uint8_t)~0 };

		void _warn(auto &&... args) const;

		char const * _base_board_feature(unsigned idx) const;
		char const * _base_board_type(unsigned idx) const;
		char const * _bios_character_0(unsigned idx) const;
		char const * _bios_character_1(unsigned idx) const;
		char const * _bios_character_2(unsigned idx) const;
		char const * _string_set_item(Header const &, unsigned idx) const;
		char const * _system_wake_up_type(unsigned idx) const;

		void _report_base_board_features(Generator &, uint8_t code) const;
		void _report_base_board_handles(Generator &, uint8_t count, uint8_t const *data) const;
		void _report_base_board(Generator &, Header const &) const;
		void _report_bios_character_0(Generator &, uint64_t code) const;
		void _report_bios_character_1(Generator &, uint8_t code) const;
		void _report_bios_character_2(Generator &, uint8_t code) const;
		void _report_bios_rom_size(Generator &, uint8_t code_1, uint16_t code_2) const;
		void _report_system_uuid(Generator &, uint8_t const *data) const;
		void _report_string(Generator &, char const *type, char const *value) const;

		void _report_string_set_item(Generator &, Header const &,
		                             char const *type, unsigned idx) const;

		void _report_system(Generator &, Header const &) const;
		void _report_bios(Generator &, Header const &) const;
		void _report_one_struct(Generator &, Header const &) const;
		void _report_structs(Generator &, Span const &) const;

		void _report_dmi(Generator &, Dmi_entry_point const &);
		void _report_v2(Generator &, V2_entry_point const &);
		void _report_v3(Generator &, V3_entry_point const &);

	public:

		Table(char const *base, size_t size, bool verbose)
		: _mem(base, size), _verbose(verbose)
		{ }

		void report(Generator &);
};


void Table::_report_string(Generator &g,
                           char const *type,
                           char const *value) const
{
	g.node(type, [&] { g.attribute("value", value); });
}


const char *Table::_string_set_item(Header const &header, unsigned idx) const
{
	if (idx == 0)
		return "[not specified]";

	uint8_t const *data { (uint8_t *)&header };
	char const *result = (char *)data + header.length;
	while (idx > 1 && *result) {
		result += strlen(result);
		result++;
		idx--;
	}
	if (!*result)
		return "[bad index]";

	return result;
}


void Table::_warn(auto &&... args) const
{
	if (_verbose)
		warning(args...);
}


char const *Table::_system_wake_up_type(unsigned idx) const
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


char const *Table::_bios_character_0(unsigned idx) const
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


char const *Table::_bios_character_1(unsigned idx) const
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


char const *Table::_bios_character_2(unsigned idx) const
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


char const *Table::_base_board_feature(unsigned idx) const
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


void Table::_report_base_board_features(Generator &g, uint8_t code) const
{
	if ((code & 0x1f) == 0) {
		_report_string(g, "feature", "[none]");
		return;
	}
	for (unsigned idx = 0; idx < 5; idx++) {
		if (code & (1 << idx)) {
			_report_string(g, "feature", _base_board_feature(idx));
		}
	}
}


void Table::_report_bios_character_0(Generator &g, uint64_t code) const
{
	if (code & (1 << 3)) {
		g.node("characteristic", [&] {
			g.attribute("value", "BIOS characteristics not supported");
		});
		return;
	}
	for (unsigned idx = 4; idx <= 31; idx++) {
		if ((code & (1 << idx)) == 0) {
			continue;
		}
		g.node("characteristic", [&] {
			g.attribute("value", _bios_character_0(idx));
		});
	}
}


void Table::_report_bios_character_1(Generator &g, uint8_t code) const
{
	for (unsigned idx = 0; idx <= 7; idx++) {
		if ((code & (1 << idx)) == 0) {
			continue;
		}
		g.node("characteristic", [&] {
			g.attribute("value", _bios_character_1(idx));
		});
	}
}


void Table::_report_bios_character_2(Generator &g, uint8_t code) const
{
	for (unsigned idx = 0; idx <= 4; idx++) {
		if ((code & (1 << idx)) == 0) {
			continue;
		}
		g.node("characteristic", [&] {
			g.attribute("value", _bios_character_2(idx));
		});
	}
}


void Table::_report_bios_rom_size(Generator &g,
                                  uint8_t  code_1,
                                  uint16_t code_2) const
{
	g.node("rom-size", [&] {
		if (code_1 != 0xff) {
			g.attribute("value", Size_string(((size_t)code_1 + 1) << 6, " KB"));
			return;
		}
		switch (code_2 >> 14) {
		case  0: g.attribute("value", Size_string(code_2 & 0x3fff, " MB")); return;
		case  1: g.attribute("value", Size_string(code_2 & 0x3fff, " GB")); return;
		default: g.attribute("value", "[bad unit]");                       return;
		}
	});
}


void Table::_report_string_set_item(Generator     &g,
                                    Header  const &header,
                                    char    const *type,
                                    unsigned       idx) const
{
	uint8_t const *data { (uint8_t *)&header };
	_report_string(g, type, _string_set_item(header, data[idx]));
}


void Table::_report_bios(Generator &g, Header const &header) const
{
	uint8_t const *data { (uint8_t *)&header };
	g.attribute("description", "BIOS Information");
	if (header.length < 18) {
		_warn("SMBIOS BIOS structure has bad length");
		return;
	}
	_report_string_set_item(g, header, "vendor",       4);
	_report_string_set_item(g, header, "version",      5);
	_report_string_set_item(g, header, "release-date", 8);
	{
		addr_t const code { *(uint16_t const *)(data + 6) };
		if (code) {
			g.node("address", [&] {
				g.attribute("value", Addr_string(Hex(code << 4))); });

			g.node("runtime-size", [&] {
				g.attribute("value", (0x10000 - code) << 4); });
		}
	}
	{
		uint16_t code_2;
		if (header.length < 26) {
			code_2 = 16;
		} else {
			code_2 = *(uint16_t const *)(data + 24);
		}
		_report_bios_rom_size(g, data[9], code_2);
	}
	_report_bios_character_0(g, *(uint64_t const *)(data + 10));
	if (header.length < 0x13) {
		return; }

	_report_bios_character_1(g, data[0x12]);
	if (header.length < 0x14) {
		return; }

	_report_bios_character_2(g, data[0x13]);
	if (header.length < 0x18) {
		return; }

	if (data[20] != 0xff && data[21] != 0xff) {
		g.node("bios-revision", [&] {
			g.attribute("value", Version_2_string(data[20], ".", data[21]));
		});
	}
	if (data[22] != 0xff && data[23] != 0xff) {
		g.node("firmware-revision", [&] {
			g.attribute("value", Version_2_string(data[22], ".", data[23]));
		});
	}
}


void Table::_report_system_uuid(Generator &g, uint8_t const *data) const
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

	g.node("uuid", [&] {
		if (only_ones) {
			g.attribute("value", "[not present]");
			return;
		}
		if (only_zeros) {
			g.attribute("value", "[not settable]");
			return;
		}
		if ( _version_major >  2 ||
		    (_version_major == 2 && _version_minor >= 6))
		{
			g.attribute("value", Uuid_string(
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
			g.attribute("value", Uuid_string(
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


const char *Table::_base_board_type(unsigned idx) const
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


void Table::_report_base_board_handles(Generator &g,
                                       uint8_t        count,
                                       uint8_t const *data) const
{
	for (unsigned idx = 0; idx < count; idx++) {
		g.node("contained-object-handle", [&] {
			uint8_t const *value { data + sizeof(uint16_t) * idx };
			g.attribute("value", Addr_string(*(uint16_t const *)value));
		});
	}
}


void Table::_report_base_board(Generator &g, Header const &header) const
{
	g.attribute("name", "Base Board Information");
	if (header.length < 8) {
		return; }

	_report_string_set_item(g, header, "manufacturer",  4);
	_report_string_set_item(g, header, "product-name",  5);
	_report_string_set_item(g, header, "version",       6);
	_report_string_set_item(g, header, "serial-number", 7);
	if (header.length < 9) {
		return; }

	_report_string_set_item(g, header, "asset-tag", 8);
	if (header.length < 10) {
		return; }

	uint8_t const *data { (uint8_t *)&header };
	_report_base_board_features(g, data[9]);
	if (header.length < 14) {
		return; }

	_report_string_set_item(g, header, "location-in-chassis", 10);

	g.node("chassis-handle", [&] {
		g.attribute("value", *(uint16_t const *)(data + 11)); });

	_report_string(g, "type", _base_board_type(data[13]));
	if (header.length < 15) {
		return; }

	if (header.length < 15 + data[14] * sizeof(uint16_t)) {
		return; }

	_report_base_board_handles(g, data[14], data + 15);
}


void Table::_report_system(Generator &g, Header const &header) const
{
	uint8_t const *data { (uint8_t *)&header };
	g.attribute("description", "System Information");
	if (header.length < 8) {
		return; }

	_report_string_set_item(g, header, "manufacturer",  4);
	_report_string_set_item(g, header, "product-name",  5);
	_report_string_set_item(g, header, "version",       6);
	_report_string_set_item(g, header, "serial-number", 7);
	if (header.length < 25) {
		return; }

	_report_system_uuid(g, data + 8);
	_report_string(g, "wake-up-type", _system_wake_up_type(data[24]));
	if (header.length < 27) {
		return; }

	_report_string_set_item(g, header, "sku-number", 25);
	_report_string_set_item(g, header, "family",     26);
}


void Table::_report_one_struct(Generator &g, Header const &entry) const
{
	g.node("structure", [&] {

		g.attribute("type",   entry.type);
		g.attribute("length", entry.length);
		g.attribute("handle", entry.handle);

		switch (entry.type) {
		case Header::BIOS:       _report_bios      (g, entry); break;
		case Header::SYSTEM:     _report_system    (g, entry); break;
		case Header::BASE_BOARD: _report_base_board(g, entry); break;
		default:
			_warn("structure type ", entry.type, " not supported");
			break;
		}
	});
}


void Table::_report_structs(Generator &g, Span const &m) const
{
	Header const *entry = (Header const *)m.start;

	while (entry) {
		if (!m.contains((char const *)(entry + 1) - 1)
		 || !m.contains((char const *)entry + entry->length - 1))
			break;

		_report_one_struct(g, *entry);

		/* seek next SMBIOS structure */
		Header const *next = nullptr;
		for (char const *n = (char const *)entry + entry->length;
		     !next && m.contains(n + 1);
		     ++n)
			if (n[0] == 0 && n[1] == 0)
				next = (Header const *)(n + 2);

		entry = next;
	};
}


void Table::_report_dmi(Generator &g, Dmi_entry_point const &ep)
{
	_version_major = (uint8_t)(ep.bcd_revision >> 4);
	_version_minor = (uint8_t)(ep.bcd_revision & 0xf);

	g.node("dmi", [&] {
		g.attribute("version",
			Version_2_string(_version_major, ".", _version_minor));

		g.attribute("structures",      ep.nr_of_structs);
		g.attribute("structures-addr", Addr_string(Hex(ep.struct_table_addr)));
		g.attribute("structures-size", ep.struct_table_length);

		_report_structs(g, Span { _mem.start + ep.LENGTH, ep.struct_table_length });
	});
}


void Table::_report_v2(Generator &g, V2_entry_point const &ep)
{
	_version_major = ep.version_major;
	_version_minor = ep.version_minor;

	if ((_version_major == 2 && _version_minor == 31) ||
	    (_version_major == 2 && _version_minor == 33)) {
		_warn("fixed weird SMBIOS version");
		_version_major = 2;
		_version_minor = 3;
	} else if (_version_major == 2 && _version_minor == 51) {

		_warn("fixed weird SMBIOS version");
		_version_major = 2;
		_version_minor = 6;
	}

	g.node("smbios", [&] {
		g.attribute("version",
			Version_2_string(_version_major, ".", _version_minor));

		g.attribute("structures",      ep.nr_of_structs);
		g.attribute("structures-addr", Addr_string(Hex(ep.struct_table_addr)));
		g.attribute("structures-size", ep.struct_table_length);

		_report_structs(g, Span { _mem.start + ep.length, ep.struct_table_length });
	});
}


void Table::_report_v3(Generator &g, V3_entry_point const &ep)
{
	_version_major = ep.version_major;
	_version_minor = ep.version_minor;

	g.node("smbios", [&] {
		g.attribute("version",
			Version_2_string(_version_major, ".", _version_minor));

		g.attribute("structures-addr", Addr_string(Hex(ep.struct_table_addr)));
		g.attribute("structures-size", ep.struct_table_max_size);

		_report_structs(g, Span { _mem.start + ep.length, ep.struct_table_max_size });
	});
}


void Table::report(Generator &g)
{
	/* check if entry point is valid */
	if (!_mem.contains(_mem.start + 5)) {
		_warn("anchor string of entry point exceeds ROM");
		return;
	}

	/* used addresses are virtual resp. 1:1 */
	auto mem_fn = [&] (addr_t base, size_t size, auto const &fn) {
		return fn(Span { (char const *)base, size }); };

	auto v3_fn  = [&] (V3_entry_point const &ep)  { _report_v3(g, ep); };
	auto v2_fn  = [&] (V2_entry_point const &ep)  { _report_v2(g, ep); };
	auto dmi_fn = [&] (Dmi_entry_point const &ep) { _report_dmi(g, ep); };

	from_pointer((addr_t)_mem.start, mem_fn, v3_fn, v2_fn, dmi_fn);
}


class Main
{
	private:

		Env                    &_env;
		Attached_rom_dataspace  _config        { _env, "config" };
		bool             const  _verbose       { _config.node().attribute_value("verbose", false) };
		Attached_rom_dataspace  _table_ds      { _env, "smbios_table" };
		Signal_handler<Main>    _table_ds_sigh { _env.ep(), *this, &Main::_handle_table_ds };
		Constructible<Table>    _table         { };

		Expanding_reporter _reporter {
			_env, "result", "result", Expanding_reporter::Initial_buffer_size { 0x2000 } };

		void _handle_table_ds();

	public:

		Main(Env &env);
};


Main::Main(Env &env) : _env (env)
{
	_table_ds.sigh(_table_ds_sigh);
	_handle_table_ds();
}


void Main::_handle_table_ds()
{
	_table_ds.update();
	if (!_table_ds.valid())
		return;

	_table.construct(_table_ds.local_addr<char const>(), _table_ds.size(), _verbose);
	_reporter.generate([&] (Generator &g) { _table->report(g); });
}


void Component::construct(Genode::Env &env) { static Main main { env }; }
