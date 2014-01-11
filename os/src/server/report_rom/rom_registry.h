/*
 * \brief  Registry of ROM modules
 * \author Norman Feske
 * \date   2014-01-11
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _ROM_REGISTRY_H_
#define _ROM_REGISTRY_H_

/* local includes */
#include <rom_module.h>

namespace Rom {
	struct Registry_for_reader;
	struct Registry_for_writer;
}


struct Rom::Registry_for_reader
{
	virtual Module const &lookup(Reader const &reader, Module::Name const &name) = 0;

	virtual void release(Reader const &reader, Module const &module) = 0;
};


struct Rom::Registry_for_writer
{
	virtual Module &lookup(Writer const &writer, Module::Name const &name) = 0;

	virtual void release(Writer const &writer, Module const &module) = 0;
};


struct Rom::Registry : Registry_for_reader, Registry_for_writer, Genode::Noncopyable
{
	private:

		Genode::Allocator &_md_alloc;

		Module_list _modules;

		Module &_lookup(Module::Name const name)
		{
			for (Module *m = _modules.first(); m; m = m->next())
				if (m->_has_name(name))
					return *m;

			/* module does not exist yet, create one */

			/* XXX proper accounting for the used memory is missing */
			/* XXX if we run out of memory, the server will abort */
			Module * const module = new (&_md_alloc) Module(name);
			_modules.insert(module);
			return *module;

		}

		void _try_to_destroy(Module const &module)
		{
			if (module._is_in_use())
				return;

			_modules.remove(&module);
			Genode::destroy(&_md_alloc, const_cast<Module *>(&module));
		}

		template <typename USER>
		Module &_lookup(USER const &user, Module::Name const &name)
		{
			Module &module = _lookup(name);
			module._register(user);
			return module;
		}

		template <typename USER>
		void _release(USER const &user, Module const &module)
		{
			module._unregister(user);
			_try_to_destroy(module);
		}

	public:

		Registry(Genode::Allocator &md_alloc) : _md_alloc(md_alloc) { }

		Module &lookup(Writer const &writer, Module::Name const &name) override
		{
			return _lookup(writer, name);
		}

		void release(Writer const &writer, Module const &module) override
		{
			return _release(writer, module);
		}

		Module const &lookup(Reader const &reader, Module::Name const &name) override
		{
			return _lookup(reader, name);
		}

		void release(Reader const &reader, Module const &module) override
		{
			return _release(reader, module);
		}
};

#endif /* _ROM_REGISTRY_H_ */
