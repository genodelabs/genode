/*
 * \brief  Policy applied to all children of the init process
 * \author Norman Feske
 * \date   2010-04-29
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__INIT__CHILD_POLICY_H_
#define _INCLUDE__INIT__CHILD_POLICY_H_

/* Genode includes */
#include <base/service.h>
#include <base/child.h>
#include <base/session_label.h>
#include <base/attached_ram_dataspace.h>
#include <util/arg_string.h>
#include <rom_session/connection.h>
#include <base/session_label.h>
#include <os/dynamic_rom_session.h>

namespace Init {

	class Child_policy_ram_phys;
	class Child_policy_handle_cpu_priorities;
	class Child_policy_provide_rom_file;
	class Child_policy_provide_dynamic_rom;
	class Child_policy_redirect_rom_file;
	class Traditional_child_policy;

	using namespace Genode;
	using Genode::size_t;
	using Genode::strcmp;
	using Genode::snprintf;
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
		                         size_t args_len)
		{
			/* intercept only RAM session requests */
			if (strcmp(service, "RAM"))
				return;

			if (_constrain_phys)
				return;

			Arg_string::remove_arg(args, "phys_start");
			Arg_string::remove_arg(args, "phys_size");
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

		void filter_session_args(const char *service, char *args, size_t args_len)
		{
			/* intercept only CPU session requests to scale priorities */
			if (strcmp(service, "CPU") || _prio_levels_log2 == 0)
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
			snprintf(value_buf, sizeof(value_buf), "0x%lx", priority);
			Arg_string::set_arg(args, args_len, "priority", value_buf);
		}
};


class Init::Child_policy_provide_rom_file
{
	private:

		struct Local_rom_session_component : Rpc_object<Rom_session>
		{
			Rpc_entrypoint      &ep;
			Dataspace_capability ds_cap;

			/**
			 * Constructor
			 */
			Local_rom_session_component(Rpc_entrypoint &ep,
			                            Dataspace_capability ds)
			: ep(ep), ds_cap(ds) { ep.manage(this); }

			~Local_rom_session_component() { ep.dissolve(this); }


			/***************************
			 ** ROM session interface **
			 ***************************/

			Rom_dataspace_capability dataspace() override {
				return static_cap_cast<Rom_dataspace>(ds_cap); }

			void sigh(Signal_context_capability) override { }

		} _session;

		Session_label const _module_name;

		typedef Local_service<Local_rom_session_component> Service;

		Service::Single_session_factory _session_factory { _session };
		Service                         _service { _session_factory };

	public:

		/**
		 * Constructor
		 */
		Child_policy_provide_rom_file(Session_label const &module_name,
		                              Dataspace_capability ds_cap,
		                              Rpc_entrypoint      *ep)
		:
			_session(*ep, ds_cap), _module_name(module_name)
		{ }

		Service *resolve_session_request_with_label(Service::Name const &name,
		                                            Session_label const &label)
		{
			return (name == "ROM" && label.last_element() == _module_name)
			       ? &_service : nullptr;
		}

		Service *resolve_session_request(const char *service_name,
		                                         const char *args)
		{
			return resolve_session_request_with_label(service_name,
			                                          label_from_args(args));
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
		                         char *args, size_t args_len)
		{
			if (!_from || !_to) return;

			/* ignore session requests for non-ROM services */
			if (strcmp(service, "ROM")) return;

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
