/**
 * \brief  ELF-dynamic section (see ELF ABI)
 * \author Sebastian Sumpf
 * \date   2015-03-12
 */

/*
 * Copyright (C) 2015-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__DYNAMIC_H_
#define _INCLUDE__DYNAMIC_H_

/* local includes */
#include <types.h>
#include <relocation.h>

namespace Linker {
	struct Hash_table;
	struct Dynamic;
}

/**
 * ELF hash table and hash function
 */
struct Linker::Hash_table
{
	unsigned long nbuckets() const { return *((Elf::Hashelt *)this); }
	unsigned long nchains()  const { return *(((Elf::Hashelt *)this) + 1); }

	Elf::Hashelt const *buckets() { return ((Elf::Hashelt *)this + 2); }
	Elf::Hashelt const *chains()  { return buckets() + nbuckets(); }

	/**
	 * ELF hash function Figure 5.12 of the 'System V ABI'
	 */
	static unsigned long hash(char const *name)
	{
		unsigned const char  *p = (unsigned char const *)name;
		unsigned long         h = 0, g;

		while (*p) {
			h = (h << 4) + *p++;
			if ((g = h & 0xf0000000) != 0)
				h ^= g >> 24;
			h &= ~g;
		}
		return h;
	}
};


/**
 * .dynamic section entries
 */
class Linker::Dynamic
{
	public:

		class Dynamic_section_missing { };

	private:

		/*
		 * Noncopyable
		 */
		Dynamic(Dynamic const &);
		Dynamic &operator = (Dynamic const &);

		struct Needed : Fifo<Needed>::Element
		{
			off_t offset;

			Needed(off_t offset) : offset(offset) { }

			char const *path(char const *strtab)
			{
				return ((char const *)(strtab + offset));
			}

			char const *name(char const *strtab)
			{
				return file(path(strtab));
			}
		};

		Dependency const    *_dep;
		Object     const    &_obj;
		Elf::Dyn   const    &_dynamic;

		Allocator           *_md_alloc      = nullptr;

		Hash_table          *_hash_table    = nullptr;

		Elf::Rela           *_reloca        = nullptr;
		unsigned long        _reloca_size   = 0;

		Elf::Sym            *_symtab        = nullptr;
		char                *_strtab        = nullptr;
		unsigned long        _strtab_size   = 0;

		Elf::Addr           *_pltgot        = nullptr;

		Elf::Rel            *_pltrel        = nullptr;
		unsigned long        _pltrel_size   = 0;
		D_tag                _pltrel_type   = DT_NULL;

		Func                 _init_function = nullptr;

		Elf::Rel            *_rel           = nullptr;
		unsigned long        _rel_size      = 0;

		Fifo<Needed>         _needed { };

		/**
		 * \throw Dynamic_section_missing
		 */
		Elf::Dyn const &_find_dynamic(Linker::Phdr const *p)
		{
			for (unsigned i = 0; i < p->count; i++)
				if (p->phdr[i].p_type == PT_DYNAMIC)
					return *reinterpret_cast<Elf::Dyn const *>
						(p->phdr[i].p_vaddr + _obj.reloc_base());

			throw Dynamic_section_missing();
		}

		void _section_dt_needed(Elf::Dyn const *d)
		{
			if (!_md_alloc) {
				error("unexpected call of section_dt_needed");
				throw Fatal();
			}
			Needed *n = new (*_md_alloc) Needed(d->un.ptr);
			_needed.enqueue(*n);
		}

		template <typename T>
		void _section(T *member, Elf::Dyn const *d)
		{
			*member = (T)(_obj.reloc_base() + d->un.ptr);
		}

		void _section_dt_debug(Elf::Dyn const *d)
		{
			Elf::Dyn *_d = const_cast<Elf::Dyn *>(d);
			_d->un.ptr   = (Elf::Addr)Debug::d();
		}

		void _init() SELF_RELOC
		{
			for (Elf::Dyn const *d = &_dynamic; d->tag != DT_NULL; d++) {
				switch (d->tag) {
				case DT_NEEDED  : _section_dt_needed(d);                                break;
				case DT_PLTRELSZ: _pltrel_size = d->un.val;                             break;
				case DT_PLTGOT  : _section<typeof(_pltgot)>(&_pltgot, d);               break;
				case DT_HASH    : _section<typeof(_hash_table)>(&_hash_table, d);       break;
				case DT_RELA    : _section<typeof(_reloca)>(&_reloca, d);               break;
				case DT_RELASZ  : _reloca_size = d->un.val;                             break;
				case DT_SYMTAB  : _section<typeof(_symtab)>(&_symtab, d);               break;
				case DT_STRTAB  : _section<typeof(_strtab)>(&_strtab, d);               break;
				case DT_STRSZ   : _strtab_size = d->un.val;                             break;
				case DT_INIT    : _section<typeof(_init_function)>(&_init_function, d); break;
				case DT_PLTREL  : _pltrel_type = (D_tag)d->un.val;                      break;
				case DT_JMPREL  : _section<typeof(_pltrel)>(&_pltrel, d);               break;
				case DT_REL     : _section<typeof(_rel)>(&_rel, d);                     break;
				case DT_RELSZ   : _rel_size = d->un.val;                                break;
				case DT_DEBUG   : _section_dt_debug(d);                                 break;
				default:
					break;
				}
			}
		}

	public:

		enum Pass { FIRST_PASS, SECOND_PASS };

		Dynamic(Dependency const &dep)
		:
			_dep(&dep), _obj(dep.obj()), _dynamic(*(Elf::Dyn *)dynamic_address())
		{
			_init();
		}

		Dynamic(Allocator &md_alloc, Dependency const &dep, Object const &obj,
		        Linker::Phdr const *phdr)
		:
			_dep(&dep), _obj(obj), _dynamic(_find_dynamic(phdr)), _md_alloc(&md_alloc)
		{
			_init();
		}

		~Dynamic()
		{
			if (!_md_alloc)
				return;

			_needed.dequeue_all([&] (Needed &n) {
				destroy(*_md_alloc, &n); });
		}

		void call_init_function() const
		{
			if (!_init_function)
				return;

			if (verbose_relocation)
				log(_obj.name(), " init func ", _init_function);

			_init_function();
		}

		Elf::Sym const *symbol(unsigned sym_index) const
		{
			if (sym_index > _hash_table->nchains())
				return nullptr;

			return _symtab + sym_index;
		}

		char const *symbol_name(Elf::Sym const &sym) const
		{
			return _strtab + sym.st_name;
		}

		void const *dynamic_ptr() const { return &_dynamic; }

		void dep(Dependency const &dep) { _dep = &dep; }

		Dependency const &dep() const { return *_dep; }

		/*
		 * Use DT_HASH table address for linker, assuming that it will always be at
		 * the beginning of the file
		 */
		Elf::Addr link_map_addr() const { return trunc_page((Elf::Addr)_hash_table); }

		/**
		 * Lookup symbol name in this ELF
		 */
		Elf::Sym const *lookup_symbol(char const *name, unsigned long hash) const
		{
			Hash_table *h = _hash_table;

			if (!h->buckets())
				return nullptr;

			unsigned long sym_index = h->buckets()[hash % h->nbuckets()];

			/* traverse hash chain */
			for (; sym_index != STN_UNDEF; sym_index = h->chains()[sym_index])
			{
				/* bad object */
				if (sym_index > h->nchains())
					return nullptr;

				Elf::Sym const *sym      = symbol(sym_index);
				char const     *sym_name = symbol_name(*sym);

				/* this omitts everything but 'NOTYPE', 'OBJECT', and 'FUNC' */
				if (sym->type() > STT_FUNC)
					continue;

				if (sym->st_value == 0)
					continue;

				/* check for symbol name */
				if (name[0] != sym_name[0] || strcmp(name, sym_name))
					continue;

				return sym;
			}

			return nullptr;
		}

		/**
		 * \throw Address_info::Invalid_address
		 */
		Elf::Sym const &symbol_by_addr(addr_t addr) const
		{
			addr_t const reloc_base = _obj.reloc_base();

			for (unsigned long i = 0; i < _hash_table->nchains(); i++)
			{
				Elf::Sym const *sym = symbol(i);
				if (!sym)
					continue;

				/* skip */
				if (sym->st_shndx == SHN_UNDEF || sym->st_shndx == SHN_COMMON)
					continue;

				addr_t const sym_addr = reloc_base + sym->st_value;
				if (addr < sym_addr || addr >= sym_addr + sym->st_size)
					continue;

				return *sym;
			}
			throw Address_info::Invalid_address();
		}

		/**
		 * Call functor for each dependency, passing the path as argument
		 */
		template <typename FUNC>
		void for_each_dependency(FUNC const &fn) const
		{
			_needed.for_each([&] (Needed &n) {
				fn(n.path(_strtab)); });
		}

		void relocate(Bind bind) SELF_RELOC
		{
			plt_setup();

			if (_pltrel_size) {
				switch (_pltrel_type) {
				case DT_RELA:
				case DT_REL:
					Reloc_plt(_obj, _pltrel_type, _pltrel, _pltrel_size);
					break;
				default:
					error("LD: Invalid PLT relocation ", (int)_pltrel_type);
					throw Incompatible();
				}
			}

			relocate_non_plt(bind, FIRST_PASS);
		}

		void plt_setup()
		{
			if (_pltgot)
				Plt_got r(*_dep, _pltgot);
		}

		void relocate_non_plt(Bind bind, Pass pass)
		{
			if (_reloca)
				Reloc_non_plt r(*_dep, _reloca, _reloca_size, pass == SECOND_PASS);

			if (_rel)
				Reloc_non_plt r(*_dep, _rel, _rel_size, pass == SECOND_PASS);

			if (bind == BIND_NOW)
				Reloc_bind_now r(*_dep, _pltrel, _pltrel_size);
		}

		Elf::Rel const *pltrel()      const { return _pltrel; }
		D_tag           pltrel_type() const { return _pltrel_type; }
};

#endif /* _INCLUDE__DYNAMIC_H_ */
