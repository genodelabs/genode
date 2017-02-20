/*
 * \brief  Child policy helper for supplying dynamic ROM modules
 * \author Norman Feske
 * \date   2012-04-04
 *
 * \deprecated use 'Local_service::Single_session_service' combined with
 *             'Dynamic_rom_session' instead
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__OS__CHILD_POLICY_DYNAMIC_ROM_H_
#define _INCLUDE__OS__CHILD_POLICY_DYNAMIC_ROM_H_

#include <ram_session/ram_session.h>
#include <rom_session/rom_session.h>
#include <base/rpc_server.h>
#include <base/attached_ram_dataspace.h>

namespace Genode { class Child_policy_dynamic_rom_file; }


class Genode::Child_policy_dynamic_rom_file : public Rpc_object<Rom_session>,
                                              public Service
{
	private:

		Ram_session *_ram;
		Region_map  &_rm;

		/*
		 * The ROM module may be written and consumed by different threads,
		 * e.g., written by the main thread and consumed by the child's
		 * entrypoint that manages the local ROM service for handing out a
		 * dynamic config. Hence, the '_lock' is used to synchronize the
		 * 'load' and 'dataspace' methods.
		 */
		Lock _lock;

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

		Session_label _module_name;

	public:

		/**
		 * Constructor
		 *
		 * \param ram  RAM session used to allocate the backing store
		 *             for buffering ROM module data
		 *
		 * If 'ram' is 0, the child policy is ineffective.
		 */
		Child_policy_dynamic_rom_file(Region_map     &rm,
		                              char const     *module_name,
		                              Rpc_entrypoint &ep,
		                              Ram_session    *ram)
		:
			Service("ROM", Ram_session_capability()),
			_ram(ram), _rm(rm),
			_fg(*_ram, _rm, 0), _bg(*_ram, _rm, 0),
			_bg_has_pending_data(false),
			_ep(ep),
			_rom_session_cap(_ep.manage(this)),
			_module_name(module_name)
		{ }

		/**
		 * Constructor
		 *
		 * \param ram  RAM session used to allocate the backing store
		 *             for buffering ROM module data
		 *
		 * \deprecated
		 *
		 * If 'ram' is 0, the child policy is ineffective.
		 */
		Child_policy_dynamic_rom_file(char const     *module_name,
		                              Rpc_entrypoint &ep,
		                              Ram_session    *ram) __attribute__((deprecated))
		:
			Service("ROM", Ram_session_capability()),
			_ram(ram), _rm(*env_deprecated()->rm_session()),
			_fg(*_ram, _rm, 0), _bg(*_ram, _rm, 0),
			_bg_has_pending_data(false),
			_ep(ep),
			_rom_session_cap(_ep.manage(this)),
			_module_name(module_name)
		{ }

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
			Lock::Guard guard(_lock);

			if (!_ram) {
				Genode::error("no backing store for loading ROM data");
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
			Lock::Guard guard(_lock);

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

		void sigh(Signal_context_capability cap) { _sigh_cap = cap; }


		/***********************
		 ** Service interface **
		 ***********************/

		void initiate_request(Session_state &session) override
		{
			switch (session.phase) {

			case Session_state::CREATE_REQUESTED:
				session.cap   = _rom_session_cap;
				session.phase = Session_state::AVAILABLE;
				break;

			case Session_state::UPGRADE_REQUESTED:
				session.phase = Session_state::CAP_HANDED_OUT;
				session.confirm_ram_upgrade();
				break;

			case Session_state::CLOSE_REQUESTED:
				session.phase = Session_state::CLOSED;
				break;

			case Session_state::INVALID_ARGS:
			case Session_state::QUOTA_EXCEEDED:
			case Session_state::AVAILABLE:
			case Session_state::CAP_HANDED_OUT:
			case Session_state::CLOSED:
				break;
			}
		}


		/**********************
		 ** Policy interface **
		 **********************/

		Service *resolve_session_request(const char *service_name,
		                                 const char *args)
		{
			if (!_ram) return 0;

			/* ignore session requests for non-ROM services */
			if (strcmp(service_name, "ROM")) return 0;

			/* drop out if request refers to another module name */
			Session_label const label = label_from_args(args);
			return _module_name == label.last_element() ? this : 0;
		}
};

#endif /* _INCLUDE__OS__CHILD_POLICY_DYNAMIC_ROM_H_ */
