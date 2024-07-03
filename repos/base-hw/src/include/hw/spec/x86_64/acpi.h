/*
 * \brief   ACPI structures and parsing
 * \author  Alexander Boettcher
 * \date    2018-04-23
 */

/*
 * Copyright (C) 2018-2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__LIB__HW__SPEC__X86_64__ACPI_H_
#define _SRC__LIB__HW__SPEC__X86_64__ACPI_H_

#include <base/fixed_stdint.h>

namespace Hw {

	struct Acpi_generic;
	struct Apic_madt;
	struct Acpi_fadt;
	struct Acpi_facs;

	void for_each_rsdt_entry (Hw::Acpi_generic &, auto const &);
	void for_each_xsdt_entry (Hw::Acpi_generic &, auto const &);
	void for_each_apic_struct(Acpi_generic     &, auto const &);
}


/* ACPI spec 5.2.6 */
struct Hw::Acpi_generic
{
	Genode::uint8_t  signature[4];
	Genode::uint32_t size;
	Genode::uint8_t  rev;
	Genode::uint8_t  checksum;
	Genode::uint8_t  oemid[6];
	Genode::uint8_t  oemtabid[8];
	Genode::uint32_t oemrev;
	Genode::uint8_t  creator[4];
	Genode::uint32_t creator_rev;

} __attribute__((packed));


struct Hw::Apic_madt
{
	enum { LAPIC = 0, IO_APIC = 1 };

	Genode::uint8_t type;
	Genode::uint8_t length;

	Apic_madt *next() const { return reinterpret_cast<Apic_madt *>((Genode::uint8_t *)this + length); }

	struct Ioapic : Genode::Mmio<0xc>
	{
		struct Id       : Register <0x02,  8> { };
		struct Paddr    : Register <0x04, 32> { };
		struct Gsi_base : Register <0x08, 32> { };

		Ioapic(Apic_madt const * a) : Mmio({(char *)a, Mmio::SIZE}) { }
	};

	struct Lapic : Genode::Mmio<0x8>
	{
		struct Apic_id : Register<0x3, 8> {};
		struct Flags   : Register<0x4, 32>
		{
			enum { VALID = 1 };
		};

		Lapic(Apic_madt const * a) : Mmio({(char *)a, Mmio::SIZE}) { }

		bool valid() { return read<Flags>() & Flags::VALID; };
		Genode::uint8_t id() { return read<Apic_id>(); }
	};

} __attribute__((packed));


/* ACPI spec 5.2.9 and ACPI GAS 5.2.3.2 */
struct Hw::Acpi_fadt : Genode::Mmio<276>
{
	enum Addressspace { IO = 0x1 };

	enum { FW_OFFSET_EXT = 0x84, SLEEP_CONTROL_REG = 236 };

	struct Size         : Register <         0x04, 32> { };
	struct Fw_ctrl      : Register <         0x24, 32> { };
	struct Fw_ctrl_ext  : Register <FW_OFFSET_EXT, 64> { };

	struct Smi_cmd     : Register<0x30, 32> { };
	struct Acpi_enable : Register<0x34, 8> { };

	struct Pm1a_cnt_blk : Register < 64, 32> {
		struct Slp_typ : Bitfield < 10, 3> { };
		struct Slp_ena : Bitfield < 13, 1> { };
	};
	struct Pm1b_cnt_blk : Register < 68, 32> {
		struct Slp_typ : Bitfield < 10, 3> { };
		struct Slp_ena : Bitfield < 13, 1> { };
	};

	struct Gpe0_blk     : Register < 80, 32> { };
	struct Gpe1_blk     : Register < 84, 32> { };

	struct Gpe0_blk_len : Register < 92, 8> { };
	struct Gpe1_blk_len : Register < 93, 8> { };

	struct Pm1_cnt_len  : Register < 89, 8> { };

	struct Pm1a_cnt_blk_ext : Register < 172, 32> {
		struct Addressspace : Bitfield < 0, 8> { };
		struct Width        : Bitfield < 8, 8> { };
	};
	struct Pm1a_cnt_blk_ext_addr : Register < 172 + 4, 64> { };

	struct Pm1b_cnt_blk_ext : Register < 184, 32> {
		struct Addressspace : Bitfield < 0, 8> { };
		struct Width        : Bitfield < 8, 8> { };
	};
	struct Pm1b_cnt_blk_ext_addr : Register < 184 + 4, 64> { };

	struct Gpe0_blk_ext : Register < 220, 32> {
		struct Addressspace : Bitfield < 0, 8> { };
		struct Width        : Bitfield < 8, 8> { };
	};
	struct Gpe0_blk_ext_addr : Register < 220 + 4, 64> { };

	struct Gpe1_blk_ext : Register < 232, 32> {
		struct Addressspace : Bitfield < 0, 8> { };
		struct Width        : Bitfield < 8, 8> { };
	};
	struct Gpe1_blk_ext_addr : Register < 232 + 4, 64> { };

	/**
	 * Read from I/O port
	 */
	Genode::uint8_t inb(Genode::uint16_t port)
	{
		Genode::uint8_t res;
		asm volatile ("inb %w1, %0" : "=a"(res) : "Nd"(port));
		return res;
	}

	Genode::uint16_t inw(Genode::uint16_t port)
	{
		Genode::uint16_t res;
		asm volatile ("inw %w1, %0" : "=a"(res) : "Nd"(port));
		return res;
	}

	Genode::uint32_t inl(Genode::uint16_t port)
	{
		Genode::uint32_t res;
		asm volatile ("inl %w1, %0" : "=a"(res) : "Nd"(port));
		return res;
	}

	/**
	 * Write to I/O port
	 */
	inline void outb(Genode::uint16_t port, Genode::uint8_t val)
	{
		asm volatile ("outb %0, %w1" : : "a"(val), "Nd"(port));
	}

	inline void outw(Genode::uint16_t port, Genode::uint16_t val)
	{
		asm volatile ("outw %0, %w1" : : "a"(val), "Nd"(port));
	}

	inline void outl(Genode::uint16_t port, Genode::uint32_t val)
	{
		asm volatile ("outl %0, %w1" : : "a"(val), "Nd"(port));
	}

	Acpi_fadt(Acpi_generic const * a) : Mmio({(char *)a, Mmio::SIZE}) { }

	void takeover_acpi()
	{
		if (!read<Acpi_enable>() || !read<Smi_cmd>())
			return;

		asm volatile ("out %0, %w1" :: "a" (read<Acpi_enable>()),
		              "Nd" (read<Smi_cmd>()));
	}

	addr_t facs() const
	{
		addr_t facs { };

		if (read<Size>() >= FW_OFFSET_EXT + 8)
			facs = read<Fw_ctrl_ext>();

		if (!facs)
			facs = read<Fw_ctrl>();

		return facs;
	}

	void clear_gpe0_status()
	{
		auto constexpr half_register = true;

		return _write<Gpe0_blk_len, Gpe0_blk, Gpe0_blk_ext::Width,
		              Gpe0_blk_ext::Addressspace, Gpe0_blk_ext_addr>(~0ULL,
		                                                             half_register);
	}

	void clear_gpe1_status()
	{
		auto constexpr half_register = true;

		return _write<Gpe1_blk_len, Gpe1_blk, Gpe1_blk_ext::Width,
		              Gpe1_blk_ext::Addressspace, Gpe1_blk_ext_addr>(~0ULL,
		                                                             half_register);
	}

	unsigned read_cnt_blk()
	{
		auto const pm1_a = _read<Pm1_cnt_len, Pm1a_cnt_blk,
		                         Pm1a_cnt_blk_ext::Width,
		                         Pm1a_cnt_blk_ext::Addressspace,
		                         Pm1a_cnt_blk_ext_addr>();
		auto const pm1_b = _read<Pm1_cnt_len, Pm1b_cnt_blk,
		                         Pm1b_cnt_blk_ext::Width,
		                         Pm1b_cnt_blk_ext::Addressspace,
		                         Pm1b_cnt_blk_ext_addr>();
		return pm1_a | pm1_b;
	}

	void write_cnt_blk(unsigned value_a, unsigned value_b)
	{
		_write<Pm1_cnt_len, Pm1a_cnt_blk, Pm1a_cnt_blk_ext::Width,
		       Pm1a_cnt_blk_ext::Addressspace, Pm1a_cnt_blk_ext_addr>(value_a);
		_write<Pm1_cnt_len, Pm1b_cnt_blk, Pm1b_cnt_blk_ext::Width,
		       Pm1b_cnt_blk_ext::Addressspace, Pm1b_cnt_blk_ext_addr>(value_b);
	}

	void suspend(Genode::uint8_t typ_slp_a, Genode::uint8_t typ_slp_b)
	{
		auto cnt     = read_cnt_blk();
		auto value_a = cnt, value_b = cnt;

		Pm1a_cnt_blk::Slp_typ::set(value_a, typ_slp_a);
		Pm1a_cnt_blk::Slp_ena::set(value_a, 1);

		Pm1b_cnt_blk::Slp_typ::set(value_b, typ_slp_b);
		Pm1b_cnt_blk::Slp_ena::set(value_b, 1);

		write_cnt_blk(value_a, value_b);
	}

	template <typename REG_LEN, typename BLK_ADDR, typename BLK_WIDTH_EXT,
	          typename BLK_AS_EXT, typename BLK_ADDR_EXT, typename T>
	unsigned _access(bool hw_read, T value, bool half_register = false,
	                 bool half_offset = false)
	{
		unsigned long long raw_io_addr = 0ull;
		unsigned width_bits = 0u;

		auto const register_len = read<REG_LEN>();
		auto const reg_size     = read<Size>();

		if (reg_size >= SLEEP_CONTROL_REG && read<BLK_ADDR_EXT>() != 0ull) {
			if (register_len)
				width_bits = register_len * 8;
			else {
				width_bits = read<BLK_WIDTH_EXT>();
			}

			auto const as = read<BLK_AS_EXT>();
			if (as != Addressspace::IO) {
				Genode::error("unsupported address space access method ", as);
				return 0;
			}

			raw_io_addr = read<BLK_ADDR_EXT>();
		} else if (read<BLK_ADDR>() != 0 && register_len != 0) {
			width_bits  = register_len * 8;
			raw_io_addr = read<BLK_ADDR>();
		}

		if (!width_bits || !raw_io_addr)
			return 0u;

		/* I/O address has to be 16 bit */
		if (raw_io_addr >= (1ull << 16) || width_bits >= (1ull << 16)) {
			Genode::error("too large I/O address ", raw_io_addr, " ", width_bits);
			return 0u;
		}

		Genode::uint16_t io_addr = Genode::uint16_t(raw_io_addr);

		if (half_register)
			width_bits /= 2;

		if (half_offset)
			io_addr += Genode::uint16_t(width_bits) / 2 / 8;

		if (hw_read) {
			switch (width_bits) {
			case  8:
				return inb(io_addr);
			case 16:
				return inw(io_addr);
			case 32:
				return inl(io_addr);
			default:
				Genode::error("unsupported width for I/O IN : ", width_bits);
			}
		} else {
			switch (width_bits) {
			case  8:
				outb(io_addr, Genode::uint8_t(value));
				break;
			case 16:
				outw(io_addr, Genode::uint16_t(value));
				break;
			case 32:
				outl(io_addr, Genode::uint32_t(value));
				break;
			case 64:
				outl(io_addr, Genode::uint32_t(value));
				if (sizeof(value) == 8)
					outl(io_addr + 4, Genode::uint32_t(value >> 32));
				break;
			default:
				Genode::error("unsupported width for I/O OUT : ", width_bits);
			}
		}
		return 0u;
	}

	template <typename REG_LEN, typename BLK_ADDR, typename BLK_WIDTH_EXT,
	          typename BLK_AS_EXT, typename BLK_ADDR_EXT>
	unsigned _read(bool half_register = false, bool half_offset = false)
	{
		enum { READ = true };
		unsigned value = 0;

		return _access<REG_LEN, BLK_ADDR, BLK_WIDTH_EXT, BLK_AS_EXT,
		               BLK_ADDR_EXT>(READ, value, half_register, half_offset);
	}

	template <typename REG_LEN, typename BLK_ADDR, typename BLK_WIDTH_EXT,
	          typename BLK_AS_EXT, typename BLK_ADDR_EXT, typename T>
	void _write(T value, bool half_register = false, bool half_offset = false)
	{
		enum { WRITE = false };

		_access<REG_LEN, BLK_ADDR, BLK_WIDTH_EXT, BLK_AS_EXT,
		        BLK_ADDR_EXT>(WRITE, value, half_register, half_offset);
	}
};


struct Hw::Acpi_facs : Genode::Mmio<64>
{
	struct Length             : Register < 0x04, 32> { };
	struct Fw_wake_vector     : Register < 0x0c, 32> { };
	struct Fw_wake_vector_ext : Register < 0x18, 64> { };

	void wakeup_vector(addr_t const entry)
	{
		auto len = read<Length>();
		if (len >= 0x0c + 4)
			write<Fw_wake_vector>(unsigned(entry));

		if (len >= 0x18 + 8)
			write<Fw_wake_vector_ext>(0);
	}

	Acpi_facs(addr_t const mmio) : Mmio({(char *)mmio, Mmio::SIZE}) { }
};


void Hw::for_each_rsdt_entry(Hw::Acpi_generic &rsdt, auto const &fn)
{
	if (Genode::memcmp(rsdt.signature, "RSDT", 4))
		return;

	using entry_t = Genode::uint32_t;

	unsigned const table_size  = rsdt.size;
	unsigned const entry_count = (unsigned)((table_size - sizeof(rsdt)) / sizeof(entry_t));

	entry_t * entries = reinterpret_cast<entry_t *>(&rsdt + 1);
	for (unsigned i = 0; i < entry_count; i++)
		fn(entries[i]);
}


void Hw::for_each_xsdt_entry(Hw::Acpi_generic &xsdt, auto const &fn)
{
	if (Genode::memcmp(xsdt.signature, "XSDT", 4))
		return;

	using entry_t = Genode::uint64_t;

	unsigned const table_size  = xsdt.size;
	unsigned const entry_count = (unsigned)((table_size - sizeof(xsdt)) / sizeof(entry_t));

	entry_t * entries = reinterpret_cast<entry_t *>(&xsdt + 1);
	for (unsigned i = 0; i < entry_count; i++)
		fn(entries[i]);
}


void Hw::for_each_apic_struct(Hw::Acpi_generic &apic_madt, auto const &fn)
{
	if (Genode::memcmp(apic_madt.signature, "APIC", 4))
		return;

	Apic_madt const * const first = reinterpret_cast<Apic_madt *>(&apic_madt.creator_rev + 3);
	Apic_madt const * const last  = reinterpret_cast<Apic_madt *>(reinterpret_cast<Genode::uint8_t *>(apic_madt.signature) + apic_madt.size);

	for (Apic_madt const * e = first; e < last ; e = e->next())
		fn(e);
}

#endif /* _SRC__LIB__HW__SPEC__X86_64__ACPI_H_ */
