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

		bool &_verbose;

		size_t const _str_line_end(char const * const str, size_t const len)
		{
			size_t i = 0;
			for (; str[i] && i < len && str[i] != '\n'; i++);

			/* the newline character belongs to the line */
			if (str[i] == '\n')
				i++;

			return i;
		}

	public:

		Session_component(Rom::Module::Name const &name, size_t buffer_size,
		                  Rom::Registry_for_writer &registry, bool &verbose)
		:
			_registry(registry),
			_ds(Genode::env()->ram_session(), buffer_size),
			_module(_registry.lookup(*this, name)),
			_verbose(verbose)
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

		void submit(size_t length) override
		{
			length = Genode::min(length, _ds.size());

			if (_verbose) {

				PLOG("report '%s'", _module.name().string());

				/*
				 * We cannot simply print the content of the report dataspace
				 * as one string because we cannot expect the client to null-
				 * terminate the content properly. Therefore, we output the
				 * report line by line while keeping track of the dataspace
				 * size.
				 */

				/* pointer and length of remaining string */
				char const *str = _ds.local_addr<char>();
				size_t      len = length;

				while (*str && len) {

					size_t const line_len = _str_line_end(str, len);

					if (!line_len)
						break;

					/*
					 * Copy line from (untrusted) report dataspace to local
					 * line buffer,
					 */
					char line_buf[200];
					Genode::strncpy(line_buf, str, Genode::min(line_len, sizeof(line_buf)));
					PLOG("  %s", line_buf);

					str += line_len;
					len -= line_len;
				}

				PLOG("\n");
			}

			_module.write_content(_ds.local_addr<char>(), length);
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

			/* read label from session arguments */
			char label[200];
			Arg_string::find_arg(args, "label").string(label, sizeof(label), "");

			/* read report buffer size from session arguments */
			size_t const buffer_size =
				Arg_string::find_arg(args, "buffer_size").ulong_value(0);

			return new (md_alloc())
				Session_component(Rom::Module::Name(label), buffer_size,
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

#endif /* _REPORT_SERVICE_H_ */
