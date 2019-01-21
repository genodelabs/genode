/*
 * \brief  Report server that dumps reports to the LOG
 * \author Norman Feske
 * \date   2014-01-10
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <report_session/report_session.h>
#include <util/arg_string.h>
#include <base/heap.h>
#include <base/env.h>
#include <base/log.h>
#include <base/session_label.h>
#include <root/component.h>
#include <base/attached_ram_dataspace.h>


namespace Report {
	using namespace Genode;

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

		Session_component(Ram_allocator               &ram,
		                  Region_map                  &rm,
		                  Genode::Session_label const &label,
		                  size_t                       buffer_size)
		:
			_label(label), _ds(ram, rm, buffer_size)
		{ }

		Dataspace_capability dataspace() override { return _ds.cap(); }

		void submit(size_t const length) override
		{
			using namespace Genode;

			log("\nreport: ", _label.string());

			char buf[1024];
			for (size_t consumed = 0; consumed < length; consumed += strlen(buf)) {
				strncpy(buf, _ds.local_addr<char>() + consumed, sizeof(buf));
				log(Cstring(buf));
			}
			log("\nend of report");
		}

		void response_sigh(Genode::Signal_context_capability) override { }

		size_t obtain_response() override { return 0; }
};


class Report::Root : public Genode::Root_component<Session_component>
{
	private:

		Ram_allocator &_ram;
		Region_map    &_rm;

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
				Session_component(_ram, _rm, label, buffer_size);
		}

	public:

		Root(Entrypoint &ep, Genode::Allocator &md_alloc,
		     Ram_allocator &ram, Region_map &rm)
		:
			Genode::Root_component<Session_component>(&ep.rpc_ep(), &md_alloc),
			_ram(ram), _rm(rm)
		{ }
};


struct Report::Main
{
	Env &_env;

	Sliced_heap sliced_heap { _env.ram(), _env.rm() };

	Root root { _env.ep(), sliced_heap, _env.ram(), _env.rm() };

	Main(Env &env) : _env(env)
	{
		env.parent().announce(env.ep().manage(root));
	}
};

void Component::construct(Genode::Env &env) { static Report::Main main(env); }
