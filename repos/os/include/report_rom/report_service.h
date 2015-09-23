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

#ifndef _INCLUDE__REPORT_ROM__REPORT_SERVICE_H_
#define _INCLUDE__REPORT_ROM__REPORT_SERVICE_H_

/* Genode includes */
#include <util/arg_string.h>
#include <report_session/report_session.h>
#include <root/component.h>
#include <util/print_lines.h>
#include <report_rom/rom_registry.h>


namespace Report {
	struct Session_component;
	struct Root;
}


struct Report::Session_component : Genode::Rpc_object<Session>, Rom::Writer
{
	private:

		Rom::Registry_for_writer &_registry;

		Genode::Session_label const _label;

		Genode::Attached_ram_dataspace _ds;

		Rom::Module &_module;

		bool &_verbose;

		Rom::Module &_create_module(Rom::Module::Name const &name)
		{
			try {
				return _registry.lookup(*this, name);
			} catch (...) {
				throw Genode::Root::Invalid_args();
			}
		}

		static void _log_lines(char const *string, size_t len)
		{
			Genode::print_lines<200>(string, len,
			                         [&] (char const *line) { PLOG("  %s", line); });
		}

	public:

		Session_component(Genode::Session_label const &label, size_t buffer_size,
		                  Rom::Registry_for_writer &registry, bool &verbose)
		:
			_registry(registry), _label(label),
			_ds(Genode::env()->ram_session(), buffer_size),
			_module(_create_module(label.string())),
			_verbose(verbose)
		{ }

		~Session_component()
		{
			_registry.release(*this, _module);
		}

		/**
		 * Rom::Writer interface
		 */
		Genode::Session_label label() const override { return _label; }

		Dataspace_capability dataspace() override { return _ds.cap(); }

		void submit(size_t length) override
		{
			length = Genode::min(length, _ds.size());

			if (_verbose) {
				PLOG("report '%s'", _module.name().string());
				_log_lines(_ds.local_addr<char>(), length);
			}

			_module.write_content(*this, _ds.local_addr<char>(), length);
		}

		void response_sigh(Genode::Signal_context_capability) override { }

		size_t obtain_response() override { return 0; }
};


struct Report::Root : Genode::Root_component<Session_component>
{
	private:

		Rom::Registry_for_writer &_rom_registry;
		bool                     &_verbose;

	protected:

		Session_component *_create_session(const char *args) override
		{
			using namespace Genode;

			/* read report buffer size from session arguments */
			size_t const buffer_size =
				Arg_string::find_arg(args, "buffer_size").ulong_value(0);

			return new (md_alloc())
				Session_component(Genode::Session_label(args), buffer_size,
				                  _rom_registry, _verbose);
		}

	public:

		Root(Server::Entrypoint       &ep,
		     Genode::Allocator        &md_alloc,
		     Rom::Registry_for_writer &rom_registry,
		     bool                     &verbose)
		:
			Genode::Root_component<Session_component>(&ep.rpc_ep(), &md_alloc),
			_rom_registry(rom_registry), _verbose(verbose)
		{ }
};

#endif /* _INCLUDE__REPORT_ROM__REPORT_SERVICE_H_ */
