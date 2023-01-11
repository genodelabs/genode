/*
 * \brief  VMM utilities to generate a flattened device tree blob
 * \author Stefan Kalkowski
 * \date   2022-11-04
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <board.h>
#include <fdt.h>
#include <psci.h>
#include <util/array.h>
#include <util/endian.h>
#include <util/register_set.h>

using namespace Genode;

class Mmio_big_endian_access
{
	friend Register_set_plain_access;

	private:

		addr_t const _base;

		/**
		 * Write '_ACCESS_T' typed 'value' to MMIO base + 'offset'
		 */
		template <typename ACCESS_T>
		inline void _write(off_t const offset, ACCESS_T const value)
		{
			addr_t const dst = _base + offset;
			*(ACCESS_T volatile *)dst = host_to_big_endian(value);
		}

		/**
		 * Read '_ACCESS_T' typed from MMIO base + 'offset'
		 */
		template <typename ACCESS_T>
		inline ACCESS_T _read(off_t const &offset) const
		{
			addr_t const dst = _base + offset;
			ACCESS_T const value = *(ACCESS_T volatile *)dst;
			return host_to_big_endian(value);
		}

	public:

		Mmio_big_endian_access(addr_t const base) : _base(base) { }

		addr_t base() const { return _base; }
};


struct Mmio : Mmio_big_endian_access,
              Register_set<Mmio_big_endian_access>
{
	Mmio(addr_t const base)
	:
		Mmio_big_endian_access(base),
		Register_set(*static_cast<Mmio_big_endian_access *>(this)) { }
};


struct Fdt_header : Mmio
{
	struct Magic             : Register<0x0,  32> {};
	struct Totalsize         : Register<0x4,  32> {};
	struct Off_dt_struct     : Register<0x8,  32> {};
	struct Off_dt_strings    : Register<0xc,  32> {};
	struct Off_mem_rsvmap    : Register<0x10, 32> {};
	struct Version           : Register<0x14, 32> {};
	struct Last_comp_version : Register<0x18, 32> {};
	struct Boot_cpuid_phys   : Register<0x1c, 32> {};
	struct Size_dt_strings   : Register<0x20, 32> {};
	struct Size_dt_struct    : Register<0x24, 32> {};

	using Mmio::Mmio;

	enum { SIZE = 10*4 };
};


struct Fdt_reserve_entry : Mmio
{
	struct Address : Register<0, 64> {};
	struct Size    : Register<8, 64> {};

	using Mmio::Mmio;

	enum { SIZE = 2*8 };
};


enum Fdt_tokens {
	FDT_BEGIN_NODE = 0x00000001U,
	FDT_END_NODE   = 0x00000002U,
	FDT_PROP       = 0x00000003U,
	FDT_NOP        = 0x00000004U,
	FDT_END        = 0x00000009U,
};


struct Fdt_token : Mmio
{
	struct Type : Register<0, 32> {};

	Fdt_token(addr_t const base, Fdt_tokens type)
	:
		Mmio(base)
	{
		write<Type>(type);
	}

	enum { SIZE = 4 };
};


struct Fdt_prop : Fdt_token
{
	struct Len     : Register<4, 32> {};
	struct Nameoff : Register<8, 32> {};

	Fdt_prop(addr_t base, uint32_t len, uint32_t name_offset)
	:
		Fdt_token(base, FDT_PROP)
	{
		write<Fdt_prop::Len>(len);
		write<Fdt_prop::Nameoff>(name_offset);
	}

	enum { SIZE = Fdt_token::SIZE + 2*4 };
};


enum Interrupt_specifier {
	GIC_SPI = 0,
	GIC_PPI = 1,
};


enum Interrupt_type {
	IRQ_TYPE_NONE         = 0,
	IRQ_TYPE_EDGE_RISING  = 1,
	IRQ_TYPE_EDGE_FALLING = 2,
	IRQ_TYPE_EDGE_BOTH    = 3,
	IRQ_TYPE_LEVEL_HIGH   = 4,
	IRQ_TYPE_LEVEL_LOW    = 8,
};


enum {
	FDT_MAGIC        = 0xd00dfeed,
	FDT_VERSION      = 17,
	FDT_COMP_VERSION = 16,
};


enum Phandles {
	GIC = 1,
	CLK = 2
};

struct Value
{
	uint32_t value;

	Value(uint32_t const & v) : value(v) {};
	Value() : value(0) {};

	Value &operator= (uint32_t const & v)
	{
		value = v;
		return *this;
	}

	size_t length() const { return sizeof(value); }

	template <typename T>
	void write(uint32_t off, T & buf) const
	{
		uint32_t v = host_to_big_endian(value);
		buf.write(off, &v, length());
	}
};


template <typename T, unsigned MAX>
struct Array : Genode::Array<T, MAX>
{
	using Genode::Array<T,MAX>::Array;

	size_t length() const
	{
		size_t ret = 0;
		this->for_each([&] (unsigned, T const & v) { ret += v.length(); });
		return ret;
	}

	template <typename BUF>
	void write(uint32_t off, BUF & buf) const
	{
		this->for_each([&] (unsigned, T const & v) {
			v.write(off, buf);
			off += (uint32_t)v.length();
		});
	}
};


void Vmm::Fdt_generator::_generate_tree(uint32_t & off, Config const & config,
                                        void * initrd_start, size_t initrd_size)
{
	using Name = Fdt_dictionary::Name;

	auto node = [&] (auto const & name, auto const & fn)
	{
		Fdt_token start(_buffer.addr+off, FDT_BEGIN_NODE);
		off += Fdt_token::SIZE;
		_buffer.write(off, name.string(), name.length());
		off += (uint32_t)name.length();
		off = align_addr(off, 2);
		fn();
		Fdt_token end(_buffer.addr+off, FDT_END_NODE);
		off += Fdt_token::SIZE;
	};

	auto property = [&] (auto const & name, auto const & val)
	{
		_dict.add(name);
		Fdt_prop prop(_buffer.addr+off, (uint32_t)val.length(),
		              _dict.offset(name));
		off += Fdt_prop::SIZE;
		val.write(off, _buffer);
		off = align_addr(off+(uint32_t)val.length(), 2);
	};

	node(Name(""), [&] ()
	{
		property(Name("compatible"),       Name("linux,dummy-virt"));
		property(Name("#address-cells"),   Value(2));
		property(Name("#size-cells"),      Value(2));
		property(Name("interrupt-parent"), Value(GIC));

		node(Name("cpus"), [&] ()
		{
			property(Name("#address-cells"),   Value(1));
			property(Name("#size-cells"),      Value(0));

			for (unsigned i = 0; i < config.cpu_count(); i++) {
				node(Name("cpu@", i), [&] ()
				{
					property(Name("compatible"),    Name(config.cpu_type()));
					property(Name("reg"),           Value(i));
					property(Name("device_type"),   Name("cpu"));
					property(Name("enable-method"), Name("psci"));
				});
			}
		});

		node(Name("psci"), [&] ()
		{
			property(Name("compatible"),  Name("arm,psci-1.0"));
			property(Name("method"),      Name("hvc"));
			property(Name("cpu_suspend"), Value{Psci::CPU_SUSPEND});
			property(Name("cpu_off"),     Value{Psci::CPU_OFF});
			property(Name("cpu_on"),      Value{Psci::CPU_ON});
		});

		node(Name("timer"), [&] ()
		{
			property(Name("compatible"),
			         ::Array<Name, 2>("arm,armv8-timer", "arm,armv7-timer"));
			property(Name("interrupts"),
			         ::Array<Value, 12>(GIC_PPI, 0xd, IRQ_TYPE_LEVEL_HIGH,
			                            GIC_PPI, 0xe, IRQ_TYPE_LEVEL_HIGH,
			                            GIC_PPI, 0xb, IRQ_TYPE_LEVEL_HIGH,
			                            GIC_PPI, 0xa, IRQ_TYPE_LEVEL_HIGH));
		});

		node(Name("gic"), [&] ()
		{
			bool gicv2 = config.gic_version() < 3U;
			property(Name("phandle"), Value(GIC));
			property(Name("compatible"),
			         (gicv2) ? Name("arm,gic-400") : Name("arm,gic-v3"));
			property(Name("ranges"),                 ::Array<Value,0>());
			property(Name("interrupt-controller"),   ::Array<Value,0>());
			property(Name("#address-cells"),         Value(2));
			property(Name("#redistributor-regions"), Value(1));
			property(Name("#interrupt-cells"),       Value(3));
			property(Name("#size-cells"),            Value(2));
			property(Name("reg"),
			         ::Array<Value, 8>(0, GICD_MMIO_START, 0, GICD_MMIO_SIZE,
			                           0, (gicv2) ? GICC_MMIO_START : GICR_MMIO_START,
			                           0, (gicv2) ? GICC_MMIO_SIZE  : GICR_MMIO_SIZE));
		});

		node(Name("clocks"), [&] ()
		{
			property(Name("#address-cells"), Value(1));
			property(Name("#size-cells"),    Value(0));

			node(Name("clk@0"), [&] ()
			{
				property(Name("compatible"),         Name("fixed-clock"));
				property(Name("clock-output-names"), Name("clk24mhz"));
				property(Name("clock-frequency"),    Value(24000000));
				property(Name("#clock-cells"),       Value(0));
				property(Name("reg"),                Value(0));
				property(Name("phandle"),            Value(CLK));
			});
		});

		node(Name("pl011"), [&] ()
		{
			property(Name("compatible"),
			         ::Array<Name, 2>("arm,pl011", "arm,primecell"));
			property(Name("interrupts"),
			         ::Array<Value, 3>(GIC_SPI, PL011_IRQ-32, IRQ_TYPE_LEVEL_HIGH));
			property(Name("reg"), ::Array<Value, 4>(0, PL011_MMIO_START,
			                                        0, PL011_MMIO_SIZE));
			property(Name("clock-names"),
			         ::Array<Name, 2>("uartclk", "apb_pclk"));
			property(Name("clocks"), ::Array<Value, 2>(CLK, CLK));
		});

		node(Name("memory"), [&] ()
		{
			property(Name("reg"), ::Array<Value, 4>(0, RAM_START, 0, config.ram_size()));
			property(Name("device_type"), Name("memory"));
		});

		node(Name("chosen"), [&] ()
		{
			property(Name("bootargs"),           Name(config.bootargs()));
			property(Name("stdout-path"),        Name("/pl011"));

			if (!initrd_size)
				return;

			/* we're sure that the initrd start address is wide below 4GB */
			uint32_t start = (uint32_t)((addr_t)initrd_start & 0xffffffff);
			property(Name("linux,initrd-start"), Value(start));
			property(Name("linux,initrd-end"),   Value(start+initrd_size));
		});

		config.for_each_virtio_device([&] (Config::Virtio_device const & dev) {
			node(Name("virtio@", dev.mmio_start), [&] ()
			{
				property(Name("interrupts"),
				         ::Array<Value, 3>(GIC_SPI, dev.irq-32, IRQ_TYPE_EDGE_RISING));
				property(Name("compatible"), Name("virtio,mmio"));
				property(Name("dma-coherent"),   ::Array<Value,0>());
				property(Name("reg"),
				         ::Array<Value, 4>(0, (uint32_t)((addr_t)dev.mmio_start & 0xffffffff),
				                           0, (uint32_t)dev.mmio_size));
			});
		});
	});

	Fdt_token end(_buffer.addr+off, FDT_END);
	off += Fdt_token::SIZE;
}


void Vmm::Fdt_generator::generate(Config const & config,
                                  void * initrd_start, size_t initrd_size)
{
	Fdt_header header(_buffer.addr);
	header.write<Fdt_header::Magic>(FDT_MAGIC);
	header.write<Fdt_header::Version>(FDT_VERSION);
	header.write<Fdt_header::Last_comp_version>(FDT_COMP_VERSION);
	header.write<Fdt_header::Boot_cpuid_phys>(0);

	uint32_t off = Fdt_header::SIZE;
	header.write<Fdt_header::Off_mem_rsvmap>(off);
	Fdt_reserve_entry memory(_buffer.addr+off);
	memory.write<Fdt_reserve_entry::Address>(0);
	memory.write<Fdt_reserve_entry::Size>(0);

	off += Fdt_reserve_entry::SIZE;
	header.write<Fdt_header::Off_dt_struct>(off);

	_generate_tree(off, config, initrd_start, initrd_size);

	header.write<Fdt_header::Size_dt_struct>(off-Fdt_header::SIZE-Fdt_reserve_entry::SIZE);

	header.write<Fdt_header::Off_dt_strings>(off);
	header.write<Fdt_header::Size_dt_strings>(_dict.length());
	_dict.write([&] (addr_t o, const char * src, size_t len) {
		_buffer.write(off+o, src, len); });

	off += _dict.length();
	header.write<Fdt_header::Totalsize>(off);
}
