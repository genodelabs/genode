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
		Genode::Ram_allocator          &_ram;
		Genode::Region_map             &_rm;
		Genode::Attached_rom_dataspace &_config_rom;

		Module_list _modules { };

		struct Read_write_policy : Module::Read_policy, Module::Write_policy
		{
			bool read_permitted(Module const &,
			                    Writer const &,
			                    Reader const &) const override
			{
				/*
				 * The access-control policy is applied at the ROM-session
				 * construction time by applying the '_report_name' method
				 * on the session label. Once connected to a ROM module,
				 * the ROM client is always allowed to read the ROM content.
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

		} _read_write_policy { };

		Module &_lookup(Module::Name const name)
		{
			for (Module *m = _modules.first(); m; m = m->next())
				if (m->_has_name(name))
					return *m;

			/* module does not exist yet, create one */

			/* XXX proper accounting for the used memory is missing */
			/* XXX if we run out of memory, the server will abort */

			Module * const module = new (&_md_alloc)
				Module(_ram, _rm, name, _read_write_policy, _read_write_policy);

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

		template <typename USER>
		Module &_lookup(USER &user, Module::Name const &name)
		{
			Module &module = _lookup(name);
			module._register(user);
			return module;
		}

		template <typename USER>
		void _release(USER &user, Module const &module)
		{
			/*
			 * The '_release' function is called by both the report service
			 * and the ROM service. The latter has merely a 'const' version
			 * of the module because it is not supposed to modify it. However,
			 * when closing a ROM session, we have to disassociate the ROM
			 * session from the module. To do that, we need a non-const
			 * reference to the module.
			 */
			const_cast<Module &>(module)._unregister(user);
			_try_to_destroy(module);
		}

		/**
		 * Return report name that corresponds to the given ROM session label
		 *
		 * \throw Registry_for_reader::Lookup_failed
		 */
		Module::Name _report_name(Module::Name const &rom_label) const
		{
			using namespace Genode;

			_config_rom.update();

			try {
				Session_policy policy(rom_label, _config_rom.xml());
				return policy.attribute_value("report", Module::Name());
			}
			catch (Session_policy::No_policy_defined) { }

			warning("no valid policy for ROM request '", rom_label, "'");
			throw Service_denied();
		}

	public:

		Registry(Genode::Allocator &md_alloc,
		         Genode::Ram_allocator &ram, Genode::Region_map &rm,
		         Genode::Attached_rom_dataspace &config_rom)
		:
			_md_alloc(md_alloc), _ram(ram), _rm(rm), _config_rom(config_rom)
		{ }

		Module &lookup(Writer &writer, Module::Name const &name) override
		{
			Module &module = _lookup(writer, name);

			/*
			 * Enforce invariant that each module can have only one writer at a
			 * time.
			 */
			if (module._num_writers() > 1) {
				release(writer, module);
				throw Genode::Service_denied();
			}

			return module;
		}

		void release(Writer &writer, Module &module) override
		{
			return _release(writer, module);
		}

		Readable_module &lookup(Reader &reader, Module::Name const &rom_label) override
		{
			return _lookup(reader, _report_name(rom_label));
		}

		void release(Reader &reader, Readable_module &module) override
		{
			return _release(reader, static_cast<Module &>(module));
		}
};

#endif /* _ROM_REGISTRY_H_ */
