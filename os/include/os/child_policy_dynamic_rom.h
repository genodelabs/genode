/*
 * \brief  Child policy helper for supplying dynamic ROM modules
 * \author Norman Feske
 * \date   2012-04-04
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__OS__CHILD_POLICY_DYNAMIC_ROM_H_
#define _INCLUDE__OS__CHILD_POLICY_DYNAMIC_ROM_H_

#include <ram_session/ram_session.h>
#include <rom_session/rom_session.h>
#include <base/rpc_server.h>
#include <os/attached_ram_dataspace.h>

namespace Genode {

	class Child_policy_dynamic_rom_file : public Rpc_object<Rom_session>,
	                                      public Service
	{
		private:

			Ram_session *_ram;

			/*
			 * We keep two dataspaces around. The foreground ('_fg') dataspace
			 * is the one we present to the client. While the foreground
			 * dataspace is in use, we perform all modifications of the data
			 * in the background dataspace (which is invisible to the client).
			 * Once the client calls 'dataspace()', we promote the old
			 * background dataspace to the new foreground and thereby hand out
			 * the former background dataspace.
			 */
			Attached_ram_dataspace _fg;
			Attached_ram_dataspace _bg;

			bool _bg_has_pending_data;

			Signal_context_capability _sigh_cap;

			Rpc_entrypoint        &_ep;
			Rom_session_capability _rom_session_cap;

			enum { FILENAME_MAX_LEN = 32 };
			char _filename[FILENAME_MAX_LEN];

		public:

			/**
			 * Constructor
			 *
			 * \param ram  RAM session used to allocate the backing store
			 *             for buffering ROM module data
			 *
			 * If 'ram' is 0, the child policy is ineffective.
			 */
			Child_policy_dynamic_rom_file(const char     *filename,
			                              Rpc_entrypoint &ep,
			                              Ram_session    *ram)
			:
				Service("ROM"),
				_ram(ram),
				_fg(0, 0), _bg(0, 0),
				_bg_has_pending_data(false),
				_ep(ep),
				_rom_session_cap(_ep.manage(this))
			{
				strncpy(_filename, filename, sizeof(_filename));
			}

			/**
			 * Destructor
			 */
			~Child_policy_dynamic_rom_file() { _ep.dissolve(this); }

			/**
			 * Load new content into ROM module
			 *
			 * \throw Ram_session::Alloc_failed
			 * \throw Rm_session::Attach_failed
			 */
			void load(void const *data, size_t data_len)
			{
				if (!_ram) {
					PERR("Error: No backing store for loading ROM data");
					return;
				}

				/* let background buffer grow if needed */
				if (_bg.size() < data_len)
					_bg.realloc(_ram, data_len);

				memcpy(_bg.local_addr<void>(), data, data_len);
				_bg_has_pending_data = true;

				if (_sigh_cap.valid())
					Signal_transmitter(_sigh_cap).submit();
			}


			/***************************
			 ** ROM session interface **
			 ***************************/

			Rom_dataspace_capability dataspace()
			{
				if (!_fg.size() && !_bg_has_pending_data) {
					PERR("Error: no data loaded");
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

			void sigh(Signal_context_capability cap) { _sigh_cap = cap; }


			/***********************
			 ** Service interface **
			 ***********************/

			Session_capability session(const char *) { return _rom_session_cap; }
			void upgrade(Session_capability, const char *) { }
			void close(Session_capability) { }


			/*********************
			 ** Policy function **
			 *********************/

			Service *resolve_session_request(const char *service_name,
			                                 const char *args)
			{
				if (!_ram) return 0;

				/* ignore session requests for non-ROM services */
				if (strcmp(service_name, "ROM")) return 0;

				/* drop out if request refers to another file name */
				char buf[FILENAME_MAX_LEN];
				Arg_string::find_arg(args, "filename").string(buf, sizeof(buf), "");
				return !strcmp(buf, _filename) ? this : 0;
			}
	};
}

#endif /* _INCLUDE__OS__CHILD_POLICY_DYNAMIC_ROM_H_ */
