/*
 * \brief  Server that aggregates reports and exposes them as ROM modules
 * \author Norman Feske
 * \date   2014-01-11
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _REPORT_SERVICE_H_
#define _REPORT_SERVICE_H_

/* Genode includes */
#include <util/arg_string.h>
#include <report_session/report_session.h>
#include <root/component.h>

/* local includes */
#include <rom_registry.h>


namespace Report {
	struct Session_component;
	struct Root;
}


struct Report::Session_component : Genode::Rpc_object<Session>, Rom::Writer
{
	private:

		Rom::Registry_for_writer &_registry;

		Genode::Attached_ram_dataspace _ds;

		Rom::Module &_module;

	public:

		Session_component(Rom::Module::Name const &name, size_t buffer_size,
		                  Rom::Registry_for_writer &registry)
		:
			_registry(registry),
			_ds(Genode::env()->ram_session(), buffer_size),
			_module(_registry.lookup(*this, name))
		{ }

		/**
		 * Destructor
		 *
		 * Clear report when the report session gets closes.
		 */
		~Session_component()
		{
			_module.write_content(0, 0);
			_registry.release(*this, _module);
		}

		Dataspace_capability dataspace() override { return _ds.cap(); }

		void submit(size_t const length) override
		{
			_module.write_content(_ds.local_addr<char>(),
			                      Genode::min(length, _ds.size()));
		}

		void response_sigh(Genode::Signal_context_capability) override { }

		size_t obtain_response() override { return 0; }
};


struct Report::Root : Genode::Root_component<Session_component>
{
	private:

		Rom::Registry_for_writer &_rom_registry;

	protected:

		Session_component *_create_session(const char *args) override
		{
			using namespace Genode;

			/* read label from session arguments */
			char label[200];
			Arg_string::find_arg(args, "label").string(label, sizeof(label), "");

			/* read report buffer size from session arguments */
			size_t const buffer_size =
				Arg_string::find_arg(args, "buffer_size").long_value(0);

			return new (md_alloc())
				Session_component(Rom::Module::Name(label), buffer_size,
				                  _rom_registry);
		}

	public:

		Root(Server::Entrypoint       &ep,
		     Genode::Allocator        &md_alloc,
		     Rom::Registry_for_writer &rom_registry)
		:
			Genode::Root_component<Session_component>(&ep.rpc_ep(), &md_alloc),
			_rom_registry(rom_registry)
		{ }
};

#endif /* _REPORT_SERVICE_H_ */
