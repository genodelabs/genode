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

#include <base/ram_allocator.h>
#include <base/rpc_server.h>
#include <base/attached_ram_dataspace.h>
#include <rom_session/rom_session.h>

namespace Genode { class Child_policy_dynamic_rom_file; }


class Genode::Child_policy_dynamic_rom_file : public Rpc_object<Rom_session>,
                                              public Service
{
	private:

		/*
		 * Noncopyable
		 */
		Child_policy_dynamic_rom_file(Child_policy_dynamic_rom_file const &);
		Child_policy_dynamic_rom_file &operator = (Child_policy_dynamic_rom_file const &);

		Ram_allocator *_ram;
		Region_map    &_rm;

		/*
		 * The ROM module may be written and consumed by different threads,
		 * e.g., written by the main thread and consumed by the child's
		 * entrypoint that manages the local ROM service for handing out a
		 * dynamic config. Hence, the '_mutex' is used to synchronize the
		 * 'load' and 'dataspace' methods.
		 */
		Mutex _mutex { };

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

		Signal_context_capability _sigh_cap { };

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
		 * \throw Out_of_ram
		 * \throw Out_of_caps
		 *
		 * If 'ram' is 0, the child policy is ineffective.
		 */
		Child_policy_dynamic_rom_file(Region_map     &rm,
		                              char const     *module_name,
		                              Rpc_entrypoint &ep,
		                              Ram_allocator  *ram)
		:
			Service("ROM"),
			_ram(ram), _rm(rm),
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
		 * \throw Out_of_ram
		 * \throw Out_of_caps
		 * \throw Region_map::Region_conflict
		 * \throw Region_map::Invalid_dataspace
		 */
		void load(void const *data, size_t data_len)
		{
			Mutex::Guard guard(_mutex);

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

		Rom_dataspace_capability dataspace() override
		{
			Mutex::Guard guard(_mutex);

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

		void sigh(Signal_context_capability cap) override { _sigh_cap = cap; }


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

			case Session_state::SERVICE_DENIED:
			case Session_state::INSUFFICIENT_RAM_QUOTA:
			case Session_state::INSUFFICIENT_CAP_QUOTA:
			case Session_state::AVAILABLE:
			case Session_state::CAP_HANDED_OUT:
			case Session_state::CLOSED:
				break;
			}
		}


		/**********************
		 ** Policy interface **
		 **********************/

		Service *resolve_session_request(Service::Name const &name,
		                                 Session_label const &label)
		{
			if (!_ram) return nullptr;

			/* ignore session requests for non-ROM services */
			if (name != "ROM") return nullptr;

			/* drop out if request refers to another module name */
			return _module_name == label.last_element() ? this : 0;
		}
};

#endif /* _INCLUDE__OS__CHILD_POLICY_DYNAMIC_ROM_H_ */
