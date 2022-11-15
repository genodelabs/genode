/*
 * \brief  Report session component and root
 * \author Josef Soentgen
 * \date   2022-11-15
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _REPORT_H_
#define _REPORT_H_

/* base includes */
#include <base/attached_ram_dataspace.h>
#include <report_session/report_session.h>
#include <root/component.h>

namespace Black_hole {

	using namespace Genode;

	class  Report_session;
	class  Report_root;
}


class Black_hole::Report_session : public Session_object<Report::Session>
{
	private:

		enum { RAM_DS_SIZE = 16 };

		Env                     &_env;
		Attached_ram_dataspace   _ram_ds  { _env.ram(), _env.rm(), RAM_DS_SIZE };

	public:

		Report_session(Env             &env,
		               Resources const &resources,
		               Label     const &label,
		               Diag      const &diag)
		:
			Session_object(env.ep(), resources, label, diag),
			_env { env }
		{
			copy_cstring(_ram_ds.local_addr<char>(), "<empty/>", RAM_DS_SIZE);
		}

		Dataspace_capability dataspace() override
		{
			return static_cap_cast<Dataspace>(_ram_ds.cap());
		}

		void submit(size_t /* length */) override { }

		void response_sigh(Signal_context_capability /* sigh */) override { }

		size_t obtain_response() override { return RAM_DS_SIZE; }
};


class Black_hole::Report_root : public Root_component<Report_session>
{
	private:

		Env &_env;

	protected:

		Report_session *_create_session(const char *args) override
		{
			return new (md_alloc())
				Report_session {
					_env, session_resources_from_args(args),
					session_label_from_args(args),
					session_diag_from_args(args) };
		}

	public:

		Report_root(Env       &env,
		            Allocator &alloc)
		:
			Root_component<Report_session> { &env.ep().rpc_ep(), &alloc },
			_env                           { env }
		{ }
};

#endif /* _REPORT_H_ */
