/*
 * \brief  Report server that dumps reports to the LOG
 * \author Norman Feske
 * \date   2014-01-10
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <report_session/report_session.h>
#include <util/arg_string.h>
#include <base/heap.h>
#include <base/env.h>
#include <base/session_label.h>
#include <base/printf.h>
#include <root/component.h>
#include <os/server.h>
#include <os/attached_ram_dataspace.h>


namespace Report {
	using Server::Entrypoint;
	using Genode::env;

	class  Session_component;
	class  Root;
	struct Main;
}


class Report::Session_component : public Genode::Rpc_object<Session>
{
	private:

		Genode::Session_label _label;

		Genode::Attached_ram_dataspace _ds;

	public:

		Session_component(Genode::Session_label const &label, size_t buffer_size)
		:
			_label(label), _ds(env()->ram_session(), buffer_size)
		{ }

		Dataspace_capability dataspace() override { return _ds.cap(); }

		void submit(size_t const length) override
		{
			using namespace Genode;

			printf("\nreport: %s\n", _label.string());

			char buf[1024];
			for (size_t consumed = 0; consumed < length; consumed += strlen(buf)) {
				strncpy(buf, _ds.local_addr<char>() + consumed, sizeof(buf));
				printf("%s", buf);
			}
			printf("\nend of report\n");
		}

		void response_sigh(Genode::Signal_context_capability) override { }

		size_t obtain_response() override { return 0; }
};


class Report::Root : public Genode::Root_component<Session_component>
{
	protected:

		Session_component *_create_session(const char *args) override
		{
			using namespace Genode;

			/* read label from session arguments */
			Session_label label = label_from_args(args);

			/* read report buffer size from session arguments */
			size_t const buffer_size =
				Arg_string::find_arg(args, "buffer_size").ulong_value(0);

			return new (md_alloc())
				Session_component(label, buffer_size);
		}

	public:

		Root(Entrypoint &ep, Genode::Allocator &md_alloc)
		:
			Genode::Root_component<Session_component>(&ep.rpc_ep(), &md_alloc)
		{ }
};


struct Report::Main
{
	Entrypoint &ep;

	Genode::Sliced_heap sliced_heap = { env()->ram_session(),
	                                    env()->rm_session() };
	Root root = { ep, sliced_heap };

	Main(Entrypoint &ep) : ep(ep)
	{
		env()->parent()->announce(ep.manage(root));
	}
};


namespace Server {

	char const *name() { return "log_report_ep"; }

	size_t stack_size() { return 16*1024*sizeof(long); }

	void construct(Entrypoint &ep)
	{
		static Report::Main main(ep);
	}
}
