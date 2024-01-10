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

		Byte_range_ptr const _range;

		/**
		 * Write '_ACCESS_T' typed 'value' to MMIO base + 'offset'
		 */
		template <typename ACCESS_T>
		inline void _write(off_t const offset, ACCESS_T const value)
		{
			*(ACCESS_T volatile *)(_range.start + offset) = host_to_big_endian(value);
		}

		/**
		 * Read '_ACCESS_T' typed from MMIO base + 'offset'
		 */
		template <typename ACCESS_T>
		inline ACCESS_T _read(off_t const &offset) const
		{
			return host_to_big_endian(*(ACCESS_T volatile *)(_range.start + offset));
		}

	public:

		Mmio_big_endian_access(Byte_range_ptr const &range) : _range(range.start, range.num_bytes) { }

		Byte_range_ptr range_at(off_t offset) const
		{
			return {_range.start + offset, _range.num_bytes - offset};
		}

		Byte_range_ptr range() const { return range_at(0); }

		addr_t base() const { return (addr_t)range().start; }
};


template <size_t MMIO_SIZE>
struct Mmio : Mmio_big_endian_access,
              Register_set<Mmio_big_endian_access, MMIO_SIZE>
{
	static constexpr size_t SIZE = MMIO_SIZE;

	Mmio(Byte_range_ptr const &range)
	:
		Mmio_big_endian_access(range),
		Register_set<Mmio_big_endian_access, SIZE>(*static_cast<Mmio_big_endian_access *>(this)) { }
};


struct Fdt_header : Mmio<10*4>
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
};


struct Fdt_reserve_entry : Mmio<2*8>
{
	struct Address : Register<0, 64> {};
	struct Size    : Register<8, 64> {};

	using Mmio::Mmio;
};


enum Fdt_tokens {
	FDT_BEGIN_NODE = 0x00000001U,
	FDT_END_NODE   = 0x00000002U,
	FDT_PROP       = 0x00000003U,
	FDT_NOP        = 0x00000004U,
	FDT_END        = 0x00000009U,
};


template <size_t SIZE>
struct Fdt_token_tpl : Mmio<SIZE>
{
	using Base = Mmio<SIZE>;

	struct Type : Base::template Register<0, 32> {};

	Fdt_token_tpl(Byte_range_ptr const &range, Fdt_tokens type)
	:
		Base(range)
	{
		Base::template write<Type>(type);
	}
};


using Fdt_token = Fdt_token_tpl<0x4>;


struct Fdt_prop : Fdt_token_tpl<Fdt_token::SIZE + 2*4>
{
	struct Len     : Register<4, 32> {};
	struct Nameoff : Register<8, 32> {};

	Fdt_prop(Byte_range_ptr const &range, uint32_t len, uint32_t name_offset)
	:
		Fdt_token_tpl(range, FDT_PROP)
	{
		write<Fdt_prop::Len>(len);
		write<Fdt_prop::Nameoff>(name_offset);
	}
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
		Fdt_token start({(char *)_buffer.addr+off, _buffer.size-off}, FDT_BEGIN_NODE);
		off += Fdt_token::SIZE;
		_buffer.write(off, name.string(), name.length());
		off += (uint32_t)name.length();
		off = align_addr(off, 2);
		fn();
		Fdt_token end({(char *)_buffer.addr+off, _buffer.size-off}, FDT_END_NODE);
		off += Fdt_token::SIZE;
	};

	auto property = [&] (auto const & name, auto const & val)
	{
		_dict.add(name);
		Fdt_prop prop({(char *)_buffer.addr+off, _buffer.size-off}, (uint32_t)val.length(),
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
			         ::Array<Name, 2U>("arm,armv8-timer", "arm,armv7-timer"));
			property(Name("interrupts"),
			         ::Array<Value, 12U>(GIC_PPI, 0xd, IRQ_TYPE_LEVEL_HIGH,
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
			property(Name("ranges"),                 ::Array<Value,0U>());
			property(Name("interrupt-controller"),   ::Array<Value,0U>());
			property(Name("#address-cells"),         Value(2));
			property(Name("#redistributor-regions"), Value(1));
			property(Name("#interrupt-cells"),       Value(3));
			property(Name("#size-cells"),            Value(2));
			property(Name("reg"),
			         ::Array<Value, 8U>(0, GICD_MMIO_START, 0, GICD_MMIO_SIZE,
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
			         ::Array<Name, 2U>("arm,pl011", "arm,primecell"));
			property(Name("interrupts"),
			         ::Array<Value, 3>(GIC_SPI, PL011_IRQ-32, IRQ_TYPE_LEVEL_HIGH));
			property(Name("reg"), ::Array<Value, 4U>(0, PL011_MMIO_START,
			                                         0, PL011_MMIO_SIZE));
			property(Name("clock-names"),
			         ::Array<Name, 2U>("uartclk", "apb_pclk"));
			property(Name("clocks"), ::Array<Value, 2U>(CLK, CLK));
		});

		node(Name("memory"), [&] ()
		{
			property(Name("reg"), ::Array<Value, 4U>(0, RAM_START, 0,
			                                         (uint32_t)config.ram_size()));
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
			property(Name("linux,initrd-end"),   Value(start+(uint32_t)initrd_size));
		});

		config.for_each_virtio_device([&] (Config::Virtio_device const & dev) {
			node(Name("virtio@", dev.mmio_start), [&] ()
			{
				property(Name("interrupts"),
				         ::Array<Value, 3U>(GIC_SPI, dev.irq-32U,
				                            IRQ_TYPE_EDGE_RISING));
				property(Name("compatible"), Name("virtio,mmio"));
				property(Name("dma-coherent"),   ::Array<Value,0U>());
				property(Name("reg"),
				         ::Array<Value, 4U>(0U, (uint32_t)((addr_t)dev.mmio_start & 0xffffffff),
				                            0U, (uint32_t)dev.mmio_size));
			});
		});
	});

	Fdt_token end({(char *)_buffer.addr+off, _buffer.size-off}, FDT_END);
	off += Fdt_token::SIZE;
}


void Vmm::Fdt_generator::generate(Config const & config,
                                  void * initrd_start, size_t initrd_size)
{
	Fdt_header header({(char *)_buffer.addr, _buffer.size});
	header.write<Fdt_header::Magic>(FDT_MAGIC);
	header.write<Fdt_header::Version>(FDT_VERSION);
	header.write<Fdt_header::Last_comp_version>(FDT_COMP_VERSION);
	header.write<Fdt_header::Boot_cpuid_phys>(0);

	uint32_t off = Fdt_header::SIZE;
	header.write<Fdt_header::Off_mem_rsvmap>(off);
	Fdt_reserve_entry memory({(char *)_buffer.addr+off, _buffer.size-off});
	memory.write<Fdt_reserve_entry::Address>(0);
	memory.write<Fdt_reserve_entry::Size>(0);

	off += Fdt_reserve_entry::SIZE;
	header.write<Fdt_header::Off_dt_struct>(off);

	_generate_tree(off, config, initrd_start, initrd_size);

	header.write<Fdt_header::Size_dt_struct>((uint32_t)(off-Fdt_header::SIZE-Fdt_reserve_entry::SIZE));

	header.write<Fdt_header::Off_dt_strings>(off);
	header.write<Fdt_header::Size_dt_strings>(_dict.length());
	_dict.write([&] (addr_t o, const char * src, size_t len) {
		_buffer.write(off+o, src, len); });

	off += _dict.length();
	header.write<Fdt_header::Totalsize>(off);
}
