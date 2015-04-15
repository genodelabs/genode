/*
 * \brief  ACPI parsing and PCI rewriting code
 * \author Sebastian Sumpf <sebastian.sumpf@genode-labs.com>
 * \date   2012-02-25
 *
 * This code parses the DSDT and SSDT-ACPI tables and extracts the PCI-bridge
 * to GSI interrupt mappings as described by "ATARE: ACPI Tables and Regular
 * Expressions, Bernhard Kauer, TU Dresden technical report TUD-FI09-09,
 * Dresden, Germany, August 2009".
 */

 /*
  * Copyright (C) 2009-2013 Genode Labs GmbH
  *
  * This file is part of the Genode OS framework, which is distributed
  * under the terms of the GNU General Public License version 2.
  */

#include <io_mem_session/connection.h>
#include <pci_session/connection.h>
#include <pci_device/client.h>
#include <os/attached_rom_dataspace.h>

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
struct Dmar_struct;

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

	/* DMAR Intel VT-d structures */
	Dmar_struct_header *dmar_header() { return reinterpret_cast<Dmar_struct_header *>(this); }
	Dmar_struct *dmar_struct() { return reinterpret_cast<Dmar_struct *>(&creator_rev + 4); }
	Dmar_struct *dmar_end()    { return reinterpret_cast<Dmar_struct *>(signature + size); }
} __attribute__((packed));


/**
 * DMA Remapping structures
 */
struct Dmar_struct_header : Generic
{
	enum { INTR_REMAP_MASK = 0x1U };

	uint8_t width;
	uint8_t flags;
	uint8_t reserved[10];
} __attribute__((packed));

/* Reserved Memory Region Reporting structure - Intel VT-d IO Spec - 8.4. */
struct Rmrr_struct
{
	uint16_t type;
	uint16_t length;
	uint16_t reserved;
	uint16_t pci_segment;
	uint64_t start;
	uint64_t end;
} __attribute__((packed));

/* DMA Remapping Reporting structure - Intel VT-d IO Spec - 8.1. */
struct Dmar_struct
{
	enum { DRHD= 0U, RMRR = 0x1U, ATSR = 0x2U, RHSA = 0x3U };
	uint16_t type;
	uint16_t length;
	uint8_t  flags;
	uint8_t  reserved;
	uint16_t pci_segment;
	uint64_t base;
	uint64_t limit;

	Dmar_struct *next() {
		return reinterpret_cast<Dmar_struct *>((uint8_t *)this + length); }

	Rmrr_struct *rmrr() { return reinterpret_cast<Rmrr_struct *>(&base + 1); }
} __attribute__((packed));


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

		bool     match(uint32_t irq) const { return irq == _irq; }
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


static Acpi::Memory & acpi_memory()
{
	static Acpi::Memory _memory;
	return _memory;
}


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
		void parse_madt()
		{
			Apic_struct *apic = _table->apic_struct();
			for (; apic < _table->end(); apic = apic->next()) {
				if (!apic->is_override())
					continue;

				Apic_override *o = static_cast<Apic_override *>(apic);
				
				PINF("MADT IRQ %u -> GSI %u flags: %x", o->irq, o->gsi, o->flags);
				
				Irq_override::list()->insert(new (env()->heap())
					Irq_override(o->irq, o->gsi, o->flags));
			}
		}

		void parse_mcfg()  const
		{
			Mcfg_struct *mcfg = _table->mcfg_struct();
			for (; mcfg < _table->mcfg_end(); mcfg = mcfg->next()) {
				PINF("MCFG BASE 0x%llx seg %02x bus %02x-%02x", mcfg->base,
				     mcfg->pci_seg, mcfg->pci_bus_start, mcfg->pci_bus_end);

				/* bus_count * up to 32 devices * 8 function per device * 4k */
				uint32_t bus_count  = mcfg->pci_bus_end - mcfg->pci_bus_start + 1;
				uint32_t func_count = bus_count * 32 * 8;
				uint32_t bus_start  = mcfg->pci_bus_start * 32 * 8;

				Pci_config_space::list()->insert(
					new (env()->heap()) Pci_config_space(bus_start, func_count, mcfg->base));
			}
		}

		void parse_dmar() const
		{
			Dmar_struct_header *head = _table->dmar_header();
			PLOG("%u bit DMA physical addressable %s\n", head->width + 1,
			     head->flags & Dmar_struct_header::INTR_REMAP_MASK ?
			     ", IRQ remapping supported" : "");

			Dmar_struct *dmar = _table->dmar_struct();
			for (; dmar < _table->dmar_end(); dmar = dmar->next())
				if (dmar->type == Dmar_struct::RMRR)
					PLOG("RMRR: [0x%llx,0x%llx] - DMA region reported by BIOS",
					     dmar->base, dmar->limit);
		}

		Table_wrapper(addr_t base) : _base(base), _table(0)
		{
			/* if table is on page boundary, map two pages, otherwise one page */
			size_t const map_size = 0x1000UL - _offset() < 8 ? 0x1000UL : 1UL;

			/* make table header accessible */
			_table = reinterpret_cast<Generic *>(acpi_memory().phys_to_virt(base, map_size));

			/* table size is known now - make it complete accessible */
			if (_offset() + _table->size > 0x1000UL)
				acpi_memory().phys_to_virt(base, _table->size);

			memset(_name, 0, 5);
			memcpy(_name, _table->signature, 4);

			if (verbose)
				PDBG("Table mapped '%s' at %p (from %lx) size %x", _name, _table, _base, _table->size);

			if (checksum((uint8_t *)_table, _table->size)) {
				PERR("Checksum mismatch for %s", _name);
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
		uint32_t pin() const { return _pin; }
		uint32_t gsi() const { return _gsi; }

		/* debug */
		void dump() { if (verbose) PDBG("Pci: adr %x pin %x gsi: %u", _adr, _pin, _gsi); }
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
		char              *_name;     /* name of element */
		uint32_t           _name_len; /* length of name in bytes */
		uint32_t           _bdf;      /* bus device function */
		uint8_t const     *_data;     /* pointer to the data section */
		uint32_t           _para_len; /* parameters to be skipped */
		bool               _valid;    /* true if this is a valid element */
		bool               _routed;   /* has the PCI information been read */
		List<Pci_routing> *_pci;      /* list of PCI routing elements for this element */

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
				_name = (char *)env()->heap()->alloc(_name_len);
				memcpy(_name, name + prefix_len, _name_len);
			}
			else {
				/* skip parts */
				int parent_len = parent->_name_len;
				parent_len = (parent_len < 0) ? 0 : parent_len;

				/* skip parent prefix */
				for (uint32_t p = 0; name[p] == PARENT_PREFIX; p++, parent_len -= NAME_LEN) ;
				_name = (char *)env()->heap()->alloc(_name_len + parent_len);

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
		void _direct_prt(Element *dev)
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
					Pci_routing * r = new (env()->heap()) Pci_routing(val[0], val[1], val[3]);

					/* set _ADR, _PIN, _GSI */
					dev->pci_list()->insert(r);
					dev->pci_list()->first()->dump();
				}

				len = len ? (e.data() - (_data + offset)) + e.size() : 1;
			}
		}

		/**
		 * Search for _PRT outside of device
		 */
		void _indirect_prt(Element *dev)
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
						PDBG("Indirect %s", name);

					for (uint32_t skip = 0; skip <= dev->_name_len / NAME_LEN; skip++) {
						Element *e = dev->_compare(name, skip * NAME_LEN);
						if (e)
							e->_direct_prt(dev);
					}
				}
				else
					name_len = 1;
			}
		}

		Element(uint8_t const *data = 0, bool package_op4 = false)
		:
			_type(0), _size(0), _size_len(0), _name(0), _name_len(0), _bdf(0),
			_data(data), _para_len(0), _valid(false), _routed(false), _pci(0)
		{
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
			_name_len = other._name_len;
			_bdf      = other._bdf;
			_data     = other._data;
			_valid    = other._valid;
			_routed   = other._routed;
			_pci      = other._pci;
			_para_len = other._para_len;

			if (other._name) {
				_name = (char *)env()->heap()->alloc(other._name_len);
				memcpy(_name, other._name, _name_len);
			}
		}

		bool is_device() { return _type == SUB_DEVICE; }
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
			PDBG("Found package %x size %u name_len %u name: %s",
			     _data[0], _size, _name_len, n);
		}

	public:

		virtual ~Element()
		{
			if (_name)
				env()->heap()->free(_name, _name_len);
		}

		/**
		 * Accessors
		 */
		uint32_t       size() const        { return _size; }
		void           size(uint32_t size) { _size = size; }
		uint32_t       size_len() const    { return _size_len; }
		uint8_t const *data() const        { return _data; }
		bool           valid() const       { return _valid; }

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

		static void clean_list()
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
				destroy(env()->heap(), element);
				element = next;
			}

			if (verbose)
				PDBG("Freeing up memory of elements - %lu bytes", freed_up);
		}

		/**
		 * Return list of PCI information for this element
		 */
		List<Pci_routing> *pci_list()
		{
			if (!_pci)
				_pci = new (env()->heap()) List<Pci_routing>();
			return _pci;
		}

		/**
		 * Parse elements of table
		 */
		static void parse(Generic *table)
		{
			uint8_t const *data = table->data();
			for (; data < table->data() + table->size; data++) {
				Element e(data);

				if (!e.valid() || !e._name_len)
					continue;

				if (data + e.size() > table->data() + table->size)
					break;

				Element *i = new (env()->heap()) Element(e);
				list()->insert(i);

				/* skip header */
				data += e.size_len();
				/* skip name */
				data += NAME_LEN;

				/* skip rest of structure if known */
				if (e.is_device_name())
					data += e._para_len;
			}

			parse_bdf();
		}

		/**
		 * Parse BDF and GSI information
		 */
		static void parse_bdf()
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
						PDBG("Scanning device %x", e->_bdf);

					prt->_direct_prt(e);
					prt->_indirect_prt(e);
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

				Pci_routing *r = e->pci_list()->first();
				for (; r; r = r->next()) {
					if (r->match_bdf(device_bdf) && r->pin() == pin) {
						if (verbose) PDBG("Found GSI: %u device : %x pin %u",
						                   r->gsi(), device_bdf, pin);
						return r->gsi();
					}
				}
			}
			throw -1;
		}

		static void create_config_file(char * text, size_t max)
		{
			Pci_config_space *e = Pci_config_space::list()->first();

			int len = snprintf(text, max, "<config>");
			text += len;
			max  -= len;
			for (; e; e = e->next())
			{
				using namespace Genode;
				len   = snprintf(text, max, "<bdf><start>%u</start>", e->_bdf_start);
				text += len;
				max  -= len;
				len   = snprintf(text, max, "<count>%u</count>"       , e->_func_count);
				text += len;
				max  -= len;
				len   = snprintf(text, max, "<base>0x%lx</base></bdf>" , e->_base);
				text += len;
				max  -= len;
			}

			Attached_rom_dataspace rom("config");
			char * rom_text = rom.local_addr<char>();
			size_t rom_len  = strlen(rom_text);

			if (max > rom_len - 9) {
				rom_text += 9;
				rom_len  -= 9;
				memcpy(text, rom_text, rom_len);
				text += rom_len;
				max  -= rom_len;
			} else
				PERR("could not add pci_drv policy");

			if (max < 2)
				PERR("config file could not be generated, buffer to small");
		}
};


/**
 * Locate and parse PCI tables we are looking for
 */
class Acpi_table
{
	private:

		/* BIOS range to scan for RSDP */
		enum { BIOS_BASE = 0xe0000, BIOS_SIZE = 0x20000 };

		/**
		 * Map table and return address and session cap
		 */
		uint8_t *_map_io(addr_t base, size_t size, Io_mem_session_capability &cap)
		{
			Io_mem_connection io_mem(base, size);
			io_mem.on_destruction(Io_mem_connection::KEEP_OPEN);
			Io_mem_dataspace_capability io_ds = io_mem.dataspace();
			if (!io_ds.valid())
				throw -1;

			uint8_t *ret = env()->rm_session()->attach(io_ds, size);
			cap = io_mem.cap();
			return ret;
		}

		/**
		 * Search for RSDP pointer signature in area
		 */
		uint8_t *_search_rsdp(uint8_t *area)
		{
			for (addr_t addr = 0; area && addr < BIOS_SIZE; addr += 16)
				if (!memcmp(area + addr, "RSD PTR ", 8) && !Table_wrapper::checksum(area + addr, 20))
					return area + addr;

			throw -2;
		}

		/**
		 * Return 'Root System Descriptor Pointer' (ACPI spec 5.2.5.1)
		 */
		uint8_t *_rsdp(Io_mem_session_capability &cap)
		{
			uint8_t *area = 0;

			/* try BIOS area (0xe0000 - 0xfffffh)*/
			try {
				area = _search_rsdp(_map_io(BIOS_BASE, BIOS_SIZE, cap));
				return area;
			} catch (...) { env()->parent()->close(cap); }

			/* search EBDA (BIOS addr + 0x40e) */
			try {
				area = _map_io(0x0, 0x1000, cap);
				if (area) {
					unsigned short base = (*reinterpret_cast<unsigned short *>(area + 0x40e)) << 4;
					env()->parent()->close(cap);
					area = _map_io(base, 1024, cap);
					area = _search_rsdp(area);
				}
				return area;
			} catch (...) { env()->parent()->close(cap); }

			return 0;
		}

		template <typename T>
		void _parse_tables(T * entries, uint32_t count)
		{
			/* search for SSDT and DSDT tables */
			for (uint32_t i = 0; i < count; i++) {
				uint32_t dsdt = 0;
				{
					Table_wrapper table(entries[i]);
					if (table.is_facp())
						dsdt = *reinterpret_cast<uint32_t *>(table->signature + 40);

					if (table.is_searched()) {

						if (verbose)
							PDBG("Found %s", table.name());

						Element::parse(table.table());
					}

					if (table.is_madt()) {
						PDBG("Found MADT");

						table.parse_madt();
					}
					if (table.is_mcfg()) {
						PDBG("Found MCFG");

						table.parse_mcfg();
					}
					if (table.is_dmar()) {
						PDBG("Found DMAR");

						table.parse_dmar();
					}
				}

				if (dsdt) {
					Table_wrapper table(dsdt);
					if (table.is_searched()) {
						if (verbose)
							PDBG("Found dsdt %s", table.name());

						Element::parse(table.table());
					}
				}
			}

		}

	public:

		Acpi_table()
		{
			Io_mem_session_capability io_mem;

			uint8_t * ptr_rsdp = _rsdp(io_mem);

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
					PDBG("No rsdp structure found");
				return;
			}

			if (verbose) {
				uint8_t oem[7];
				memcpy(oem, rsdp->oemid, 6);
				oem[6] = 0;
				PDBG("ACPI revision %u of OEM '%s', rsdt:0x%x xsdt:0x%llx",
				     rsdp->revision, oem, rsdp->rsdt, rsdp->xsdt);
			}

			addr_t const rsdt = rsdp->rsdt;
			addr_t const xsdt = rsdp->xsdt;
			/* drop rsdp io_mem mapping since rsdt/xsdt may overlap */
			env()->parent()->close(io_mem);

			if (xsdt && sizeof(addr_t) != sizeof(uint32_t)) {
				/* running 64bit and xsdt is valid */
				Table_wrapper table(xsdt);
				uint64_t * entries = reinterpret_cast<uint64_t *>(table.table() + 1);
				_parse_tables(entries, table.entry_count(entries));
			} else {
				/* running (32bit) or (64bit and xsdt isn't valid) */
				Table_wrapper table(rsdt);
				uint32_t * entries = reinterpret_cast<uint32_t *>(table.table() + 1);
				_parse_tables(entries, table.entry_count(entries));
			}

			/* free up memory of elements not of any use */
			Element::clean_list();

			/* free up io memory */
			acpi_memory().free_io_memory();
		}
};


/**
 * Pci::Device_client extensions identifies bridges and adds IRQ line re-write
 */
class Pci_client : public ::Pci::Device_client
{
	public:

		Pci_client(Pci::Device_capability &cap) : ::Pci::Device_client(cap) { }

		/**
		 * Return true if this is a PCI-PCI bridge
		 */
		bool is_bridge()
		{
			enum { BRIDGE_CLASS = 0x6 };
			if ((class_code() >>  16) != BRIDGE_CLASS)
				return false;

			/* see PCI bridge spec (3.2) */
			enum { BRIDGE = 0x1 };
			uint16_t header = config_read(0xe, ::Pci::Device::ACCESS_16BIT);

			/* skip multi function flag 0x80) */
			return ((header & 0x3f) != BRIDGE) ? false : true;
		}

		/**
		 * Return bus-device function of this device
		 */
		uint32_t bdf()
		{
			uint8_t bus, dev, func;
			bus_address(&bus, &dev, &func);
			return (bus << 8) | ((dev & 0x1f) << 3) | (func & 0x7);
		}

		/**
		 * Return IRQ pin
		 */
		uint32_t irq_pin()
		{
			return ((config_read(0x3c, ::Pci::Device::ACCESS_32BIT) >> 8) & 0xf);
		}

		/**
		 * Return IRQ line
		 */
		uint8_t irq_line()
		{
			return (config_read(0x3c, ::Pci::Device::ACCESS_8BIT));
		}

		/**
		 * Write line to config space
		 */
		void irq_line(uint32_t gsi)
		{
			config_write(0x3c, gsi, ::Pci::Device::ACCESS_8BIT);
		}
};


/**
 * List of PCI-bridge devices
 */
class Pci_bridge : public List<Pci_bridge>::Element
{
	private:

		/* PCI config space fields of bridge */
		uint32_t _bdf;
		uint32_t _secondary_bus;
		uint32_t _subordinate_bus;

		Pci_bridge(Pci_client &client) : _bdf(client.bdf())
		{
			/* PCI bridge spec 3.2.5.3, 3.2.5.4 */
			uint32_t bus = client.config_read(0x18, ::Pci::Device::ACCESS_32BIT);
			_secondary_bus = (bus >> 8) & 0xff;
			_subordinate_bus = (bus >> 16) & 0xff;

			if (verbose)
				PDBG("New bridge: bdf %x se: %u su: %u", _bdf, _secondary_bus, _subordinate_bus);
		}

		static List<Pci_bridge> *_list()
		{
			static List<Pci_bridge> list;
			return &list;
		}

	public:

		/**
		 * Scan PCI bus for bridges
		 */
		Pci_bridge(Pci::Session_capability &session)
		{
			Pci::Session_client pci(session);
			Pci::Device_capability device_cap = pci.first_device(), prev_device_cap;

			/* search for bridges */
			while (device_cap.valid()) {
				prev_device_cap = device_cap;
				Pci_client device(device_cap);

				if (device.is_bridge())
					_list()->insert(new (env()->heap()) Pci_bridge(device));

				device_cap = pci.next_device(device_cap);
				pci.release_device(prev_device_cap);
			}
		}

		/**
		 * Locate BDF of bridge belonging to given bdf
		 */
		static uint32_t bridge_bdf(uint32_t bdf)
		{
			Pci_bridge *bridge = _list()->first();
			uint32_t bus = bdf >> 8;

			for (; bridge; bridge = bridge->next())
				if (bridge->_secondary_bus <= bus && bridge->_subordinate_bus >= bus)
					return bridge->_bdf;

			return 0;
		}

};


/**
 * Debugging
 */
static void dump_bdf(uint32_t a, uint32_t b, uint32_t pin)
{
	if (verbose)
		PDBG("Device bdf %02x:%02x.%u (%x) bridge %02x:%02x.%u (%x) Pin: %u",
		     (a >> 8), (a >> 3) & 0x1f, (a & 0x7), a,
		     (b >> 8), (b >> 3) & 0x1f, (b & 0x7), b, pin);
}


static void dump_rewrite(uint32_t bdf, uint8_t line, uint8_t gsi)
{
	PINF("Rewriting %02x:%02x.%u IRQ: %02u -> GSI: %02u",
	     (bdf >> 8), (bdf >> 3) & 0x1f, (bdf & 0x7), line, gsi);
}


/**
 * Parse acpi table
 */
static void init_acpi_table()
{
	static Acpi_table table;
}


/**
 * Create config file for pci_drv
 */
void Acpi::create_pci_config_file(char * config_space,
                                  Genode::size_t config_space_max)
{
	init_acpi_table();
	Element::create_config_file(config_space, config_space_max);
}


/**
 * Rewrite GSIs of PCI config space
 */
void Acpi::configure_pci_devices(Pci::Session_capability &session)
{
	init_acpi_table();
	static Pci_bridge bridge(session);

	/* if no _PIC method could be found don't rewrite */
	bool acpi_rewrite = Element::supported_acpi_format();

	if (acpi_rewrite)
		PINF("ACPI table format is supported - rewrite GSIs");
	else
		PWRN("ACPI table format not supported - will not rewrite GSIs");

	Pci::Session_client pci(session);
	Pci::Device_capability device_cap = pci.first_device(), prev_device_cap;

	while (device_cap.valid()) {
		prev_device_cap = device_cap;
		Pci_client device(device_cap);

		/* rewrite IRQs */
		if (acpi_rewrite && !device.is_bridge()) {
			uint32_t device_bdf = device.bdf();
			uint32_t bridge_bdf = Pci_bridge::bridge_bdf(device_bdf);
			uint32_t irq_pin    = device.irq_pin();
			if (irq_pin) {
				dump_bdf(device_bdf, bridge_bdf, irq_pin - 1);
				try {
					uint8_t gsi = Element::search_gsi(device_bdf, bridge_bdf, irq_pin - 1);
					dump_rewrite(device_bdf, device.irq_line(), gsi);
					device.irq_line(gsi);
				} catch (...) { }
			}
		}

		device_cap = pci.next_device(device_cap);
		pci.release_device(prev_device_cap);
	}

}


/**
 * Search override structures
 */
unsigned Acpi::override(unsigned irq, unsigned *mode)
{
	for (Irq_override *i = Irq_override::list()->first(); i; i = i->next())
		if (i->match(irq)) {
			*mode = i->flags();
			return i->gsi();
		}

	*mode = 0;
	return irq;
}
