/*
 * \brief  Policy applied to all children of the init process
 * \author Norman Feske
 * \date   2010-04-29
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
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
#include <util/arg_string.h>
#include <rom_session/connection.h>

namespace Init {

	/**
	 * Policy for prepending the child name to the 'label' argument
	 *
	 * By applying this policy, the identity of the child becomes imprinted
	 * with each session request.
	 */
	class Child_policy_enforce_labeling
	{
		const char *_name;

		public:

			Child_policy_enforce_labeling(const char *name) : _name(name) { }

			/**
			 * Filter arguments of session request
			 *
			 * This function modifies the 'label' argument and leaves all other
			 * session arguments intact.
			 */
			void filter_session_args(const char *, char *args,
			                         Genode::size_t args_len)
			{
				using namespace Genode;

				char label_buf[Parent::Session_args::MAX_SIZE];
				Arg_string::find_arg(args, "label").string(label_buf, sizeof(label_buf), "");

				char value_buf[Parent::Session_args::MAX_SIZE];
				Genode::snprintf(value_buf, sizeof(value_buf),
				                 "\"%s%s%s\"",
				                 _name,
				                 Genode::strcmp(label_buf, "") == 0 ? "" : " -> ",
				                 label_buf);

				Arg_string::set_arg(args, args_len, "label", value_buf);
			}
	};


	class Child_policy_handle_cpu_priorities
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

				long priority = Arg_string::find_arg(args, "priority").long_value(0);

				long discarded_prio_lsb_bits_mask = (1 << _prio_levels_log2) - 1;
				if (priority & discarded_prio_lsb_bits_mask) {
					PWRN("priority band too small, losing least-significant priority bits");
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


	class Child_policy_provide_rom_file
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

			enum { FILENAME_MAX_LEN = 32 };
			char _filename[FILENAME_MAX_LEN];

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

				Genode::Session_capability session(const char *args)
				{
					if (!_valid)
						throw Invalid_args();

					return _rom_cap;
				}

				void upgrade(Genode::Session_capability, const char *args) { }
				void close(Genode::Session_capability) { }

			} _local_rom_service;

		public:

			/**
			 * Constructor
			 */
			Child_policy_provide_rom_file(const char                  *filename,
			                              Genode::Dataspace_capability ds_cap,
			                              Genode::Rpc_entrypoint      *ep)
			:
				_local_rom_session(ds_cap), _ep(ep),
				_rom_session_cap(_ep->manage(&_local_rom_session)),
				_local_rom_service(_rom_session_cap, ds_cap.valid())
			{
				Genode::strncpy(_filename, filename, sizeof(_filename));
			}

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
				char buf[FILENAME_MAX_LEN];
				Genode::Arg_string::find_arg(args, "filename").string(buf, sizeof(buf), "");
				return !Genode::strcmp(buf, _filename) ? &_local_rom_service : 0;
			}
	};


	class Child_policy_redirect_rom_file
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
				if (!_from || !_to) return;

				/* ignore session requests for non-ROM services */
				if (Genode::strcmp(service, "ROM")) return;

				/* drop out if request refers to another file name */
				enum { FILENAME_MAX_LEN = 32 };
				char buf[FILENAME_MAX_LEN];
				Genode::Arg_string::find_arg(args, "filename").string(buf, sizeof(buf), "");
				if (Genode::strcmp(_from, buf) != 0) return;

				/* replace filename argument */
				Genode::snprintf(buf, sizeof(buf), "\"%s\"", _to);
				Genode::Arg_string::set_arg(args, args_len, "filename", buf);
			}
	};


	class Traditional_child_policy : public Genode::Child_policy,
	                                 public Genode::Client
	{
		protected:

			enum { NAME_LEN = 64 };
			char _name[NAME_LEN];

			Genode::Server                    *_server;
			Genode::Service_registry          *_parent_services;
			Genode::Service_registry          *_child_services;
			Genode::Dataspace_capability       _config_ds;
			Genode::Rpc_entrypoint            *_parent_entrypoint;
			Child_policy_enforce_labeling      _labeling_policy;
			Child_policy_handle_cpu_priorities _priority_policy;
			Child_policy_provide_rom_file      _config_policy;
			Child_policy_provide_rom_file      _binary_policy;

		public:

			/**
			 * Constructor
			 */
			Traditional_child_policy(const char                  *name,
			                         Genode::Server              *server,
			                         Genode::Service_registry    *parent_services,
			                         Genode::Service_registry    *child_services,
			                         Genode::Dataspace_capability config_ds,
			                         Genode::Dataspace_capability binary_ds,
			                         long                         prio_levels_log2,
			                         long                         priority,
			                         Genode::Rpc_entrypoint      *parent_entrypoint)
			:
				_server(server),
				_parent_services(parent_services),
				_child_services(child_services),
				_config_ds(config_ds),
				_parent_entrypoint(parent_entrypoint),
				_labeling_policy(_name),
				_priority_policy(prio_levels_log2, priority),
				_config_policy("config", config_ds, _parent_entrypoint),
				_binary_policy("binary", binary_ds, _parent_entrypoint) {
				Genode::strncpy(_name, name, sizeof(_name)); }

			const char *name() const { return _name; }

			Genode::Service *resolve_session_request(const char *service_name,
			                                         const char *args)
			{
				Genode::Service *service;

				/* check for config file request */
				if ((service = _config_policy.resolve_session_request(service_name, args)))
					return service;

				/* check for binary file request */
				if ((service = _binary_policy.resolve_session_request(service_name, args)))
					return service;

				/* check for services provided by the parent */
				if ((service = _parent_services->find(service_name)))
					return service;

				/*
				 * If the service is provided by one of our children use it,
				 * or wait for the service to become available.
				 */
				return _child_services->wait_for_service(service_name, this,
				                                         name());
			}

			void filter_session_args(const char *service, char *args,
			                         Genode::size_t args_len)
			{
				_labeling_policy.filter_session_args(service, args, args_len);
				_priority_policy.filter_session_args(service, args, args_len);
			}

			bool announce_service(const char              *service_name,
			                      Genode::Root_capability  root,
			                      Genode::Allocator       *alloc,
			                      Genode::Server          *server)
			{
				if (_child_services->find(service_name)) {
					PWRN("%s: service %s is already registered",
					     name(), service_name);
					return false;
				}

				/* XXX remove potential race between checking for and inserting service */

				_child_services->insert(new (alloc)
					Genode::Child_service(service_name, root, _server));
				Genode::printf("%s registered service %s\n", name(), service_name);
				return true;
			}

			void unregister_services()
			{
				Genode::Service *rs;
				while ((rs = _child_services->find_by_server(_server)))
					_child_services->remove(rs);
			}
	};
}

#endif /* _INCLUDE__INIT__CHILD_POLICY_H_ */
