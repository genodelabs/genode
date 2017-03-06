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
  * Copyright (C) 2009-2017 Genode Labs GmbH
  *
  * This file is part of the Genode OS framework, which is distributed
  * under the terms of the GNU Affero General Public License version 3.
  */

/* base includes */
#include <base/attached_io_mem_dataspace.h>
#include <util/misc_math.h>
#include <util/mmio.h>

/* os includes */
#include <os/reporter.h>

#include "acpi.h"
#include "memory.h"

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

	uint8_t const *data() { return reinterpret_cast<uint8_t *>(this); }

	/* MADT ACPI structure */
	Apic_struct *apic_struct() { return reinterpret_cast<Apic_struct *>(&creator_rev + 3); }
	Apic_struct *end()         { return reinterpret_cast<Apic_struct *>(signature + size); }

	/* MCFG ACPI structure */
	Mcfg_struct *mcfg_struct() { return reinterpret_cast<Mcfg_struct *>(&creator_rev + 3); }
	Mcfg_struct *mcfg_end()    { return reinterpret_cast<Mcfg_struct *>(signature + size); }

	Dmar_struct_header *dmar_header() { return reinterpret_cast<Dmar_struct_header *>(this); }
} __attribute__((packed));


struct Dmar_common : Genode::Mmio
{
	struct Type : Register<0x0, 16> {
		enum { DRHD= 0U, RMRR = 0x1U, ATSR = 0x2U, RHSA = 0x3U }; };
	struct Length : Register<0x2, 16> { };

	Dmar_common(addr_t mmio) : Genode::Mmio(mmio) { }
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
		do {
			Dmar_common dmar(addr);

			func(dmar);

			addr = dmar.base() + dmar.read<Dmar_common::Length>();
		} while (addr < dmar_entry_end());
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
struct Device_scope : Genode::Mmio
{
	Device_scope(addr_t a) : Genode::Mmio(a) { }

	struct Type   : Register<0x0, 8> { enum { PCI_END_POINT = 0x1 }; };
	struct Length : Register<0x1, 8> { };
	struct Bus    : Register<0x5, 8> { };

	struct Path   : Register_array<0x6, 8, 1, 16> {
		struct Dev  : Bitfield<0,8> { };
		struct Func : Bitfield<8,8> { };
	};

	unsigned count() const { return (read<Length>() - 6) / 2; }
};


/* DMA Remapping Reporting structure - Intel VT-d IO Spec - 8.3. */
struct Dmar_rmrr : Genode::Mmio
{
	struct Length : Register<0x02, 16> { };
	struct Base   : Register<0x08, 64> { };
	struct Limit  : Register<0x10, 64> { };

	Dmar_rmrr(addr_t a) : Genode::Mmio(a) { }

	template <typename FUNC>
	void apply(FUNC const &func = [] () { } )
	{
		addr_t addr = base() + 24;
		do {
			Device_scope scope(addr);

			func(scope);

			addr = scope.base() + scope.read<Device_scope::Length>();
		} while (addr < base() + read<Length>());
	}
};


/* Fixed ACPI description table (FADT) */
struct Fadt : Genode::Mmio
{
	static uint32_t features;
	static uint32_t reset_type;
	static uint64_t reset_addr;
	static uint8_t  reset_value;

	Fadt(addr_t a) : Genode::Mmio(a)
	{
		features    = read<Fadt::Feature_flags>();
		reset_type  = read<Fadt::Reset_reg_type>();
		reset_addr  = read<Fadt::Reset_reg_addr>();
		reset_value = read<Fadt::Reset_value>();
	}

	struct Dsdt           : Register<0x28, 32> { };
	struct Feature_flags  : Register<0x70, 32> { };
	struct Reset_reg_type : Register<0x74, 32> { };
	struct Reset_reg_addr : Register<0x78, 64> { };
	struct Reset_value    : Register<0x80, 8>  { };
};

uint32_t Fadt::features    = 0;
uint32_t Fadt::reset_type  = 0;
uint64_t Fadt::reset_addr  = 0;
uint8_t  Fadt::reset_value = 0;

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
};



/**
 * ACPI table wrapper that for mapping tables to this address space
 */
class Table_wrapper
{
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

		Table_wrapper(Acpi::Memory &memory, addr_t base)
		: _base(base), _table(0)
		{
			/* if table is on page boundary, map two pages, otherwise one page */
			size_t const map_size = 0x1000UL - _offset() < 8 ? 0x1000UL : 1UL;

			/* make table header accessible */
			_table = reinterpret_cast<Generic *>(memory.phys_to_virt(base, map_size));

			/* table size is known now - make it complete accessible */
			if (_offset() + _table->size > 0x1000UL)
				memory.phys_to_virt(base, _table->size);

			memset(_name, 0, 5);
			memcpy(_name, _table->signature, 4);

			if (verbose)
				Genode::log("table mapped '", Genode::Cstring(_name), "' at ", _table, " "
				            "(from ", Genode::Hex(_base), ") "
				            "size ",  Genode::Hex(_table->size));

			if (checksum((uint8_t *)_table, _table->size)) {
			Genode::error("checksum mismatch for ", Genode::Cstring(_name));
				throw -1;
			}
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


/**
 * A table element (method, device, scope or name)
 */
class Element : public List<Element>::Element
{
	private:

		uint8_t            _type;     /* the type of this element */
		uint32_t           _size;     /* size in bytes */
		uint32_t           _size_len; /* length of size in bytes */
		char               _name[64]; /* name of element */
		uint32_t           _name_len; /* length of name in bytes */
		uint32_t           _bdf;      /* bus device function */
		uint8_t const     *_data;     /* pointer to the data section */
		uint32_t           _para_len; /* parameters to be skipped */
		bool               _valid;    /* true if this is a valid element */
		bool               _routed;   /* has the PCI information been read */
		List<Pci_routing>  _pci;      /* list of PCI routing elements for this element */

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

			case DEVICE_NAME:
				/* ACPI 19.2.5.1 - NameOp NameString DataRefObject */

				if (!(_name_len = _read_name_len()))
					return;

				_valid = true;

				/* ACPI 19.2.3 DataRefObject */
				switch (data[_name_len + 1]) {
					case QWORD_PREFIX: _para_len += 4;
					case DWORD_PREFIX: _para_len += 2;
					case  WORD_PREFIX: _para_len += 1;
					case  BYTE_PREFIX: _para_len += 1;
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

				freed_up += sizeof(*element) + element->_name ? element->_name_len : 0;

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
					if (verbose)
						Genode::log("Scanning device ", Genode::Hex(e->_bdf));

					prt->_direct_prt(alloc, e);
					prt->_indirect_prt(alloc, e);
				}

				e->_routed = true;
			}
		}

		/**
		 * Search for GSI of given device, bridge, and pin
		 */
		static uint32_t search_gsi(uint32_t device_bdf, uint32_t bridge_bdf, uint32_t pin)
		{
			Element *e = list()->first();

			for (; e; e = e->next()) {
				if (!e->is_device() || e->_bdf != bridge_bdf)
					continue;

				Pci_routing *r = e->pci_list().first();
				for (; r; r = r->next()) {
					if (r->match_bdf(device_bdf) && r->pin() == pin) {
						if (verbose)
							Genode::log("Found GSI: ", r->gsi(), " "
							            "device : ", Genode::Hex(device_bdf), " ",
							            "pin ", pin);
						return r->gsi();
					}
				}
			}
			throw -1;
		}
};


/**
 * Locate and parse PCI tables we are looking for
 */
class Acpi_table
{
	private:

		Genode::Env       &_env;
		Genode::Allocator &_alloc;
		Acpi::Memory       _memory;

		/* BIOS range to scan for RSDP */
		enum { BIOS_BASE = 0xe0000, BIOS_SIZE = 0x20000 };

		Genode::Constructible<Genode::Attached_io_mem_dataspace> _mmio;

		/**
		 * Search for RSDP pointer signature in area
		 */
		uint8_t *_search_rsdp(uint8_t *area)
		{
			for (addr_t addr = 0; area && addr < BIOS_SIZE; addr += 16)
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
				return _search_rsdp(_mmio->local_addr<uint8_t>());
			}
			catch (...) { }

			/* search EBDA (BIOS addr + 0x40e) */
			try {
				/* read RSDP base address from EBDA */

				_mmio.construct(_env, 0x0, 0x1000);
				uint8_t const * const ebda = _mmio->local_addr<uint8_t const>();
				unsigned short const rsdp_phys =
					(*reinterpret_cast<unsigned short const *>(ebda + 0x40e)) << 4;

				_mmio.construct(_env, rsdp_phys, 0x1000);
				return _search_rsdp(_mmio->local_addr<uint8_t>());
			}
			catch (...) { }

			return nullptr;
		}

		template <typename T>
		void _parse_tables(Genode::Allocator &alloc, T * entries, uint32_t count)
		{
			/* search for SSDT and DSDT tables */
			for (uint32_t i = 0; i < count; i++) {
				uint32_t dsdt = 0;
				{
					Table_wrapper table(_memory, entries[i]);
					if (table.is_facp()) {
						Fadt fadt(reinterpret_cast<Genode::addr_t>(table->signature));
						dsdt = fadt.read<Fadt::Dsdt>();
					}

					if (table.is_searched()) {

						if (verbose)
							Genode::log("Found ", table.name());

						Element::parse(alloc, table.table());
					}

					if (table.is_madt()) {
						Genode::log("Found MADT");

						table.parse_madt(alloc);
					}
					if (table.is_mcfg()) {
						Genode::log("Found MCFG");

						table.parse_mcfg(alloc);
					}
					if (table.is_dmar()) {
						Genode::log("Found DMAR");

						table.parse_dmar(alloc);
					}
				}

				if (dsdt) {
					Table_wrapper table(_memory, dsdt);
					if (table.is_searched()) {
						if (verbose)
							Genode::log("Found dsdt ", table.name());

						Element::parse(alloc, table.table());
					}
				}
			}

		}

	public:

		Acpi_table(Genode::Env &env, Genode::Allocator &alloc)
		: _env(env), _alloc(alloc), _memory(_env, _alloc)
		{
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
				if (verbose)
					Genode::log("No rsdp structure found");
				return;
			}

			if (verbose) {
				uint8_t oem[7];
				memcpy(oem, rsdp->oemid, 6);
				oem[6] = 0;
				Genode::log("ACPI revision ", rsdp->revision, " of "
				            "OEM '", oem, "', "
				            "rsdt:", Genode::Hex(rsdp->rsdt), " "
				            "xsdt:", Genode::Hex(rsdp->xsdt));
			}

			addr_t const rsdt = rsdp->rsdt;
			addr_t const xsdt = rsdp->xsdt;
			uint8_t const acpi_revision = rsdp->revision;
			/* drop rsdp io_mem mapping since rsdt/xsdt may overlap */
			_mmio.destruct();

			if (acpi_revision != 0 && xsdt && sizeof(addr_t) != sizeof(uint32_t)) {
				/* running 64bit and xsdt is valid */
				Table_wrapper table(_memory, xsdt);
				uint64_t * entries = reinterpret_cast<uint64_t *>(table.table() + 1);
				_parse_tables(alloc, entries, table.entry_count(entries));
			} else {
				/* running (32bit) or (64bit and xsdt isn't valid) */
				Table_wrapper table(_memory, rsdt);
				uint32_t * entries = reinterpret_cast<uint32_t *>(table.table() + 1);
				_parse_tables(alloc, entries, table.entry_count(entries));
			}

			/* free up memory of elements not of any use */
			Element::clean_list(alloc);

			/* free up io memory */
			_memory.free_io_memory();
		}
};


static void attribute_hex(Xml_generator &xml, char const *name,
                          unsigned long long value)
{
	char buf[32];
	Genode::snprintf(buf, sizeof(buf), "0x%llx", value);
	xml.attribute(name, buf);
}


void Acpi::generate_report(Genode::Env &env, Genode::Allocator &alloc)
{
	/* parse table */
	Acpi_table acpi_table(env, alloc);

	enum { REPORT_SIZE = 4 * 4096 };
	static Reporter acpi(env, "acpi", "acpi", REPORT_SIZE);
	acpi.enabled(true);

	Genode::Reporter::Xml_generator xml(acpi, [&] () {
		if (!(!Fadt::features && !Fadt::reset_type &&
		      !Fadt::reset_addr && !Fadt::reset_value))
			xml.node("fadt", [&] () {
				attribute_hex(xml, "features"   , Fadt::features);
				attribute_hex(xml, "reset_type" , Fadt::reset_type);
				attribute_hex(xml, "reset_addr" , Fadt::reset_addr);
				attribute_hex(xml, "reset_value", Fadt::reset_value);
			});

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
				for (unsigned j = 0 ; j < scope.count(); j++) {
					xml.node("path", [&] () {
						attribute_hex(xml, "dev",
						              scope.read<Device_scope::Path::Dev>(j));
						attribute_hex(xml, "func",
						              scope.read<Device_scope::Path::Func>(j));
					});
				}
			});
		};

		for (Dmar_entry *entry = Dmar_entry::list()->first();
		     entry; entry = entry->next()) {

			entry->apply([&] (Dmar_common const &dmar) {
				if (dmar.read<Dmar_common::Type>() != Dmar_common::Type::RMRR)
					return;

				Dmar_rmrr rmrr(dmar.base());

				xml.node("rmrr", [&] () {
					attribute_hex(xml, "start", rmrr.read<Dmar_rmrr::Base>());
					attribute_hex(xml, "end", rmrr.read<Dmar_rmrr::Limit>());

					rmrr.apply(func_scope);
				});
			});
		}
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
	});
}
