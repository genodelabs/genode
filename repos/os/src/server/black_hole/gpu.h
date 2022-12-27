/*
 * \brief  Gpu session component and root
 * \author Martin Stein
 * \date   2022-02-12
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _GPU_H_
#define _GPU_H_

/* Genode includes */
#include <base/attached_ram_dataspace.h>
#include <base/rpc_server.h>
#include <base/session_object.h>
#include <dataspace/client.h>
#include <gpu_session/gpu_session.h>
#include <root/component.h>

namespace Black_hole {

	using namespace Genode;
	using namespace Gpu;

	class Gpu_session;
	class Gpu_root;

	using Gpu_root_component = Root_component<Gpu_session, Multiple_clients>;
}


class Black_hole::Gpu_session : public Session_object<Gpu::Session>
{
	private:

		Env                    &_env;
		Attached_ram_dataspace  _info_dataspace { _env.ram(), _env.rm(), 1 };

	public:

		Gpu_session(Env         &env,
		            Resources    resources,
		            Label const &label,
		            Diag         diag)
		:
			Session_object { env.ep(), resources, label, diag },
			_env           { env }
		{ }


		/***************************
		 ** Gpu session interface **
		 ***************************/

		Dataspace_capability info_dataspace() const override
		{
			return _info_dataspace.cap();
		}

		Sequence_number exec_buffer(Buffer_id /* id */,
		                            size_t    /* size */) override
		{
			throw Gpu::Session::Invalid_state();
		}

		bool complete(Sequence_number /* seqno */) override
		{
			return false;
		}

		void completion_sigh(Signal_context_capability /* sigh */) override
		{ }

		Dataspace_capability alloc_buffer(Buffer_id /* id */,
		                                  size_t    /* size */) override
		{
			return Dataspace_capability { };
		}

		void free_buffer(Buffer_id /* id */) override { }

		Buffer_capability export_buffer(Buffer_id /* id */) override
		{
			return Buffer_capability { };
		}

		void import_buffer(Buffer_capability /* cap */,
		                   Buffer_id         /* id */) override
		{ }

		Dataspace_capability
		map_buffer(Buffer_id          /* id */,
		           bool               /* aperture */,
		           Mapping_attributes /* attrs */) override
		{
			return Dataspace_capability { };
		}

		void unmap_buffer(Buffer_id /* id */) override { }

		bool map_buffer_ppgtt(Buffer_id   /* id */,
		                      Gpu::addr_t /* va */) override
		{
			return false;
		}

		void unmap_buffer_ppgtt(Buffer_id   /* id */,
		                        Gpu::addr_t /* va */) override
		{ }

		Gpu::addr_t query_buffer_ppgtt(Buffer_id /* id */) override
		{
			return (Gpu::addr_t)-1;
		}

		bool set_tiling(Buffer_id      /* id */,
		                uint32_t const /* mode */) override
		{
			return false;
		}
};


class Black_hole::Gpu_root : public Gpu_root_component
{
	private:

		Env &_env;

		size_t _ram_quota(char const *args)
		{
			return Arg_string::find_arg(args, "ram_quota").ulong_value(0);
		}

	protected:

		Gpu_session *_create_session(char const *args) override
		{
			/* at the moment we just need about ~160KiB for initial RCS bring-up */
			size_t const required_quota { Gpu::Session::REQUIRED_QUOTA / 2 };
			size_t const ram_quota      { _ram_quota(args) };

			if (ram_quota < required_quota) {

				Session_label const label { label_from_args(args) };
				warning("insufficient dontated ram_quota (", ram_quota,
				        " bytes), require ", required_quota, " bytes ",
				        " by '", label, "'");

				throw Gpu::Session::Out_of_ram();
			}
			Genode::Session::Resources const resources {
				session_resources_from_args(args) };

			return new (md_alloc())
				Gpu_session {
					_env, resources, session_label_from_args(args),
					session_diag_from_args(args) };
		}

	public:

		Gpu_root(Env       &env,
		         Allocator &alloc)
		:
			Root_component { env.ep(), alloc },
			_env           { env }
		{ }
};

#endif /* _GPU_H_ */
