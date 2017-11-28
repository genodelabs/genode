/*
 * \brief  Registry of ROM modules
 * \author Norman Feske
 * \date   2014-01-11
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _ROM_REGISTRY_H_
#define _ROM_REGISTRY_H_

/* Genode includes */
#include <report_rom/rom_registry.h>
#include <os/session_policy.h>

namespace Rom { struct Registry; }


struct Rom::Registry : Registry_for_reader, Registry_for_writer, Genode::Noncopyable
{
	private:

		Genode::Allocator              &_md_alloc;
		Genode::Ram_session            &_ram;
		Genode::Region_map             &_rm;
		Reader                         &_reader;

		Module_list _modules;

		struct Read_write_policy : Module::Read_policy, Module::Write_policy
		{
			bool read_permitted(Module const &,
			                    Writer const &,
			                    Reader const &) const override
			{
				/*
				 * The pointer application is always allowed to read the ROM content.
				 */
				return true;
			}

			bool write_permitted(Module const &, Writer const &) const override
			{
				/*
				 * Because the report-session label is used as the module name
				 * for the writer, each report session refers to a distinct
				 * module. Report client can write to their respective modules
				 * at any time.
				 */
				return true;
			}

		} _read_write_policy;

		Module &_lookup(Module::Name const name, bool create_if_not_found)
		{
			for (Module *m = _modules.first(); m; m = m->next())
				if (m->_has_name(name))
					return *m;

			if (!create_if_not_found)
				throw Genode::Service_denied();

			/* module does not exist yet, create one */

			Genode::Session_label session_label(name);

			if (session_label.last_element() != "shape")
				Genode::warning("received unexpected report with label '",
				                session_label, "'");

			/* XXX proper accounting for the used memory is missing */
			/* XXX if we run out of memory, the server will abort */

			Module * const module = new (&_md_alloc)
				Module(_ram, _rm, session_label.prefix(), _read_write_policy,
				       _read_write_policy);

			_modules.insert(module);
			return *module;
		}

		void _try_to_destroy(Module const &module)
		{
			if (module._in_use())
				return;

			_modules.remove(&module);
			Genode::destroy(&_md_alloc, const_cast<Module *>(&module));
		}

	public:

		Registry(Genode::Allocator &md_alloc,
		         Genode::Ram_session &ram, Genode::Region_map &rm,
		         Reader &reader)
		:
			_md_alloc(md_alloc), _ram(ram), _rm(rm), _reader(reader)
		{ }

		Module &lookup(Writer &writer, Module::Name const &name) override
		{
			Module &module = _lookup(name, true);

			module._register(writer);

			/*
			 * Enforce invariant that each module can have only one writer at a
			 * time.
			 */
			if (module._num_writers() > 1) {
				release(writer, module);
				throw Genode::Service_denied();
			}

			module._register(_reader);

			return module;
		}

		void release(Writer &writer, Module &module) override
		{
			module._unregister(_reader);
			module._unregister(writer);
			_try_to_destroy(module);
		}

		Readable_module &lookup(Reader &reader, Module::Name const &rom_label) override
		{
			return _lookup(rom_label, false);
		}

		void release(Reader &reader, Readable_module &module) override
		{
			_try_to_destroy(static_cast<Module&>(module));
		}
};

#endif /* _ROM_REGISTRY_H_ */
