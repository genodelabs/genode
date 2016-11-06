/*
 * \brief  ROM session implementation for serving dynamic content
 * \author Norman Feske
 * \date   2016-11-03
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__OS__DYNAMIC_ROM_SESSION_H_
#define _INCLUDE__OS__DYNAMIC_ROM_SESSION_H_

#include <util/volatile_object.h>
#include <base/rpc_server.h>
#include <base/session_label.h>
#include <base/attached_ram_dataspace.h>
#include <rom_session/rom_session.h>

namespace Genode { class Dynamic_rom_session; }


class Genode::Dynamic_rom_session : public Rpc_object<Rom_session>
{
	public:

		struct Content_producer
		{
			class Buffer_capacity_exceeded : Exception { };

			/**
			 * Write content into the specified buffer
			 *
			 * \throw  Buffer_capacity_exceeded
			 */
			virtual void produce_content(char *dst, size_t dst_len) = 0;
		};

	private:

		/*
		 * Synchronize calls of 'trigger_update' (called locally) with the
		 * 'Rom_session' methods (invoked via RPC).
		 */
		Lock _lock;

		Rpc_entrypoint            &_ep;
		Ram_session               &_ram;
		Region_map                &_rm;
		Signal_context_capability  _sigh;
		Content_producer          &_content_producer;

		/*
		 * Keep track of the last version handed out to the client (at the
		 * time of the last 'Rom_session::update' RPC call, and the newest
		 * version that is available. If the client version is out of date
		 * when the client registers a signal handler, submit a signal
		 * immediately.
		 */
		unsigned _current_version = 0, _client_version = 0;

		size_t _ds_size = 4096;

		Lazy_volatile_object<Attached_ram_dataspace> _ds;

		void _notify_client()
		{
			if (_sigh.valid() && (_current_version != _client_version))
				Signal_transmitter(_sigh).submit();
		}

		bool _unsynchronized_update()
		{
			bool ds_reallocated = false;

			for (;;) {
				try {
					if (!_ds.constructed()) {
						_ds.construct(_ram, _rm, _ds_size);
						ds_reallocated = true;
					}
				}
				catch (Ram_session::Quota_exceeded) {

					error("ouf of child quota while delivering dynamic ROM");

					/*
					 * XXX We may try to generate a resource request on
					 *     behalf of the child.
					 */

					/*
					 * Don't let the child try again to obtain a dataspace
					 * by pretending that the ROM module is up-to-date.
					 */
					return true;
				}
				catch (Ram_session::Out_of_metadata) {
					error("ouf of RAM session quota while delivering dynamic ROM");
					return true;
				}

				try {
					_content_producer.produce_content(_ds->local_addr<char>(),
					                                  _ds->size());
					_client_version = _current_version;
					return !ds_reallocated;
				}
				catch (Content_producer::Buffer_capacity_exceeded) {

					/* force the re-allocation of a larger buffer */
					_ds.destruct();
					_ds_size *= 2;
				}
			}
		}

	public:

		/**
		 * Constructor
		 *
		 * \param ep                entrypoint serving the ROM session
		 * \param ram               RAM session used to allocate the backing
		 *                          store for the dataspace handed out to the
		 *                          client
		 * \param rm                local region map ('env.rm()') required to
		 *                          make the dataspace locally visible to
		 *                          populate its content
		 * \param content_producer  callback to generate the content of the
		 *                          ROM dataspace
		 *
		 * The 'Dynamic_rom_session' associates/disassociates itself with 'ep'.
		 */
		Dynamic_rom_session(Rpc_entrypoint   &ep,
		                    Ram_session      &ram,
		                    Region_map       &rm,
		                    Content_producer &content_producer)
		:
			_ep(ep), _ram(ram), _rm(rm), _content_producer(content_producer)
		{
			_ep.manage(this);
		}

		~Dynamic_rom_session() { _ep.dissolve(this); }

		/*
		 * Called locally, potentially from another thread than 'ep'
		 */
		void trigger_update()
		{
			Lock::Guard guard(_lock);

			_current_version++;
			_notify_client();
		}


		/***************************
		 ** ROM session interface **
		 ***************************/

		Rom_dataspace_capability dataspace() override
		{
			Lock::Guard guard(_lock);

			if (!_ds.constructed())
				_unsynchronized_update();

			Dataspace_capability ds_cap = _ds->cap();

			return static_cap_cast<Rom_dataspace>(ds_cap);
		}

		bool update() override
		{
			Lock::Guard guard(_lock);

			return _unsynchronized_update();
		}

		void sigh(Signal_context_capability sigh) override
		{
			Lock::Guard guard(_lock);

			_sigh = sigh;
			_notify_client();
		}
};

#endif /* _INCLUDE__OS__DYNAMIC_ROM_SESSION_H_ */

