/*
 * \brief  ROM service exposed to the loaded subsystem
 * \author Norman Feske
 * \date   2012-04-17
 */

#ifndef _ROM_H_
#define _ROM_H_

//#include <util/string.h>
#include <rom_session/rom_session.h>
#include <base/rpc_server.h>
#include <os/attached_ram_dataspace.h>

namespace Genode {

	class Rom_module : public List<Rom_module>::Element
	{
		private:

			enum { MAX_NAME_LEN = 64 };
			char _name[MAX_NAME_LEN];

			Ram_session           &_ram;
			Attached_ram_dataspace _fg;
			Attached_ram_dataspace _bg;

			bool _bg_has_pending_data;

			Signal_context_capability _sigh;

			Lock _lock;

		public:

			Rom_module(char const *name, Ram_session &ram_session)
			:
				_ram(ram_session),
				_fg(&_ram, 0), _bg(&_ram, 0),
				_bg_has_pending_data(false),
				_lock(Lock::LOCKED)
			{
				strncpy(_name, name, sizeof(_name));
			}

			bool has_name(char const *name) const
			{
				return strcmp(_name, name) == 0;
			}

			void lock()   { _lock.lock(); }
			void unlock() { _lock.unlock(); }

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
			void sigh(Signal_context_capability sigh) { _sigh = sigh; }

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

			Lock             _lock;
			Ram_session     &_ram_session;
			Allocator       &_md_alloc;
			List<Rom_module> _list;

		public:

			/**
			 * Exception type
			 */
			class Lookup_failed { };

			/**
			 * Constructor
			 *
			 * \param ram_session  RAM session used as backing store for
			 *                     module data
			 * \param md_alloc     backing store for ROM module meta data
			 */
			Rom_module_registry(Ram_session &ram_session, Allocator &md_alloc)
			:
				_ram_session(ram_session), _md_alloc(md_alloc)
			{ }

			~Rom_module_registry()
			{
				Lock::Guard guard(_lock);

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
			Rom_module &lookup_and_lock(char const *name)
			{
				Lock::Guard guard(_lock);

				Rom_module *curr = _list.first();
				for (; curr; curr = curr->next()) {
					if (curr->has_name(name)) {
						curr->lock();
						return *curr;
					}
				}
				throw Lookup_failed();
			}

			Dataspace_capability alloc_rom_module(char const *name, size_t size)
			{
				try {
					Rom_module &module = lookup_and_lock(name);
					Rom_module_lock_guard guard(module);
					return module.bg_dataspace(size);
				}
				catch (Lookup_failed) {

					Lock::Guard guard(_lock);

					Rom_module *module = new (&_md_alloc)
						Rom_module(name, _ram_session);

					Rom_module_lock_guard module_guard(*module);

					_list.insert(module);

					return module->bg_dataspace(size);
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


	class Rom_session_component : public Rpc_object<Rom_session>,
	                              public List<Rom_session_component>::Element
	{
		private:

			Rom_module &_rom_module;

		public:

			Rom_session_component(Rom_module &rom_module)
			: _rom_module(rom_module) { }

			Rom_dataspace_capability dataspace()
			{
				Rom_module_lock_guard guard(_rom_module);
				return _rom_module.fg_dataspace();
			}

			void sigh(Signal_context_capability sigh)
			{
				Rom_module_lock_guard guard(_rom_module);
				_rom_module.sigh(sigh);
			}
	};
}

#endif /* _ROM_H_ */
