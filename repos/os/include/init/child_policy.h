/*
 * \brief  Policy applied to all children of the init process
 * \author Norman Feske
 * \date   2010-04-29
 */

/*
 * Copyright (C) 2010-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__INIT__CHILD_POLICY_H_
#define _INCLUDE__INIT__CHILD_POLICY_H_

/* Genode includes */
#include <base/service.h>
#include <base/child.h>
#include <base/rpc_server.h>
#include <base/session_label.h>
#include <util/arg_string.h>
#include <rom_session/connection.h>
#include <base/session_label.h>

namespace Init {

	class Child_policy_ram_phys;
	class Child_policy_enforce_labeling;
	class Child_policy_handle_cpu_priorities;
	class Child_policy_provide_rom_file;
	class Child_policy_redirect_rom_file;
	class Traditional_child_policy;
}


class Init::Child_policy_ram_phys
{
	private:

		bool _constrain_phys;

	public:

		Child_policy_ram_phys(bool constrain_phys)
		: _constrain_phys(constrain_phys) { }

		/**
		 * Filter arguments of session request
		 *
		 * This method removes phys_start and phys_size ram_session
		 * parameters if the child configuration does not explicitly
		 * permits this.
		 */
		void filter_session_args(const char *service, char *args,
		                         Genode::size_t args_len)
		{
			using namespace Genode;

			/* intercept only RAM session requests */
			if (Genode::strcmp(service, "RAM"))
				return;

			if (_constrain_phys)
				return;

			Arg_string::remove_arg(args, "phys_start");
			Arg_string::remove_arg(args, "phys_size");
		}
};


/**
 * Policy for prepending the child name to the 'label' argument
 *
 * By applying this policy, the identity of the child becomes imprinted
 * with each session request.
 */
class Init::Child_policy_enforce_labeling
{
	const char *_name;

	public:

		Child_policy_enforce_labeling(const char *name) : _name(name) { }

		/**
		 * Filter arguments of session request
		 *
		 * This method modifies the 'label' argument and leaves all other
		 * session arguments intact.
		 */
		void filter_session_args(const char *, char *args,
		                         Genode::size_t args_len)
		{
			using namespace Genode;

			Session_label const old_label = label_from_args(args);
			if (old_label == "") {
				Arg_string::set_arg_string(args, args_len, "label", _name);
			} else {
				Session_label const name(_name);
				Session_label const new_label = prefixed_label(name, old_label);
				Arg_string::set_arg_string(args, args_len, "label", new_label.string());
			}
		}
};


class Init::Child_policy_handle_cpu_priorities
{
	/* priority parameters */
	long _prio_levels_log2;
	long _priority;

	public:

		Child_policy_handle_cpu_priorities(long prio_levels_log2, long priority)
		: _prio_levels_log2(prio_levels_log2), _priority(priority) { }

		void filter_session_args(const char *service, char *args, Genode::size_t args_len)
		{
			using namespace Genode;

			/* intercept only CPU session requests to scale priorities */
			if (Genode::strcmp(service, "CPU") || _prio_levels_log2 == 0)
				return;

			unsigned long priority = Arg_string::find_arg(args, "priority").ulong_value(0);

			/* clamp priority value to valid range */
			priority = min((unsigned)Cpu_session::PRIORITY_LIMIT - 1, priority);

			long discarded_prio_lsb_bits_mask = (1 << _prio_levels_log2) - 1;
			if (priority & discarded_prio_lsb_bits_mask) {
				warning("priority band too small, losing least-significant priority bits");
			}
			priority >>= _prio_levels_log2;

			/* assign child priority to the most significant priority bits */
			priority |= _priority*(Cpu_session::PRIORITY_LIMIT >> _prio_levels_log2);

			/* override priority when delegating the session request to the parent */
			char value_buf[64];
			Genode::snprintf(value_buf, sizeof(value_buf), "0x%lx", priority);
			Arg_string::set_arg(args, args_len, "priority", value_buf);
		}
};


class Init::Child_policy_provide_rom_file
{
	private:

		struct Local_rom_session_component : Genode::Rpc_object<Genode::Rom_session>
		{
			Genode::Dataspace_capability ds_cap;

			/**
			 * Constructor
			 */
			Local_rom_session_component(Genode::Dataspace_capability ds)
			: ds_cap(ds) { }


			/***************************
			 ** ROM session interface **
			 ***************************/

			Genode::Rom_dataspace_capability dataspace() {
				return Genode::static_cap_cast<Genode::Rom_dataspace>(ds_cap); }

			void sigh(Genode::Signal_context_capability) { }

		} _local_rom_session;

		Genode::Rpc_entrypoint *_ep;
		Genode::Rom_session_capability _rom_session_cap;

		Genode::Session_label _module_name;

		struct Local_rom_service : public Genode::Service
		{
			Genode::Rom_session_capability _rom_cap;
			bool                           _valid;

			/**
			 * Constructor
			 *
			 * \param rom_cap  capability to return on session requests
			 * \param valid    true if local rom service is backed by a
			 *                 valid dataspace
			 */
			Local_rom_service(Genode::Rom_session_capability rom_cap, bool valid)
			: Genode::Service("ROM"), _rom_cap(rom_cap), _valid(valid) { }

			Genode::Session_capability session(char const * /*args*/,
			                                   Genode::Affinity const &)
			{
				if (!_valid)
					throw Invalid_args();

				return _rom_cap;
			}

			void upgrade(Genode::Session_capability, const char * /*args*/) { }
			void close(Genode::Session_capability) { }

		} _local_rom_service;

	public:

		/**
		 * Constructor
		 */
		Child_policy_provide_rom_file(const char                  *module_name,
		                              Genode::Dataspace_capability ds_cap,
		                              Genode::Rpc_entrypoint      *ep)
		:
			_local_rom_session(ds_cap), _ep(ep),
			_rom_session_cap(_ep->manage(&_local_rom_session)),
			_module_name(module_name),
			_local_rom_service(_rom_session_cap, ds_cap.valid())
		{ }

		/**
		 * Destructor
		 */
		~Child_policy_provide_rom_file() { _ep->dissolve(&_local_rom_session); }

		Genode::Service *resolve_session_request(const char *service_name,
		                                         const char *args)
		{
			/* ignore session requests for non-ROM services */
			if (Genode::strcmp(service_name, "ROM")) return 0;

			/* drop out if request refers to another file name */
			{
				Genode::Session_label const label = Genode::label_from_args(args);
				return label.last_element() == _module_name
				       ? &_local_rom_service : nullptr;
			}
		}
};


class Init::Child_policy_redirect_rom_file
{
	private:

		char const *_from;
		char const *_to;

	public:

		Child_policy_redirect_rom_file(const char *from, const char *to)
		: _from(from), _to(to) { }

		void filter_session_args(const char *service,
		                         char *args, Genode::size_t args_len)
		{
			using namespace Genode;

			if (!_from || !_to) return;

			/* ignore session requests for non-ROM services */
			if (Genode::strcmp(service, "ROM")) return;

			/* drop out if request refers to another module name */
			Session_label const label = label_from_args(args);
			Session_label const from(_from);
			if (from != label.last_element()) return;

			/*
			 * The module name corresponds to the last part of the label.
			 * We have to replace this part with the 'to' module name.
			 * If the label consists of only the module name but no prefix,
			 * we replace the entire label with the 'to' module name.
			 */
			Session_label const prefix = label.prefix();
			Session_label const to(_to);

			Session_label const prefixed_to =
				prefixed_label(prefix.valid() ? prefix : Session_label(), to);

			Arg_string::set_arg_string(args, args_len, "label", prefixed_to.string());
		}
};

#endif /* _INCLUDE__INIT__CHILD_POLICY_H_ */
