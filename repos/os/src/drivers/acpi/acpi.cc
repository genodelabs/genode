/*
 * \brief  ACPI parsing and PCI rewriting code
 * \author Sebastian Sumpf <sebastian.sumpf@genode-labs.com>
 * \author Alexander Boettcher
 * \date   2012-02-25
 *
 * This code parses the DSDT and SSDT-ACPI tables and extracts the PCI-bridge
 * to GSI interrupt mappings as described by "ATARE: ACPI Tables and Regular
 * Expressions, Bernhard Kauer, TU Dresden technical report TUD-FI09-09,
 * Dresden, Germany, August 2009".
 */

 /*
  * Copyright (C) 2009-2022 Genode Labs GmbH
  *
  * This file is part of the Genode OS framework, which is distributed
  * under the terms of the GNU Affero General Public License version 3.
  */

/* base includes */
#include <base/attached_io_mem_dataspace.h>
#include <base/attached_rom_dataspace.h>
#include <base/registry.h>
#include <util/misc_math.h>
#include <util/mmio.h>

/* os includes */
#include <os/reporter.h>

#include "acpi.h"
#include "memory.h"
#include "intel_opregion.h"

using namespace Genode;

/* enable debugging output */
static const bool verbose = false;


/**
 * Generic Apic structure
 */
struct Apic_struct
{
	enum Types { SRC_OVERRIDE = 2 };

	uint8_t type;
	uint8_t length;

	bool is_override() { return type == SRC_OVERRIDE; }

	Apic_struct *next() { return reinterpret_cast<Apic_struct *>((uint8_t *)this + length); }
} __attribute__((packed));


struct Mcfg_struct
{
	uint64_t base;
	uint16_t pci_seg;
	uint8_t  pci_bus_start;
	uint8_t  pci_bus_end;
	uint32_t reserved;

	Mcfg_struct *next() {
		return reinterpret_cast<Mcfg_struct *>((uint8_t *)this + sizeof(*this)); }
} __attribute__((packed));


/* ACPI spec 5.2.12.5 */
struct Apic_override : Apic_struct
{
	uint8_t  bus;
	uint8_t  irq;
	uint32_t gsi;
	uint16_t flags;
} __attribute__((packed));

struct Dmar_struct_header;

/* ACPI spec 5.2.6 */
struct Generic
{
	uint8_t  signature[4];
	uint32_t size;
	uint8_t  rev;
	uint8_t  checksum;
	uint8_t  oemid[6];
	uint8_t  oemtabid[8];
	uint32_t oemrev;
	uint8_t  creator[4];
	uint32_t creator_rev;

	void print(Genode::Output &out) const
	{
		Genode::String<7> s_oem((const char *)oemid);
		Genode::String<9> s_oemtabid((const char *)oemtabid);
		Genode::String<5> s_creator((const char *)creator);

		Genode::print(out, "OEM '", s_oem, "', table id '", s_oemtabid, "', "
		              "revision ", oemrev, ", creator '", s_creator, "' (",
		              creator_rev, ")");
	}

	uint8_t const *data() { return reinterpret_cast<uint8_t *>(this); }

	/* MADT ACPI structure */
	Apic_struct *apic_struct() { return reinterpret_cast<Apic_struct *>(&creator_rev + 3); }
	Apic_struct *end()         { return reinterpret_cast<Apic_struct *>(signature + size); }

	/* MCFG ACPI structure */
	Mcfg_struct *mcfg_struct() { return reinterpret_cast<Mcfg_struct *>(&creator_rev + 3); }
	Mcfg_struct *mcfg_end()    { return reinterpret_cast<Mcfg_struct *>(signature + size); }

	Dmar_struct_header *dmar_header() { return reinterpret_cast<Dmar_struct_header *>(this); }
} __attribute__((packed));


struct Dmar_common : Genode::Mmio<0x4>
{
	struct Type : Register<0x0, 16> {
		enum { DRHD= 0U, RMRR = 0x1U, ATSR = 0x2U, RHSA = 0x3U }; };
	struct Length : Register<0x2, 16> { };

	Dmar_common(Byte_range_ptr const &range) : Mmio(range) { }
};


/* DMA Remapping Reporting Structure - Intel VT-d IO Spec - 8.1. */
struct Dmar_struct_header : Generic
{
	enum { INTR_REMAP_MASK = 0x1U };

	uint8_t width;
	uint8_t flags;
	uint8_t reserved[10];

	/* DMAR Intel VT-d structures */
	addr_t dmar_entry_start() { return reinterpret_cast<addr_t>(&creator_rev + 4); }
	addr_t dmar_entry_end() { return reinterpret_cast<addr_t>(signature + size); }

	template <typename FUNC>
	void apply(FUNC const &func = [] () { } )
	{
		addr_t addr = dmar_entry_start();
		while (addr < dmar_entry_end()) {
			Dmar_common dmar({(char *)addr, dmar_entry_end() - addr});

			func(dmar);

			addr = dmar.base() + dmar.read<Dmar_common::Length>();
		}
	}

	struct Dmar_struct_header * clone(Genode::Allocator &alloc)
	{
		size_t const size = dmar_entry_end() - reinterpret_cast<addr_t>(this);
		char * clone = new (&alloc) char[size];
		memcpy(clone, this, size);

		return reinterpret_cast<Dmar_struct_header *>(clone);
	}
} __attribute__((packed));


/* Intel VT-d IO Spec - 8.3.1. */
struct Device_scope : Genode::Mmio<0x6>
{
	Device_scope(Byte_range_ptr const &range) : Mmio(range) { }

	struct Type   : Register<0x0, 8> { enum { PCI_END_POINT = 0x1 }; };
	struct Length : Register<0x1, 8> { };
	struct Bus    : Register<0x5, 8> { };

	struct Path : Genode::Mmio<0x2>
	{
		Path(Byte_range_ptr const &range) : Mmio(range) { }

		struct Dev  : Register<0, 8> { };
		struct Func : Register<1, 8> { };

		uint8_t dev()  const { return read<Dev >(); }
		uint8_t func() const { return read<Func>(); }
	};

	void for_each_path(auto const &fn) const
	{
		auto const length = read<Length>();

		unsigned offset = Device_scope::SIZE;

		while (offset < length) {
			Path const path(Mmio::range_at(offset));

			fn(path);

			offset += Path::SIZE;
		}
	}
};

/* DMA Remapping Hardware Definition - Intel VT-d IO Spec - 8.3. */
struct Dmar_drhd : Genode::Mmio<0x10>
{
	struct Length  : Register<0x2, 16> { };
	struct Flags   : Register<0x4,  8> { };
	struct Size    : Register<0x5,  8> {
		struct Num_pages : Bitfield<0,4> { }; };
	struct Segment : Register<0x6, 16> { };
	struct Phys    : Register<0x8, 64> { };

	Dmar_drhd(Byte_range_ptr const &range) : Mmio(range) { }

	template <typename FUNC>
	void apply(FUNC const &func = [] () { } )
	{
		off_t offset = 16;
		while (offset < read<Length>()) {
			Device_scope scope(Mmio::range_at(offset));

			func(scope);

			offset += scope.read<Device_scope::Length>();
		}
	}
};

/* DMA Remapping Reporting structure - Intel VT-d IO Spec - 8.3. */
struct Dmar_rmrr : Genode::Mmio<0x18>
{
	struct Length : Register<0x02, 16> { };
	struct Base   : Register<0x08, 64> { };
	struct Limit  : Register<0x10, 64> { };

	Dmar_rmrr(Byte_range_ptr const &range) : Mmio(range) { }

	template <typename FUNC>
	void apply(FUNC const &func = [] () { } )
	{
		addr_t offset = 24;
		while (offset < read<Length>()) {
			Device_scope scope(Mmio::range_at(offset));

			func(scope);

			offset += scope.read<Device_scope::Length>();
		}
	}
};

/* I/O Virtualization Definition Blocks for AMD IO-MMU */
struct Ivdb : Genode::Mmio<0x4>
{
	struct Type   : Register<0x00, 8>  { };
	struct Length : Register<0x02, 16> { };

	Ivdb(Byte_range_ptr const &range) : Mmio(range) { }
};


struct Ivdb_entry : public List<Ivdb_entry>::Element
{
	unsigned const type;

	Ivdb_entry(unsigned t) : type (t) { }

	template <typename FUNC>
	static void for_each(FUNC const &func)
	{
		for (Ivdb_entry *entry = list()->first(); entry; entry = entry->next()) {
			func(*entry);
		}
	}

	static List<Ivdb_entry> *list()
	{
		static List<Ivdb_entry> _list;
		return &_list;
	}
};


/* I/O Virtualization Reporting Structure (IVRS) for AMD IO-MMU */
struct Ivrs : Genode::Mmio<0x28>
{
	struct Length : Register<0x04, 32> { };
	struct Ivinfo : Register<0x24, 32> {
		struct Dmar : Bitfield<1, 1> { };
	};

	static constexpr unsigned min_size() { return 0x30; }

	Ivrs(Byte_range_ptr const &range) : Mmio(range) { }

	void parse(Allocator &alloc)
	{
		uint32_t offset = 0x30;
		while (offset < read<Ivrs::Length>()) {
			bool dmar = Ivinfo::Dmar::get(read<Ivinfo::Dmar>());
			if (dmar)
				Genode::warning("Predefined regions should be added to IOMMU");

			Ivdb ivdb(Mmio::range_at(offset));

			uint32_t const type = ivdb.read<Ivdb::Type>();
			uint32_t const size = ivdb.read<Ivdb::Length>();

			Ivdb_entry::list()->insert(new (&alloc) Ivdb_entry(type));

			offset += size;
		}
	}
};


struct Fadt_reset : Genode::Mmio<0x88>
{
	Fadt_reset(Byte_range_ptr const &range) : Mmio(range) { }

	struct Features       : Register<0x70, 32> {
		/* Table 5-35 Fixed ACPI Description Table Fixed Feature Flags */
		struct Reset : Bitfield<10, 1> { };
	};
	struct Reset_type     : Register<0x74, 32> {
		/* ACPI spec - 5.2.3.2 Generic Address Structure */
		struct Address_space : Bitfield<0, 8> { enum { SYSTEM_IO = 1 }; };
		struct Access_size   : Bitfield<24,8> {
			enum { UNDEFINED = 0, BYTE = 1, WORD = 2, DWORD = 3, QWORD = 4}; };
	};
	struct Reset_reg      : Register<0x78, 64> { };
	struct Reset_value    : Register<0x80,  8> { };

	uint16_t io_port_reset() const { return read<Reset_reg>() & 0xffffu; }
	uint8_t  reset_value()   const { return read<Fadt_reset::Reset_value>(); }
};


/* Fixed ACPI description table (FADT) */
struct Fadt : Genode::Mmio<0x30>
{
	Fadt(Byte_range_ptr const &range) : Mmio(range) { }

	struct Dsdt    : Register<0x28, 32> { };
	struct Sci_int : Register<0x2e, 16> { };

	void detect_io_reset(auto const &range, auto const &fn) const
	{
		if (range.num_bytes < Fadt_reset::SIZE)
			return;

		Fadt_reset const reset(range);

		if (!read<Fadt_reset::Features::Reset>())
			return;

		if (read<Fadt_reset::Reset_type::Address_space>() == Fadt_reset::Reset_type::Address_space::SYSTEM_IO)
			fn(reset);
	}
};


class Dmar_entry : public List<Dmar_entry>::Element
{
	private:

		Dmar_struct_header *_header;

	public:

		Dmar_entry(Dmar_struct_header * h) : _header(h) { }

		template <typename FUNC>
		void apply(FUNC const &func = [] () { } ) { _header->apply(func); }

		static List<Dmar_entry> *list()
		{
			static List<Dmar_entry> _list;
			return &_list;
		}
};

/**
 * List that holds interrupt override information
 */
class Irq_override : public List<Irq_override>::Element
{
	private:

		uint32_t _irq;   /* source IRQ */
		uint32_t _gsi;   /* target GSI */
		uint32_t _flags; /* interrupt flags */

	public:

		Irq_override(uint32_t irq, uint32_t gsi, uint32_t flags)
		: _irq(irq), _gsi(gsi), _flags(flags) { }

		static List<Irq_override> *list()
		{
			static List<Irq_override> _list;
			return &_list;
		}

		uint32_t irq()               const { return _irq; }
		uint32_t gsi()               const { return _gsi; }
		uint32_t flags()             const { return _flags; }
};


/**
 * List that holds the result of the mcfg table parsing which are pointers
 * to the extended pci config space - 4k for each device.
 */
class Pci_config_space : public List<Pci_config_space>::Element
{
	public:

		uint32_t _bdf_start;
		uint32_t _func_count;
		addr_t   _base;

		Pci_config_space(uint32_t bdf_start, uint32_t func_count, addr_t base)
		:
			_bdf_start(bdf_start), _func_count(func_count), _base(base) { }

		static List<Pci_config_space> *list()
		{
			static List<Pci_config_space> _list;
			return &_list;
		}

		struct Config_space : Mmio<0x100>
		{
			struct Vendor : Register<0x00, 16> { enum { INTEL = 0x8086 }; };
			struct Class  : Register<0x0b,  8> { enum { DISPLAY = 0x3 }; };
			struct Asls   : Register<0xfc, 32> { };

			Config_space(Byte_range_ptr const &range) : Mmio(range) { }
		};

		struct Opregion : Mmio<0x3c6>
		{
			struct Minor : Register<0x16, 8> { };
			struct Major : Register<0x17, 8> { };
			struct MBox  : Register<0x58, 32> {
				struct Asle : Bitfield<2, 1> { };
			};
			struct Asle_ardy : Register<0x300, 32> { };
			struct Asle_rvda : Register<0x3ba, 64> { };
			struct Asle_rvds : Register<0x3c2, 32> { };

			Opregion(Byte_range_ptr const &range) : Mmio(range) { }
		};

		static void intel_opregion(Env &env)
		{
			for (auto *e = list()->first(); e; e = e->next()) {
				if (e->_bdf_start != 0u) /* BDF 0:0.0 */
					continue;

				auto const config_offset = 8u * 2; /* BDF 0:2.0 */
				auto const config_size   = 4096;

				if (e->_func_count <= config_offset)
					continue;

				Attached_io_mem_dataspace pci_config(env, e->_base +
				                                     config_offset * config_size,
				                                     config_size);
				Config_space device({pci_config.local_addr<char>(), config_size});

				if ((device.read<Config_space::Vendor>() != Config_space::Vendor::INTEL) ||
				    (device.read<Config_space::Class>()  != Config_space::Class::DISPLAY))
					continue;

				enum {
					OPREGION_SIZE = 2 * 4096
				};

				addr_t const phys_asls = device.read<Config_space::Asls>();
				if (!phys_asls)
					continue;

				addr_t asls_size = OPREGION_SIZE;

				{
					Attached_io_mem_dataspace map_asls(env, phys_asls, asls_size);
					Opregion opregion({map_asls.local_addr<char>(), asls_size});

					auto const rvda = opregion.read<Opregion::Asle_rvda>();
					auto const rvds = opregion.read<Opregion::Asle_rvds>();

					if (opregion.read<Opregion::MBox::Asle>() &&
					    opregion.read<Opregion::Major>() >= 2 && rvda && rvds) {

						/* 2.0 rvda is physical, 2.1+ rvda is relative offset */
						if (opregion.read<Opregion::Major>() > 2 ||
						    opregion.read<Opregion::Minor>() >= 1) {

							if (rvda > asls_size)
								asls_size += rvda - asls_size;
							asls_size += opregion.read<Opregion::Asle_rvds>();
						} else {
							warning("rvda/rvds unsupported case");
						}
					}
				}

				/*
				 * Intel_opregion requires access to the opregion memory later
				 * on used by acpica. Therefore the code must be executed here
				 * and finished, before the acpi report is sent.
				 * With a valid acpi report the acpica driver starts to run
				 * and would collide with Intel_opregion.
				 */
				static Acpi::Intel_opregion opregion_report { env, phys_asls,
				                                              asls_size };
			}
		}
};



/**
 * ACPI table wrapper that for mapping tables to this address space
 */
class Table_wrapper
{
	public:

		struct Info : Registry<Info>::Element
		{
			String<5> name;
			addr_t    addr;
			size_t    size;

			Info(Registry<Info> &registry,
			     char const *name, addr_t addr, size_t size)
			:
				Registry<Info>::Element(registry, *this),
				name(name), addr(addr), size(size) { }
		};

	private:

		addr_t             _base;    /* table base address */
		Generic           *_table;   /* pointer to table header */
		char               _name[5]; /* table name */

		/* return offset of '_base' to page boundary */
		addr_t _offset() const              { return (_base & 0xfff); }

		/* compare table name with 'name' */
		bool _cmp(char const *name) const { return !memcmp(_table->signature, name, 4); }

	public:

		/**
		 * Accessors
		 */
		Generic* operator -> ()  { return _table; }
		Generic* table()         { return _table; }
		char const *name() const { return _name; }

		/**
		 * Determine maximal number of potential elements
		 */
		template <typename T>
		addr_t entry_count(T *)
		{
			return (_table->size - sizeof (Generic)) / sizeof(T);
		}

		/**
		 * Create ACPI checksum (is zero if valid)
		 */
		static uint8_t checksum(uint8_t *table, uint32_t count)
		{
			uint8_t sum = 0;
			while (count--)
				sum += table[count];
			return sum;
		}

		bool valid() { return !checksum((uint8_t *)_table, _table->size); }

		bool is_ivrs() const { return _cmp("IVRS");}

		/**
		 * Is this the FACP table
		 */
		bool is_facp() const { return _cmp("FACP");}

		/**
		 * Is this a MADT table
		 */
		bool is_madt() { return _cmp("APIC"); }

		/**
		 * Is this a MCFG table
		 */
		bool is_mcfg() { return _cmp("MCFG"); }

		/**
		 * Look for DSDT and SSDT tables
		 */
		bool is_searched() const { return _cmp("DSDT") || _cmp("SSDT"); }

		/**
		 * Is this a DMAR table
		 */
		bool is_dmar() { return _cmp("DMAR"); }

		/**
		 * Parse override structures
		 */
		void parse_madt(Genode::Allocator &alloc)
		{
			Apic_struct *apic = _table->apic_struct();
			for (; apic < _table->end(); apic = apic->next()) {
				if (!apic->is_override())
					continue;

				Apic_override *o = static_cast<Apic_override *>(apic);
				
				Genode::log("MADT IRQ ", o->irq, " -> GSI ", (unsigned)o->gsi, " "
				            "flags: ", (unsigned)o->flags);
				
				Irq_override::list()->insert(new (&alloc)
					Irq_override(o->irq, o->gsi, o->flags));
			}
		}

		void parse_mcfg(Genode::Allocator &alloc)  const
		{
			Mcfg_struct *mcfg = _table->mcfg_struct();
			for (; mcfg < _table->mcfg_end(); mcfg = mcfg->next()) {
				Genode::log("MCFG BASE ", Genode::Hex(mcfg->base),    " "
				            "seg ", Genode::Hex(mcfg->pci_seg),       " "
				            "bus ", Genode::Hex(mcfg->pci_bus_start), "-",
				                    Genode::Hex(mcfg->pci_bus_end));

				/* bus_count * up to 32 devices * 8 function per device * 4k */
				uint32_t bus_count  = mcfg->pci_bus_end - mcfg->pci_bus_start + 1;
				uint32_t func_count = bus_count * 32 * 8;
				uint32_t bus_start  = mcfg->pci_bus_start * 32 * 8;

				Pci_config_space::list()->insert(
					new (&alloc) Pci_config_space(bus_start, func_count, mcfg->base));
			}
		}

		void parse_dmar(Genode::Allocator &alloc) const
		{
			Dmar_struct_header *head = _table->dmar_header();
			Genode::log(head->width + 1, " bit DMA physical addressable",
			            head->flags & Dmar_struct_header::INTR_REMAP_MASK ?
			                              " , IRQ remapping supported" : "");

			head->apply([&] (Dmar_common const &dmar) {
			 Genode::log("DMA remapping structure type=", dmar.read<Dmar_common::Type>());
			});

			Dmar_entry::list()->insert(new (&alloc) Dmar_entry(head->clone(alloc)));
		}

		Table_wrapper(Acpi::Memory &memory, addr_t base,
		              Registry<Info> &registry, Allocator &heap)
		: _base(base), _table(0)
		{
			/* make table header accessible */
			_table = reinterpret_cast<Generic *>(memory.map_region(base, 8));

			/* table size is known now - make it completely accessible (in place) */
			memory.map_region(base, _table->size);

			memset(_name, 0, 5);
			memcpy(_name, _table->signature, 4);

			new (heap) Info(registry, name(), _base, _table->size);

			if (verbose)
				Genode::log("table mapped '", Genode::Cstring(_name), "' at ", _table, " "
				            "(from ", Genode::Hex(_base), ") "
				            "size ",  Genode::Hex(_table->size));
		}
};


/**
 * PCI routing information
 */
class Pci_routing : public List<Pci_routing>::Element
{
	private:

		uint32_t _adr; /* address (6.1.1) */
		uint32_t _pin; /* IRQ pin */
		uint32_t _gsi; /* global system interrupt */

	public:

		Pci_routing(uint32_t adr, uint32_t pin, uint32_t gsi)
		: _adr(adr), _pin(pin), _gsi(gsi) { }

		/**
		 * Compare BDF of this object to given bdf
		 */
		bool match_bdf(uint32_t bdf) const { return (_adr >> 16) == ((bdf >> 3) & 0x1f); }

		/**
		 * Accessors
		 */
		uint32_t pin()    const { return _pin; }
		uint32_t gsi()    const { return _gsi; }
		uint32_t device() const { return _adr >> 16; }

		void print(Genode::Output &out) const
		{
			Genode::print(out, "adr: ", Genode::Hex(_adr), " "
			                   "pin: ", Genode::Hex(_pin), " "
			                   "gsi: ", Genode::Hex(_gsi));
		}

		/* debug */
		void dump()
		{
			if (verbose)
				Genode::log("Pci: ", *this);
		}
};

/* set during ACPI Table walk to valid value */
enum { INVALID_ROOT_BRIDGE = 0x10000U };
static unsigned root_bridge_bdf = INVALID_ROOT_BRIDGE;

/**
 * A table element (method, device, scope or name)
 */
class Element : private List<Element>::Element
{
	private:

		friend class List<Element>;

		Element &operator = (Element const &);

		uint8_t        _type     = 0;       /* the type of this element */
		uint32_t       _size     = 0;       /* size in bytes */
		uint32_t       _size_len = 0;       /* length of size in bytes */
		char           _name[64];           /* name of element */
		uint32_t       _name_len = 0;       /* length of name in bytes */
		uint32_t       _bdf      = 0;       /* bus device function */
		uint8_t const *_data     = nullptr; /* pointer to the data section */
		uint32_t       _para_len = 0;       /* parameters to be skipped */
		bool           _valid    = false;   /* true if this is a valid element */
		bool           _routed   = false;   /* has the PCI information been read */

		/* list of PCI routing elements for this element */
		List<Pci_routing> _pci { };

		/* packages we are looking for */
		enum { DEVICE = 0x5b, SUB_DEVICE = 0x82, DEVICE_NAME = 0x8, SCOPE = 0x10, METHOD = 0x14, PACKAGE_OP = 0x12 };

		/* name prefixes */
		enum { ROOT_PREFIX = 0x5c, PARENT_PREFIX = 0x5e, DUAL_NAME_PREFIX = 0x2e, MULTI_NAME_PREFIX = 0x2f, };

		/* default signature length of ACPI elements */
		enum { NAME_LEN = 4 };

		/* ComputationalData - ACPI 19.2.3 */
		enum { BYTE_PREFIX = 0xa, WORD_PREFIX = 0xb, DWORD_PREFIX = 0xc, QWORD_PREFIX=0xe };

		/* return address of 'name' in Element */
		uint8_t const *_name_addr() const { return _data + _size_len + 1; }

		/**
		 * See ACPI spec 5.4
		 */
		uint32_t _read_size_encoding()
		{
			/*
			 * Most sig. 2 bits set number of bytes (1-4), next two bits are only used
			 * in one byte encoding, if bits are set in all two areas it is no valid
			 * size
			 */
			uint32_t encoding = _data[1];
			return ((encoding & 0xc0) && (encoding & 0x30)) ? 0 :  1 + (encoding >> 6);
		}

		/**
		 * See ACPI spec. 5.4
		 */
		void _read_size()
		{
			_size = _data[1] & 0x3f;
			for (uint32_t i = 1; i < _read_size_encoding(); i++)
				_size += _data[i + 1] << (8 * i - 4);
		}

		/**
		 * Return the length of the name prefix
		 */
		uint32_t _prefix_len(uint8_t const *name)
		{
			uint8_t const *n = name;
			if (*n == ROOT_PREFIX)
				n++;
			else while (*n == PARENT_PREFIX)
				n++;

			if (*n == DUAL_NAME_PREFIX)
				n++;
			else if (*n == MULTI_NAME_PREFIX)
				n += 2;

			return n - name;
		}

		/**
		 * Return length of a given name
		 */
		uint32_t _read_name_len(uint8_t const *name = 0)
		{
			uint8_t const *name_addr = name ? name : _name_addr();
			uint8_t const *n = name_addr;

			/* skip prefixes (ACPI 18.2.1) */
			if (*n == ROOT_PREFIX)
				n++;
			else while (*n == PARENT_PREFIX)
				n++;

			/* two names follow */
			if (*n == DUAL_NAME_PREFIX) {
				/* check first + second name */
				if (_check_name_segment(n + 1) && _check_name_segment(n + NAME_LEN + 1))

					/* prefixes + dual prefixe + 2xname */
					return n - name_addr + 1 + 2 * NAME_LEN;
			}

			/* multiple name segments ('MultiNamePrefix SegCount NameSeg(SegCount)') */
			else if (*n == MULTI_NAME_PREFIX) {
				uint32_t i;
				for (i = 0; i < n[1]; i++)

					/* check segment */
					if (!_check_name_segment(n + 2 + NAME_LEN * i))
						return 0;

				if (i)

					/* prefix + multi prefix + seg. count + name length * seg. count */
					return n - name_addr + 2 + NAME_LEN * i;
			}

			/* single name segment */
			else if (_check_name_segment(n))

				/* prefix + name */
				return n - name_addr + NAME_LEN;

			return n - name_addr;
		}

		/**
		 * Check if name is a valid ASL name (18.2.1)
		 */
		bool _check_name_segment(uint8_t const *name)
		{
			for (uint32_t i = 0; i < NAME_LEN; i++) {
				uint8_t c = name[i];
				if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' ||
				      (i > 0 && c >= '0' && c <= '9')))
					return false;
			}

			return true;
		}

		/**
		 * Return parent of this element
		 */
		Element *_parent(bool update_size = false)
		{
			Element *parent = list()->first();

			/* set length of previous element */
			if (update_size && parent && !parent->size())
				parent->size(_data - parent->data());

			/* find parent */
			for (; parent; parent = parent->next())

				/* parent surrounds child */
				if ((parent->data() < _data) && ((parent->data() + parent->size()) > _data))
					break;

			return parent;
		}

		/**
		 * Set the name of this object
		 */
		void _set_name()
		{
			uint8_t const *name = _name_addr();
			Element *parent     = _parent(true);
			uint32_t prefix_len = _prefix_len(name);

			if (_name_len <= prefix_len) {
				_name_len = 0;
				return;
			}

			_name_len -= prefix_len;

			/* is absolute name */
			if (*name == ROOT_PREFIX || !parent) {
				memcpy(_name, name + prefix_len, min(sizeof(_name), _name_len));
			}
			else {
				/* skip parts */
				int parent_len = parent->_name_len;
				parent_len = (parent_len < 0) ? 0 : parent_len;

				/* skip parent prefix */
				for (uint32_t p = 0; name[p] == PARENT_PREFIX; p++, parent_len -= NAME_LEN) ;

				if (_name_len + parent_len > sizeof(_name)) {
					Genode::error("name is not large enough");
					throw -1;
				}

				memcpy(_name, parent->_name, parent_len);
				memcpy(_name + parent_len, name + prefix_len, _name_len);

				_name_len += parent_len;
			}
		}

		/**
		 * Compare 'sub_string' with '_name'
		 */
		Element *_compare(char const *sub_string, uint32_t skip = 0)
		{
			size_t sub_len = strlen(sub_string);

			Element *other = list()->first();
			while (other) {

				if ((other->_name_len == _name_len + sub_len - skip) &&

				    /* compare our part */
				    !(memcmp(other->_name, _name, _name_len - skip)) &&

				    /* compare other part */
				    !memcmp(other->_name + _name_len - skip, sub_string, sub_len))
					return other;

				other = other->next();
			}
			return 0;
		}

		/**
		 * Read value of element that matches 'sub_string'
		 */
		uint32_t _value(char const *sub_string)
		{
			Element *other = _compare(sub_string);

			if (!other || !other->is_device_name())
				return 0;

			uint32_t data_len;
			uint32_t data = other->_read(other->_data + other->_read_name_len() + 1, data_len);

			return data_len ? data : 0;
		}

		/**
		 * Read data if return length of data read
		 */
		uint32_t _read(uint8_t const *data, uint32_t &length)
		{
			switch (data[0]) {
			case 0:     length = 1; return 0;
			case 1:     length = 1; return 1;
			case 0xff:  length = 1; return 0xffffffff;
			case 0x0a:  length = 2; return data[1];
			case 0x0b:  length = 3; return data[1] | (data[2] << 8);
			case 0x0c:  length = 5; return data[1] | (data[2] << 8) | (data[3] << 16)  | (data[4] << 24);
			default:    length = 0; return 0;
			}
		}

		/**
		 * Try to find an element containing four values of data
		 */
		Element _packet(uint8_t const *table, long len)
		{
			for (uint8_t const *data = table; data < table + len; data++) {
				Element e(data, true);
				if (e.valid())
					return Element(data, true);
			}
			return Element();
		}

		/**
		 * Try to locate _PRT table and its GSI values for device
		 * (data has to be located within the device data)
		 */
		void _direct_prt(Genode::Allocator &alloc, Element *dev)
		{
			uint32_t len = 0;

			for (uint32_t offset = 0; offset < size(); offset += len) {
				len = 1;

				/* search for four value packet */
				Element e = _packet(_data + offset, size() - offset);

				if (!e.valid())
					continue;

				/* read values */
				uint32_t val[4];
				uint32_t read_offset = 0;
				uint32_t i;
				for (i = 0; i < 4; i++) {
					val[i] = e._read(e.data() + e.size_len() + 2 + read_offset, len);
					if (!len) break;
					read_offset += len;
				}

				if (i == 4) {
					Pci_routing * r = new (&alloc) Pci_routing(val[0], val[1], val[3]);

					/* set _ADR, _PIN, _GSI */
					dev->pci_list().insert(r);
					dev->pci_list().first()->dump();
				}

				len = len ? (e.data() - (_data + offset)) + e.size() : 1;
			}
		}

		/**
		 * Search for _PRT outside of device
		 */
		void _indirect_prt(Genode::Allocator &alloc, Element *dev)
		{
			uint32_t name_len;
			uint32_t found = 0;
			for (uint32_t offset = size_len(); offset < size(); offset += name_len) {
				name_len = _read_name_len(_data + offset);
				if (name_len) {
					if (!found++)
						continue;

					char name[name_len + 1];
					memcpy(name, _data + offset, name_len);
					name[name_len] = 0;

					if (verbose)
						Genode::log("indirect ", Genode::Cstring(name));

					for (uint32_t skip = 0; skip <= dev->_name_len / NAME_LEN; skip++) {
						Element *e = dev->_compare(name, skip * NAME_LEN);
						if (e)
							e->_direct_prt(alloc, dev);
					}
				}
				else
					name_len = 1;
			}
		}

		Element(uint8_t const *data = 0, bool package_op4 = false)
		:
			_type(0), _size(0), _size_len(0),  _name_len(0), _bdf(0),
			_data(data), _para_len(0), _valid(false), _routed(false)
		{
			_name[0] = '\0';

			if (!data)
				return;

			/* special handle for four value packet */
			if (package_op4) {

				/* scan for data package with four entries */
				if (data[0] != PACKAGE_OP)
					return;

				/* area there four entries */
				if (!(_size_len = _read_size_encoding()) || _data[1 + _size_len] != 0x04)
					return;

				_read_size();
				_valid = true;
				return;
			}

			switch (data[0]) {

			case DEVICE:

				data++; _data++;

				if (data[0] != SUB_DEVICE)
					return;

				[[fallthrough]];

			case SCOPE:
			case METHOD:

				if (!(_size_len = _read_size_encoding()))
					return;

				_read_size();

				if (_size) {

					/* check if element is larger than parent */
					Element *p = _parent();
					for (; p; p = p->_parent())
						if (p->_size && p->_size < _size)
							return;
				}

				[[fallthrough]];

			case DEVICE_NAME:
				/* ACPI 19.2.5.1 - NameOp NameString DataRefObject */

				if (!(_name_len = _read_name_len()))
					return;

				_valid = true;

				/* ACPI 19.2.3 DataRefObject */
				switch (data[_name_len + 1]) {
					case QWORD_PREFIX: _para_len += 4; [[fallthrough]];
					case DWORD_PREFIX: _para_len += 2; [[fallthrough]];
					case  WORD_PREFIX: _para_len += 1; [[fallthrough]];
					case  BYTE_PREFIX: _para_len += 1; [[fallthrough]];
					default: _para_len += 1;
				}

				/* set absolute name of this element */
				_set_name();
				_type = data[0];

				dump();

			default:

				return;
			}
		}

		/**
		 * Copy constructor
		 */
		Element(Element const &other)
		{
			_type     = other._type;
			_size     = other._size;
			_size_len = other._size_len;
			memcpy(_name, other._name, sizeof(_name));
			_name_len = other._name_len;
			_bdf      = other._bdf;
			_data     = other._data;
			_para_len = other._para_len;
			_valid    = other._valid;
			_routed   = other._routed;
			_pci      = other._pci;
		}

		bool is_device_name() { return _type == DEVICE_NAME; }

		/**
		 * Debugging
		 */
		void dump()
		{
			if (!verbose)
				return;

			char n[_name_len + 1];
			memcpy(n, _name, _name_len);
			n[_name_len] = 0;
			Genode::log("Found package ", Genode::Hex(_data[0]), " "
			            "size: ", _size, " name_len: ", _name_len, " "
			            "name: ", Genode::Cstring(n));
		}

	public:

		using List<Element>::Element::next;

		virtual ~Element() { }

		/**
		 * Accessors
		 */
		uint32_t       size() const        { return _size; }
		void           size(uint32_t size) { _size = size; }
		uint32_t       size_len() const    { return _size_len; }
		uint8_t const *data() const        { return _data; }
		bool           valid() const       { return _valid; }
		uint32_t       bdf() const         { return _bdf; }
		bool           is_device() const   { return _type == SUB_DEVICE; }


		static bool supported_acpi_format()
		{
			/* check if _PIC method is present */
			for (Element *e = list()->first(); e; e = e->next())
				if (e->_name_len == 4 && !memcmp(e->_name, "_PIC", 4))
					return true;
			return false;
		}

		/**
		 * Return list of elements
		 */
		static List<Element> *list()
		{
			static List<Element> _list;
			return &_list;
		}

		static void clean_list(Genode::Allocator &alloc)
		{
			unsigned long freed_up = 0;

			Element * element = list()->first();
			while (element) {
				if (element->is_device() || (element->_name_len == 4 &&
				                             !memcmp(element->_name, "_PIC", 4))) {
					element = element->next();
					continue;
				}

				freed_up += sizeof(*element);

				Element * next = element->next();
				Element::list()->remove(element);
				destroy(&alloc, element);
				element = next;
			}

			if (verbose)
				Genode::log("Freeing up memory of elements - ", freed_up, " bytes");
		}

		/**
		 * Return list of PCI information for this element
		 */
		List<Pci_routing> & pci_list() { return _pci; }

		/**
		 * Parse elements of table
		 */
		static void parse(Genode::Allocator &alloc, Generic *table)
		{
			uint8_t const *data = table->data();
			for (; data < table->data() + table->size; data++) {
				Element e(data);

				if (!e.valid() || !e._name_len)
					continue;

				if (data + e.size() > table->data() + table->size)
					break;

				Element *i = new (&alloc) Element(e);
				list()->insert(i);

				/* skip header */
				data += e.size_len();
				/* skip name */
				data += NAME_LEN;

				/* skip rest of structure if known */
				if (e.is_device_name())
					data += e._para_len;
			}

			parse_bdf(alloc);
		}

		/**
		 * Parse BDF and GSI information
		 */
		static void parse_bdf(Genode::Allocator &alloc)
		{
			for (Element *e = list()->first(); e; e = e->next()) {

				if (!e->is_device() || e->_routed)
					continue;

				/* address (high word = device, low word = function) (6.1.1) */
				uint32_t adr = e->_value("_ADR");

				/* Base bus number (6.5.5) */
				uint32_t bbn = e->_value("_BBN");

				/* Segment object located under host bridge (6.5.6) */
				uint32_t seg = e->_value("_SEG");

				/* build BDF */
				e->_bdf = (seg << 16) | (bbn << 8) | (adr >> 16) << 3 | (adr & 0xffff);

				/* add routing */
				Element *prt = e->_compare("_PRT");
				if (prt) prt->dump();

				if (prt) {
					uint32_t const hid= e->_value("_HID");
					uint32_t const cid= e->_value("_CID");
					if (hid == 0x80ad041 || cid == 0x80ad041 || // "PNP0A08" PCI Express root bridge
					    hid == 0x30ad041 || cid == 0x30ad041) { // "PNP0A03" PCI root bridge
						root_bridge_bdf = e->_bdf;
					}

					if (verbose)
						Genode::log("Scanning device ", Genode::Hex(e->_bdf));

					prt->_direct_prt(alloc, e);
					prt->_indirect_prt(alloc, e);
				}

				e->_routed = true;
			}
		}
};


/**
 * Locate and parse ACPI tables we are looking for
 */
class Acpi_table
{
	private:

		Genode::Env       &_env;
		Genode::Allocator &_heap;
		Acpi::Memory       _memory;

		struct Reset_info
		{
			Genode::uint16_t io_port;
			Genode::uint8_t  value;
		};

		Genode::Constructible<Reset_info> _reset_info { };
		unsigned short                    _sci_int    { };
		bool                              _sci_int_valid { };

		Registry<Table_wrapper::Info> _table_registry { };

		/* BIOS range to scan for RSDP */
		enum { BIOS_BASE = 0xe0000, BIOS_SIZE = 0x20000 };

		Genode::Constructible<Genode::Attached_io_mem_dataspace> _mmio { };

		/**
		 * Search for RSDP pointer signature in area
		 */
		uint8_t *_search_rsdp(uint8_t *area, size_t const area_size)
		{
			for (addr_t addr = 0; area && addr < area_size; addr += 16)
				if (!memcmp(area + addr, "RSD PTR ", 8) &&
				    !Table_wrapper::checksum(area + addr, 20))
					return area + addr;

			throw -2;
		}

		/**
		 * Return 'Root System Descriptor Pointer' (ACPI spec 5.2.5.1)
		 *
		 * As a side effect, the function initializes the '_mmio' member.
		 */
		uint8_t *_rsdp()
		{
			/* try BIOS area (0xe0000 - 0xfffffh)*/
			try {
				_mmio.construct(_env, BIOS_BASE, BIOS_SIZE);
				return _search_rsdp(_mmio->local_addr<uint8_t>(), BIOS_SIZE);
			}
			catch (...) { }

			/* search EBDA (BIOS addr + 0x40e) */
			try {
				/* read RSDP base address from EBDA */
				size_t const size = 0x1000;

				_mmio.construct(_env, 0x0, size);
				uint8_t const * const ebda = _mmio->local_addr<uint8_t const>();
				unsigned short const rsdp_phys =
					(*reinterpret_cast<unsigned short const *>(ebda + 0x40e)) << 4;

				_mmio.construct(_env, rsdp_phys, size);
				return _search_rsdp(_mmio->local_addr<uint8_t>(), size);
			}
			catch (...) { }

			return nullptr;
		}

		template <typename T>
		void _parse_tables(T * entries, uint32_t const count)
		{
			/* search for SSDT and DSDT tables */
			for (uint32_t i = 0; i < count; i++) {
				uint32_t dsdt = 0;
				try {
					Table_wrapper table(_memory, entries[i], _table_registry, _heap);

					if (!table.valid()) {
						Genode::error("ignoring table '", table.name(),
						              "' - checksum error");
						continue;
					}

					if (table.is_ivrs() && Ivrs::min_size() <= table->size) {
						log("Found IVRS");

						Ivrs ivrs({(char *)table->signature, table->size});
						ivrs.parse(_heap);
					}

					if (table.is_facp() && (Fadt::SIZE <= table->size)) {
						Byte_range_ptr const range((char *)table->signature,
						                           table->size);

						Fadt fadt(range);

						dsdt = fadt.read<Fadt::Dsdt>();

						_sci_int = fadt.read<Fadt::Sci_int>();
						_sci_int_valid = true;

						fadt.detect_io_reset(range, [&](auto const &reset) {
							_reset_info.construct(Reset_info { .io_port = reset.io_port_reset(),
							                                   .value   = reset.reset_value() });
						});
					}

					if (table.is_searched()) {

						if (verbose)
							Genode::log("Found ", table.name());

						Element::parse(_heap, table.table());
					}

					if (table.is_madt()) {
						Genode::log("Found MADT");

						table.parse_madt(_heap);
					}
					if (table.is_mcfg()) {
						Genode::log("Found MCFG");

						table.parse_mcfg(_heap);
					}
					if (table.is_dmar()) {
						Genode::log("Found DMAR");

						table.parse_dmar(_heap);
					}
				} catch (Acpi::Memory::Unsupported_range &) { }

				if (!dsdt)
					continue;

				try {
					Table_wrapper table(_memory, dsdt, _table_registry, _heap);

					if (!table.valid()) {
						Genode::error("ignoring table '", table.name(),
						              "' - checksum error");
						continue;
					}
					if (table.is_searched()) {
						if (verbose)
							Genode::log("Found dsdt ", table.name());

						Element::parse(_heap, table.table());
					}
				} catch (Acpi::Memory::Unsupported_range &) { }
			}

		}

	public:

		Acpi_table(Genode::Env &env, Genode::Allocator &heap)
		: _env(env), _heap(heap), _memory(_env, _heap)
		{
			addr_t rsdt = 0, xsdt = 0;
			uint8_t acpi_revision = 0;

			/* try platform_info ROM provided by core */
			try {
				Genode::Attached_rom_dataspace info(_env, "platform_info");
				Genode::Xml_node xml(info.local_addr<char>(), info.size());
				Xml_node acpi_node = xml.sub_node("acpi");
				acpi_revision = acpi_node.attribute_value("revision", 0U);
				rsdt = acpi_node.attribute_value("rsdt", 0UL);
				xsdt = acpi_node.attribute_value("xsdt", 0UL);
			} catch (...) { }

			/* try legacy way if not found in platform_info */
			if (!rsdt && !xsdt) {
				uint8_t * ptr_rsdp = _rsdp();

				struct rsdp {
					char     signature[8];
					uint8_t  checksum;
					char     oemid[6];
					uint8_t  revision;
					/* table pointer at 16 byte offset in RSDP structure (5.2.5.3) */
					uint32_t rsdt;
					/* With ACPI 2.0 */
					uint32_t len;
					uint64_t xsdt;
					uint8_t  checksum_extended;
					uint8_t  reserved[3];
				} __attribute__((packed));
				struct rsdp * rsdp = reinterpret_cast<struct rsdp *>(ptr_rsdp);

				if (!rsdp) {
					Genode::error("No valid ACPI RSDP structure found");
					return;
				}

				rsdt = rsdp->rsdt;
				xsdt = rsdp->xsdt;
				acpi_revision = rsdp->revision;
				/* drop rsdp io_mem mapping since rsdt/xsdt may overlap */
				_mmio.destruct();
			}

			if (acpi_revision != 0 && xsdt && sizeof(addr_t) != sizeof(uint32_t)) {
				/* running 64bit and xsdt is valid */
				Table_wrapper table(_memory, xsdt, _table_registry, _heap);
				if (!table.valid()) throw -1;

				uint64_t * entries = reinterpret_cast<uint64_t *>(table.table() + 1);
				_parse_tables(entries, table.entry_count(entries));

				Genode::log("XSDT ", *table.table());
			} else {
				/* running (32bit) or (64bit and xsdt isn't valid) */
				Table_wrapper table(_memory, rsdt, _table_registry, _heap);
				if (!table.valid()) throw -1;

				uint32_t * entries = reinterpret_cast<uint32_t *>(table.table() + 1);
				_parse_tables(entries, table.entry_count(entries));

				Genode::log("RSDT ", *table.table());
			}

			/* free up memory of elements not of any use */
			Element::clean_list(_heap);

			/* free up io memory */
			_memory.free_io_memory();
		}

		~Acpi_table()
		{
			_table_registry.for_each([&] (Table_wrapper::Info &info) {
				destroy(_heap, &info);
			});
		}

		void generate_info(Xml_generator &xml) const
		{
			if (_sci_int_valid)
				xml.node("sci_int", [&] () { xml.attribute("irq", _sci_int); });

			if (!_reset_info.constructed())
				return;

			xml.node("reset", [&] () {
				xml.attribute("io_port", String<32>(Hex(_reset_info->io_port)));
				xml.attribute("value",   _reset_info->value);
			});

			Registry<Table_wrapper::Info> const &reg = _table_registry;
			reg.for_each([&] (Table_wrapper::Info const &info) {
				xml.node("table", [&] {
					xml.attribute("name", info.name);
					xml.attribute("addr", String<20>(Hex(info.addr)));
					xml.attribute("size", info.size);
				});
			});
		}
};


static void attribute_hex(Xml_generator &xml, char const *name,
                          unsigned long long value)
{
	xml.attribute(name, String<32>(Hex(value)));
}


void Acpi::generate_report(Genode::Env &env, Genode::Allocator &alloc,
                           Xml_node const &config_xml)
{
	/* parse table */
	Acpi_table acpi_table(env, alloc);

	static Expanding_reporter acpi(env, "acpi", "acpi");

	acpi.generate([&] (Genode::Xml_generator &xml) {

		acpi_table.generate_info(xml);

		if (root_bridge_bdf != INVALID_ROOT_BRIDGE) {
			xml.node("root_bridge", [&] () {
				attribute_hex(xml, "bdf", root_bridge_bdf);
			});
		}

		for (Pci_config_space *e = Pci_config_space::list()->first(); e;
		     e = e->next())
		{
			xml.node("bdf", [&] () {
				xml.attribute("start", e->_bdf_start);
				xml.attribute("count", e->_func_count);
				attribute_hex(xml, "base", e->_base);
			});
		}

		for (Irq_override *i = Irq_override::list()->first(); i; i = i->next())
		{
			xml.node("irq_override", [&] () {
				xml.attribute("irq", i->irq());
				xml.attribute("gsi", i->gsi());
				attribute_hex(xml, "flags", i->flags());
			});
		}

		/* lambda definition for scope evaluation in rmrr */
		auto func_scope = [&] (Device_scope const &scope)
		{
			xml.node("scope", [&] () {
				xml.attribute("bus_start", scope.read<Device_scope::Bus>());
				xml.attribute("type", scope.read<Device_scope::Type>());

				scope.for_each_path([&](auto const &path) {
					xml.node("path", [&] () {
						attribute_hex(xml, "dev" , path.dev());
						attribute_hex(xml, "func", path.func());
					});
				});
			});
		};

		bool ignore_drhd = config_xml.attribute_value("ignore_drhd", false);
		for (Dmar_entry *entry = Dmar_entry::list()->first();
		     entry; entry = entry->next()) {

			entry->apply([&] (Dmar_common const &dmar) {
				if (!ignore_drhd &&
				    dmar.read<Dmar_common::Type>() == Dmar_common::Type::DRHD)
				{
					Dmar_drhd drhd(dmar.range());

					size_t size_log2 = drhd.read<Dmar_drhd::Size::Num_pages>() + 12;

					xml.node("drhd", [&] () {
						attribute_hex(xml, "phys", drhd.read<Dmar_drhd::Phys>());
						attribute_hex(xml, "flags", drhd.read<Dmar_drhd::Flags>());
						attribute_hex(xml, "segment", drhd.read<Dmar_drhd::Segment>());
						attribute_hex(xml, "size", 1 << size_log2);
						drhd.apply(func_scope);
					});
				}

				if (dmar.read<Dmar_common::Type>() != Dmar_common::Type::RMRR)
					return;

				Dmar_rmrr rmrr(dmar.range());

				xml.node("rmrr", [&] () {
					attribute_hex(xml, "start", rmrr.read<Dmar_rmrr::Base>());
					attribute_hex(xml, "end", rmrr.read<Dmar_rmrr::Limit>());

					rmrr.apply(func_scope);
				});
			});
		}

		Ivdb_entry::for_each([&](auto entry) {
			xml.node("ivdb", [&] () {
				xml.attribute("type", entry.type);
			});
		});

		{
			Element *e = Element::list()->first();

			for (; e; e = e->next()) {
				if (!e->is_device())
					continue;

				Pci_routing *r = e->pci_list().first();
				for (; r; r = r->next()) {
					xml.node("routing", [&] () {
						attribute_hex(xml, "gsi", r->gsi());
						attribute_hex(xml, "bridge_bdf", e->bdf());
						attribute_hex(xml, "device", r->device());
						attribute_hex(xml, "device_pin", r->pin());
					});
				}
			}
		}

		/*
		 * Intel opregion lookup & parsing must be finished before acpi
		 * report is sent, therefore the invocation is placed exactly here.
		 */
		Pci_config_space::intel_opregion(env);
	});
}
