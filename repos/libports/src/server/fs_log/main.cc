/*
 * \brief  LOG service that writes to a file
 * \author Alexander Tarasikov <tarasikov@ksyslabs.org>
 * \author Norman Feske <norman.feske@genode-labs.com>
 * \date   2013-05-07
 */

/*
 * Copyright (C) 2013 Ksys Labs LLC
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <base/rpc_server.h>
#include <base/sleep.h>
#include <root/component.h>
#include <util/string.h>
#include <os/session_policy.h>
#include <cap_session/connection.h>
#include <log_session/log_session.h>

/* libc includes */
#include <fcntl.h>
#include <unistd.h>


class Log_component : public Genode::Rpc_object<Genode::Log_session>
{
	public:

		enum { LABEL_LEN = 64 };

	private:

		typedef Genode::size_t size_t;

		char   _label[LABEL_LEN];
		size_t _label_len;
		int    _fd;

	public:

		/**
		 * Constructor
		 */
		Log_component(const char *label, char const *filename)
		:
			_fd(::open(filename, O_CREAT | O_WRONLY | O_APPEND))
		{
			using namespace Genode;

			if (_fd < 0) {
				PERR("unable to open \"%s\"", filename);
				throw Root::Unavailable();
			}

			snprintf(_label, LABEL_LEN, "[%s] ", label);
			_label_len = strlen(_label);

			PINF("log client \"%s\" to file \"%s\")", label, filename);
		}

		/**
		 * Destructor
		 */
		~Log_component() { ::close(_fd); }


		/*****************
		 ** Log session **
		 *****************/

		/**
		 * Write a log-message to the file.
		 */
		size_t write(String const &string_buf)
		{
			if (!(string_buf.is_valid_string())) {
				PERR("corrupted string");
				return 0;
			}

			char const *string       = string_buf.string();
			Genode::size_t const len = Genode::strlen(string);

			::write(_fd, _label, _label_len);
			::write(_fd, string, len);

			return len;
		}
};


class Log_root : public Genode::Root_component<Log_component>
{
	private:

		int _fd;

	protected:

		/**
		 * Root component interface
		 */
		Log_component *_create_session(const char *args)
		{
			using namespace Genode;

			char label_buf[Log_component::LABEL_LEN];
			Arg label_arg = Arg_string::find_arg(args, "label");
			label_arg.string(label_buf, sizeof(label_buf), "");

			/* obtain file name from configured policy */
			enum { FILENAME_MAX_LEN = 256 };
			char filename[FILENAME_MAX_LEN];
			try {
				Session_label  label(args);
				Session_policy policy(label);
				policy.attribute("file").value(filename, sizeof(filename));
			} catch (...) {
				PERR("Invalid session request, no matching policy");
				throw Root::Unavailable();
			}

			return new (md_alloc()) Log_component(label_buf, filename);
		}

	public:

		/**
		 * Constructor
		 *
		 * \param session_ep  entry point for managing cpu session objects
		 * \param md_alloc    meta-data allocator to be used by root component
		 */
		Log_root(Genode::Rpc_entrypoint *session_ep, Genode::Allocator *md_alloc)
		: Genode::Root_component<Log_component>(session_ep, md_alloc) { }
};


int main(int argc, char **argv)
{
	using namespace Genode;

	/*
	 * Initialize server entry point
	 *
	 * Use a large stack because we are calling libc functions from the
	 * context of the entrypoint.
	 */
	enum { STACK_SIZE = sizeof(addr_t)*16*1024 };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "fs_log_ep");

	static Log_root log_root(&ep, env()->heap());

	/*
	 * Announce services
	 */
	env()->parent()->announce(ep.manage(&log_root));

	/**
	 * Got to sleep forever
	 */
	sleep_forever();
	return 0;
}
