/*
 * \brief  ROM service exposed to the loaded subsystem
 * \author Norman Feske
 * \date   2012-04-17
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _ROM_H_
#define _ROM_H_

#include <rom_session/rom_session.h>
#include <base/rpc_server.h>
#include <base/attached_ram_dataspace.h>
#include <base/attached_rom_dataspace.h>

namespace Genode {

	class Rom_module : public List<Rom_module>::Element
	{
		public:

			typedef String<128> Name;

		private:

			Name const _name;

			Ram_allocator         &_ram;
			Attached_ram_dataspace _fg;
			Attached_ram_dataspace _bg;

			Constructible<Attached_rom_dataspace> _parent_rom { };

			bool _bg_has_pending_data = false;

			Signal_context_capability _sigh { };

			Blockade _blockade { };

		public:

			enum Origin { PARENT_PROVIDED, SESSION_LOCAL };

			Rom_module(Env &env, Xml_node /* config */, Name const &name,
			           Ram_allocator &ram_allocator, Origin origin)
			:
				_name(name), _ram(ram_allocator),
				_fg(_ram, env.rm(), 0), _bg(_ram, env.rm(), 0)
			{
				if (origin == SESSION_LOCAL)
					return;

				try {
					_parent_rom.construct(env, name.string()); }
				catch (...) {
					warning("ROM ", name, " unavailable from parent, "
					        "try to use session-local ROM");
				}
			}

			bool has_name(Name const &name) const { return _name == name; }

			void lock()   { _blockade.block(); }
			void unlock() { _blockade.wakeup(); }

			/**
			 * Return dataspace as handed out to loader session
			 *
			 * The loader session submits the data into the ROM module into
			 * the background dataspace. Once it is ready, the 'commit_bg()'
			 * function is called.
			 */
			Dataspace_capability bg_dataspace(size_t size)
			{
				/* let background buffer grow if needed */
				if (_bg.size() < size)
					_bg.realloc(&_ram, size);

				return _bg.cap();
			}

			/**
			 * Return dataspace as handed out to ROM session client
			 */
			Rom_dataspace_capability fg_dataspace()
			{
				if (_parent_rom.constructed())
					return static_cap_cast<Rom_dataspace>(_parent_rom->cap());

				if (!_fg.size() && !_bg_has_pending_data) {
					Genode::error("no data loaded");
					return Rom_dataspace_capability();
				}

				/*
				 * Keep foreground if no background exists. Otherwise, use
				 * old background as new foreground.
				 */
				if (_bg_has_pending_data) {
					_fg.swap(_bg);
					_bg_has_pending_data = false;
				}

				Dataspace_capability ds_cap = _fg.cap();
				return static_cap_cast<Rom_dataspace>(ds_cap);
			}

			/**
			 * Set signal handler to inform about ROM module updates
			 *
			 * This function is indirectly called by the ROM session client.
			 */
			void sigh(Signal_context_capability sigh)
			{
				if (_parent_rom.constructed())
					_parent_rom->sigh(sigh);

				_sigh = sigh;
			}

			/**
			 * Commit data contained in background dataspace
			 *
			 * This function is indirectly called by the loader session.
			 */
			void commit_bg()
			{
				_bg_has_pending_data = true;

				if (_sigh.valid())
					Signal_transmitter(_sigh).submit();
			}
	};


	struct Rom_module_lock_guard
	{
		Rom_module &rom;

		Rom_module_lock_guard(Rom_module &rom) : rom(rom) { }

		~Rom_module_lock_guard() { rom.unlock(); }
	};


	class Rom_module_registry
	{
		private:

			Env             &_env;
			Xml_node   const _config;
			Mutex            _mutex { };
			Ram_allocator   &_ram_allocator;
			Allocator       &_md_alloc;
			List<Rom_module> _list { };

		public:

			/**
			 * Exception type
			 */
			class Lookup_failed { };

			/**
			 * Constructor
			 *
			 * \param ram_allocator  RAM allocator used as backing store for
			 *                       module data
			 * \param md_alloc       backing store for ROM module meta data
			 */
			Rom_module_registry(Env &env, Xml_node config,
			                    Ram_allocator &ram_allocator,
			                    Allocator &md_alloc)
			:
				_env(env), _config(config), _ram_allocator(ram_allocator),
				_md_alloc(md_alloc)
			{ }

			~Rom_module_registry()
			{
				Mutex::Guard guard(_mutex);

				while (_list.first()) {
					Rom_module *rom = _list.first();
					rom->lock();
					_list.remove(rom);
					destroy(&_md_alloc, rom);
				}
			}

			/**
			 * Lookup ROM module by name
			 *
			 * \throw  Lookup_failed
			 */
			Rom_module &lookup_and_lock(Rom_module::Name const &name)
			{
				Mutex::Guard guard(_mutex);

				Rom_module *curr = _list.first();
				for (; curr; curr = curr->next()) {
					if (curr->has_name(name)) {
						curr->lock();
						return *curr;
					}
				}
				throw Lookup_failed();
			}

			Dataspace_capability alloc_rom_module(Rom_module::Name const &name, size_t size)
			{
				try {
					Rom_module &module = lookup_and_lock(name);
					Rom_module_lock_guard guard(module);
					return module.bg_dataspace(size);
				}
				catch (Lookup_failed) {

					Mutex::Guard guard(_mutex);

					Rom_module *module = new (&_md_alloc)
						Rom_module(_env, _config, name, _ram_allocator,
						           Rom_module::SESSION_LOCAL);

					Rom_module_lock_guard module_guard(*module);

					_list.insert(module);

					return module->bg_dataspace(size);
				}
			}

			void fetch_parent_rom_module(Rom_module::Name const &name)
			{
				try {
					lookup_and_lock(name);
				}
				catch (Lookup_failed) {

					Mutex::Guard guard(_mutex);

					Rom_module *module = new (&_md_alloc)
						Rom_module(_env, _config, name, _ram_allocator,
						           Rom_module::PARENT_PROVIDED);

					Rom_module_lock_guard module_guard(*module);

					_list.insert(module);
				}
			}

			/**
			 * \throw Lookup_failed
			 */
			void commit_rom_module(char const *name)
			{
				Rom_module &rom_module = lookup_and_lock(name);
				rom_module.commit_bg();
				rom_module.unlock();
			}
	};


	class Rom_session_component : public  Rpc_object<Rom_session>,
	                              private List<Rom_session_component>::Element
	{
		private:

			friend class List<Rom_session_component>;

			Entrypoint &_ep;
			Rom_module &_rom_module;

		public:

			Rom_session_component(Entrypoint &ep, Rom_module &rom_module)
			: _ep(ep), _rom_module(rom_module) { _ep.manage(*this); }

			~Rom_session_component() { _ep.dissolve(*this); }

			Rom_dataspace_capability dataspace() override
			{
				Rom_module_lock_guard guard(_rom_module);
				return _rom_module.fg_dataspace();
			}

			void sigh(Signal_context_capability sigh) override
			{
				Rom_module_lock_guard guard(_rom_module);
				_rom_module.sigh(sigh);
			}
	};
}

#endif /* _ROM_H_ */
