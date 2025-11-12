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
#include <util/mmio.h>

namespace Hw {

	namespace Acpi {

		using namespace Genode;

		template <size_t> struct Table;

		struct Rsdp;
		struct Rsdt;
		struct Xsdt;
		struct Madt;
		struct Fadt;
		struct Facs;

		enum {
			SIZE_RSDP = 36,
			SIZE_RSDT = 36,
			SIZE_XSDT = 36,
			SIZE_MADT = 44,
			SIZE_FADT = 244,
			SIZE_FACS = 64,
		};
	}
}


/**
 * Generic ACPI system desriptor table header
 * See ACPI spec 5.2.6
 */
template <Genode::size_t SIZE>
struct Hw::Acpi::Table : Mmio<SIZE>
{
	struct Signature : Mmio<SIZE>::template Register<0x0, 32> {};
	struct Size      : Mmio<SIZE>::template Register<0x4, 32> {};

	bool equals(const char * const signature)
	{
		typename Signature::access_t s = Mmio<SIZE>::template read<Signature>();
		return !memcmp(&s, signature, 4);
	}

	size_t size() { return Mmio<SIZE>::template read<Size>(); }

	Table(addr_t addr) : Mmio<SIZE>({(char*)addr, SIZE}) {};
};


/**
 * ACPI Root System Descriptor Table
 * See ACPI spec 5.2.7
 */
struct Hw::Acpi::Rsdt : Table<SIZE_RSDT>
{
	bool valid() { return equals("RSDT"); }

	void for_each_entry(auto const &fn)
	{
		using entry_t = Genode::uint32_t;

		size_t const entry_count = (size() - SIZE) / sizeof(entry_t);
		entry_t * entries = reinterpret_cast<entry_t *>(base() + SIZE);
		for (unsigned i = 0; i < entry_count; i++)
			fn(entries[i]);
	}

	using Table<SIZE_RSDT>::Table;
};


/**
 * ACPI Extended System Descriptor Table
 * See ACPI spec 5.2.8
 */
struct Hw::Acpi::Xsdt : Table<SIZE_XSDT>
{
	bool valid() { return equals("XSDT"); }

	void for_each_entry(auto const &fn)
	{
		using entry_t = Genode::uint64_t;

		size_t const entry_count = (size() - SIZE) / sizeof(entry_t);
		entry_t * entries = reinterpret_cast<entry_t *>(base() + SIZE);
		for (unsigned i = 0; i < entry_count; i++)
			fn(entries[i]);
	}

	using Table<SIZE_XSDT>::Table;
};


/**
 * Multiple APIC Description Table
 * See ACPI spec 5.2.12
 */
struct Hw::Acpi::Madt : Table<SIZE_MADT>
{
	struct Flags : Register <0x28, 32> { };

	template <size_t SIZE>
	struct Apic_tpl : Mmio<SIZE>
	{
		struct Type : Register<0x0, 8>
		{
			enum { LAPIC = 0, IO_APIC = 1 };
		};

		struct Size : Register<0x1, 8> {};

		uint8_t type() { return Mmio<SIZE>::template read<Type>(); }
		size_t  size() { return Mmio<SIZE>::template read<Size>(); }

		Apic_tpl(addr_t addr) : Mmio<SIZE>({(char*)addr, SIZE}) {};
	};

	using Apic_descriptor = Apic_tpl<0x2>;

	/* Local-APIC, see 5.2.12.2 */
	struct Local_apic : Apic_tpl<0x8>
	{
		struct Apic_id : Register<0x3, 8> {};
		struct Flags   : Register<0x4, 32>
		{
			enum { VALID = 1 };
		};

		bool valid() { return read<Flags>() & Flags::VALID; };
		Genode::uint8_t id() { return read<Apic_id>(); }

		using Apic_tpl<0x8>::Apic_tpl;
	};

	/* I/O-APIC, see 5.2.12.3 */
	struct Io_apic : Apic_tpl<0xc>
	{
		struct Id       : Register <0x2,  8> { };
		struct Paddr    : Register <0x4, 32> { };
		struct Gsi_base : Register <0x8, 32> { };

		using Apic_tpl<0xc>::Apic_tpl;
	};

	bool valid() { return equals("APIC"); }

	void for_each_apic(auto const &fn_lapic, auto const &fn_ioapic)
	{
		if (!valid())
			return;

		for (addr_t addr = base() + SIZE; addr < (base() + size());) {
			Apic_descriptor desc(addr);
			switch (desc.type()) {
			case Apic_descriptor::Type::LAPIC:
				{
					Local_apic lapic(addr);
					if (lapic.valid()) fn_lapic(lapic);
					break;
				}
			case Apic_descriptor::Type::IO_APIC:
				{
					Io_apic ioapic(addr);
					fn_ioapic(ioapic);
					break;
				}
			};
			addr += desc.size();
		}
	}

	using Table<SIZE_MADT>::Table;
} __attribute__((packed));


/*
 * Fixed ACPI Descriptor Table
 * See spec 5.2.9 and ACPI GAS 5.2.3.2
 */
class Hw::Acpi::Fadt : public Table<SIZE_FADT>
{
	private:

		enum Addressspace { IO = 0x1 };

		enum { FW_OFFSET_EXT = 0x84, SLEEP_CONTROL_REG = 236 };

		struct Fw_ctrl      : Register <         0x24, 32> { };
		struct Fw_ctrl_ext  : Register <FW_OFFSET_EXT, 64> { };

		struct Smi_cmd     : Register<0x30, 32> { };
		struct Acpi_enable : Register<0x34, 8> { };

		struct Pm_tmr_len : Register< 91, 8> { };

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

		struct X_pm_tmr_blk : Register < 208, 32> {
			struct Addressspace : Bitfield < 0, 8> { };
			struct Width        : Bitfield < 8, 8> { };
		};

		struct X_pm_tmr_blk_addr : Register < 208 + 4, 64> { };

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
		Genode::uint8_t _inb(Genode::uint16_t port)
		{
			Genode::uint8_t res;
			asm volatile ("inb %w1, %0" : "=a"(res) : "Nd"(port));
			return res;
		}

		Genode::uint16_t _inw(Genode::uint16_t port)
		{
			Genode::uint16_t res;
			asm volatile ("inw %w1, %0" : "=a"(res) : "Nd"(port));
			return res;
		}

		Genode::uint32_t _inl(Genode::uint16_t port)
		{
			Genode::uint32_t res;
			asm volatile ("inl %w1, %0" : "=a"(res) : "Nd"(port));
			return res;
		}

		/**
		 * Write to I/O port
		 */
		inline void _outb(Genode::uint16_t port, Genode::uint8_t val)
		{
			asm volatile ("outb %0, %w1" : : "a"(val), "Nd"(port));
		}

		inline void _outw(Genode::uint16_t port, Genode::uint16_t val)
		{
			asm volatile ("outw %0, %w1" : : "a"(val), "Nd"(port));
		}

		inline void _outl(Genode::uint16_t port, Genode::uint32_t val)
		{
			asm volatile ("outl %0, %w1" : : "a"(val), "Nd"(port));
		}

		unsigned _read_cnt_blk()
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

		/* see ACPI spec version 6.5 4.8.3.3. Power Management Timer (PM_TMR) */
		uint32_t _read_pm_tmr()
		{
			if (read<Pm_tmr_len>() != 4)
				return 0;

			addr_t const tmr_addr = read<X_pm_tmr_blk_addr>();

			if (!tmr_addr)
				return 0;

			uint8_t const tmr_addr_type = read<X_pm_tmr_blk::Addressspace>();

			/* I/O port address, most likely */
			if (tmr_addr_type == 1) return _inl((uint16_t)tmr_addr);

			/* System Memory space address */
			if (tmr_addr_type == 0) return *(uint32_t *)tmr_addr;

			return 0;
		}

		void _write_cnt_blk(unsigned value_a, unsigned value_b)
		{
			_write<Pm1_cnt_len, Pm1a_cnt_blk, Pm1a_cnt_blk_ext::Width,
			       Pm1a_cnt_blk_ext::Addressspace, Pm1a_cnt_blk_ext_addr>(value_a);
			_write<Pm1_cnt_len, Pm1b_cnt_blk, Pm1b_cnt_blk_ext::Width,
			       Pm1b_cnt_blk_ext::Addressspace, Pm1b_cnt_blk_ext_addr>(value_b);
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
					return _inb(io_addr);
				case 16:
					return _inw(io_addr);
				case 32:
					return _inl(io_addr);
				default:
					Genode::error("unsupported width for I/O IN : ", width_bits);
				}
			} else {
				switch (width_bits) {
				case  8:
					_outb(io_addr, Genode::uint8_t(value));
					break;
				case 16:
					_outw(io_addr, Genode::uint16_t(value));
					break;
				case 32:
					_outl(io_addr, Genode::uint32_t(value));
					break;
				case 64:
					_outl(io_addr, Genode::uint32_t(value));
					if (sizeof(value) == 8)
						_outl(io_addr + 4, Genode::uint32_t(value >> 32));
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

	public:

		using Table<SIZE_FADT>::Table;

		bool valid() { return equals("FACP"); }

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

		void suspend(Genode::uint8_t typ_slp_a, Genode::uint8_t typ_slp_b)
		{
			auto cnt     = _read_cnt_blk();
			auto value_a = cnt, value_b = cnt;

			Pm1a_cnt_blk::Slp_typ::set(value_a, typ_slp_a);
			Pm1a_cnt_blk::Slp_ena::set(value_a, 1);

			Pm1b_cnt_blk::Slp_typ::set(value_b, typ_slp_b);
			Pm1b_cnt_blk::Slp_ena::set(value_b, 1);

			_write_cnt_blk(value_a, value_b);
		}

		uint32_t calibrate_freq_khz(uint32_t sleep_ms, auto get_value_fn, bool reverse = false)
		{
			unsigned const acpi_timer_freq = 3'579'545;

			uint32_t const initial = _read_pm_tmr();

			if (!initial) return 0;

			uint64_t const t1 = get_value_fn();
			while ((_read_pm_tmr() - initial) < (acpi_timer_freq * sleep_ms / 1000))
				asm volatile ("pause":::"memory");
			uint64_t const t2 = get_value_fn();

			return (uint32_t)((reverse ? (t1 - t2) : (t2 - t1)) / sleep_ms);
		}
};


/*
 * Firmware ACPI Control Structure
 * See spec 5.2.10
 */
struct Hw::Acpi::Facs : Genode::Mmio<SIZE_FACS>
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

	Facs(addr_t const mmio) : Mmio({(char *)mmio, Mmio::SIZE}) { }
};


/**
 * ACPI Root System Description Pointer
 * See ACPI spec 5.2.5
 */
struct Hw::Acpi::Rsdp : Genode::Mmio<SIZE_RSDP>
{
	struct Signature : Register<0x0,  64> {};
	struct Revision  : Register<0xf,  8> {};
	struct Rsdt      : Register<0x10, 32> {};
	struct Xsdt      : Register<0x18, 64> {};

	bool valid()
	{
		Signature::access_t signature = read<Signature>();
		return !memcmp(&signature, "RSD PTR ", 8);
	}

	uint8_t  revision() { return read<Revision>(); }
	uint32_t rsdt() { return read<Rsdt>(); }
	uint64_t xsdt() { return revision() ? read<Xsdt>() : 0; }

	static void search(addr_t const addr, size_t const size,
	                   auto const &found, auto const &not_found)
	{
		for (addr_t off = 0; off + SIZE < size; off += 8) {
			Rsdp rsdp(addr+off);
			if (rsdp.valid()) {
				found(rsdp);
				return;
			}
		}
		not_found();
	}

	void for_each_entry(auto const &fadt_fn,
	                    auto const &madt_fn)
	{
		if (!valid())
			return;

		auto lambda = [&] (auto addr) {
			Fadt fadt(addr);
			if (fadt.valid()) fadt_fn(fadt);

			Madt madt(addr);
			if (madt.valid()) madt_fn(madt);
		};

		if (xsdt()) {
			Acpi::Xsdt table(xsdt());
			if (table.valid()) {
				table.for_each_entry(lambda);
				return;
			}
		}

		if (rsdt()) {
			Acpi::Rsdt table(rsdt());
			if (table.valid())
				table.for_each_entry(lambda);
		}
	}

	Rsdp(addr_t addr) : Mmio<SIZE_RSDP>({(char*)addr, SIZE_RSDP}) {};
};

#endif /* _SRC__LIB__HW__SPEC__X86_64__ACPI_H_ */
