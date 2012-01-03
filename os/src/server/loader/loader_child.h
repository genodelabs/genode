/*
 * \brief  Loader child interface
 * \author Christian Prochaska
 * \date   2009-10-05
 *
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LOADER_CHILD_H_
#define _LOADER_CHILD_H_

/* Genode includes */
#include <base/child.h>
#include <base/service.h>
#include <util/arg_string.h>
#include <init/child_policy.h>
#include <os/timed_semaphore.h>

/* local includes */
#include <nitpicker_root.h>
#include <rom_root.h>

namespace Loader {

	class Child : public Genode::Child_policy,
	              public Init::Child_policy_enforce_labeling
	{
		private:

			const char *_name;

			/*
			 * Entry point used for serving the parent interface
			 */
			enum { STACK_SIZE = 8*1024 };
			Genode::Rpc_entrypoint _entrypoint;

			Genode::Service_registry *_parent_services;

			Init::Child_policy_provide_rom_file _binary_policy;

			int _max_width;
			int _max_height;

			/**
			 * Monitored Nitpicker service
			 */
			class Nitpicker_service : public Genode::Service
			{
				private:

					Nitpicker::Root _nitpicker_root;

				public:

					Nitpicker_service(Genode::Rpc_entrypoint  *entrypoint,
					                  Genode::Allocator       *md_alloc,
					                  Genode::Timed_semaphore *ready_sem)
					:
						Service("Nitpicker"),
						_nitpicker_root(entrypoint, md_alloc, ready_sem) { }

					Genode::Session_capability session(const char *args)
					{
						return _nitpicker_root.session(args);
					}

					void upgrade(Genode::Session_capability, const char *) { }

					void close(Genode::Session_capability cap)
					{
						/*
						 * Prevent view destruction. When view exists, the view
						 * will be destroyed along with the nitpicker session
						 * when the loader session gets closed.
						 */
						if (!_nitpicker_root.view(0, 0, 0, 0).valid())
							_nitpicker_root.close(cap);
					}

					Nitpicker::View_capability view(int *w, int *h, int *buf_x, int *buf_y)
					{
						return _nitpicker_root.view(w, h, buf_x, buf_y);
					}

			} _nitpicker_service;

			/**
			 * Monitored ROM service
			 */
			class Rom_service : public Genode::Service
			{
				private:

					Rom_root _rom_root;

				public:
					Rom_service(Genode::Rpc_entrypoint  *entrypoint,
					            Genode::Allocator       *md_alloc,
					            Genode::Root_capability  tar_server_root)
					:
						Genode::Service("ROM"),
						_rom_root(entrypoint, entrypoint, md_alloc, tar_server_root)
					{ }

					Genode::Session_capability session(const char *args)
					{
						return _rom_root.session(args);
					}

					void upgrade(Genode::Session_capability, const char *) { }

					void close(Genode::Session_capability cap)
					{
						_rom_root.close(cap);
					}
			} _rom_service;

			struct Resources
			{
				Genode::Ram_connection ram;
				Genode::Cpu_connection cpu;
				Genode::Rm_connection  rm;

				Resources(char const *label, Genode::size_t ram_quota)
				: ram(label), cpu(label)
				{
					/* deduce session costs from usable ram quota */
					Genode::size_t session_donations = Genode::Rm_connection::RAM_QUOTA +
					                                   Genode::Cpu_connection::RAM_QUOTA +
					                                   Genode::Ram_connection::RAM_QUOTA;

					if (ram_quota > session_donations)
						ram_quota -= session_donations;
					else ram_quota = 0;

					ram.ref_account(Genode::env()->ram_session_cap());
					Genode::env()->ram_session()->transfer_quota(
						ram.cap(), ram_quota);
				}
			} _resources;

			 /* must be declared after the service objects for the right destruction order */
			Genode::Child _child;

		public:

			Child(const char                     *name,
			      Genode::Dataspace_capability    elf_ds,
			      Genode::Cap_session            *cap_session,
			      Genode::size_t                  ram_quota,
			      Genode::Service_registry       *parent_services,
			      Genode::Timed_semaphore        *ready_sem,
			      int                             max_width,
			      int                             max_height,
			      Genode::Root_capability         tar_server_root)
			: Init::Child_policy_enforce_labeling(name),
			  _name(name),
			  _entrypoint(cap_session, STACK_SIZE, name, false),
			  _parent_services(parent_services),
			  _binary_policy("binary", elf_ds, &_entrypoint),
			  _max_width(max_width),
			  _max_height(max_height),
			  /* FIXME: temporarily replaced '_child.heap()' with 'env()->heap()' */
			  _nitpicker_service(&_entrypoint, Genode::env()->heap(), ready_sem),
			  _rom_service(&_entrypoint, Genode::env()->heap(), tar_server_root),
			  _resources(name, ram_quota),
			  _child(elf_ds, _resources.ram.cap(), _resources.cpu.cap(),
			         _resources.rm.cap(), &_entrypoint, this)
			{
				_entrypoint.activate();
			}

			/****************************
			 ** Child-policy interface **
			 ****************************/

			const char *name() const { return _name; }

			void filter_session_args(const char *service_name, char *args, Genode::size_t args_len)
			{
				using namespace Genode;

				Init::Child_policy_enforce_labeling::filter_session_args(0, args, args_len);

				if (!strcmp(service_name, "Nitpicker")) {

					/*
					 * Restrict the child's framebuffer size to the maximum
					 * size given by the client.
					 */

					if (_max_width > -1) {
						int fb_width = Arg_string::find_arg(args, "fb_width" ).long_value(_max_width);
						if (!Arg_string::set_arg(args, args_len, "fb_width",
						                         min(fb_width, _max_width))) {
							PERR("could not set fb_width argument");
						}
					}
					if (_max_height > -1) {
						int fb_height = Arg_string::find_arg(args, "fb_height" ).long_value(_max_height);
						if (!Arg_string::set_arg(args, args_len, "fb_height",
						                         min(fb_height, _max_height))) {
							PERR("could not set fb_height argument");
						}
					}
				}
			}

			Genode::Service *resolve_session_request(const char *service_name,
			                                         const char *args)
			{
				using namespace Genode;

				Service *service = 0;

				PDBG("service_name = %s", service_name);

				/* check for binary file request */
				if ((service = _binary_policy.resolve_session_request(service_name, args)))
					return service;

				if (!strcmp(service_name, "Nitpicker"))
					return &_nitpicker_service;

				if (!strcmp(service_name, "ROM"))
					return &_rom_service;

				service = _parent_services->find(service_name);
				if (!service) {
					service = new (env()->heap()) Parent_service(service_name);
					_parent_services->insert(service);
				}
				return service;
			}

			Nitpicker::View_capability view(int *w, int *h, int *buf_x, int *buf_y)
			{
				return _nitpicker_service.view(w, h, buf_x, buf_y);
			}
	};
}

#endif /* _LOADER_CHILD_H_ */
