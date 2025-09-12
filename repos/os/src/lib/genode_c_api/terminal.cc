/*
 * \brief  C interface to Genode's terminal session
 * \author Christian Helmuth
 * \date   2025-09-10
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <base/registry.h>
#include <base/session_label.h>
#include <terminal_session/connection.h>
#include <genode_c_api/terminal.h>

using namespace Genode;

struct Statics
{
	Env       *env_ptr;
	Allocator *alloc_ptr;

	Signal_context_capability             sigh { };
	Registry<Registered<genode_terminal>> sessions { };
};


static Statics & statics()
{
	static Statics instance { };

	return instance;
};


struct genode_terminal : private Noncopyable, private Interface
{
	private:

		Env       &_env;
		Allocator &_alloc;

		Session_label const _session_label;

		Terminal::Connection _connection { _env, _session_label.string() };

	public:

		genode_terminal(Env &env, Allocator &alloc,
		                Signal_context_capability const &sigh,
		                Session_label const &session_label)
		:
			_env(env), _alloc(alloc), _session_label(session_label)
		{
			_connection.read_avail_sigh(sigh);
		}

		template <typename FN>
		void with_read_bytes(FN const &fn)
		{
			_connection.with_read_bytes(fn);
		}

		unsigned long write(Span const &span)
		{
			return _connection.write(span.start, span.num_bytes);
		}
};


void genode_terminal_init(genode_env            *env_ptr,
                          genode_allocator      *alloc_ptr,
                          genode_signal_handler *sigh_ptr)
{
	statics().env_ptr   = env_ptr;
	statics().alloc_ptr = alloc_ptr;
	statics().sigh      = cap(sigh_ptr);
}


void genode_terminal_read(struct genode_terminal          *session,
                          genode_terminal_read_fn          read_fn,
                          struct genode_terminal_read_ctx *ctx)
{
	session->with_read_bytes([&] (Span const &span) {
		(*read_fn)(ctx, genode_const_buffer { span.start, span.num_bytes });
	});
}


unsigned long genode_terminal_write(struct genode_terminal     *session,
                                    struct genode_const_buffer  buffer)
{
	return session->write(Span { buffer.start, buffer.num_bytes });
}


struct genode_terminal *genode_terminal_create(struct genode_terminal_args const *args)
{
	if (!statics().env_ptr || !statics().alloc_ptr) {
		error("genode_terminal_create: missing call of genode_terminal_init");
		return nullptr;
	}

	return new (*statics().alloc_ptr)
		Registered<genode_terminal>(statics().sessions,
		                            *statics().env_ptr, *statics().alloc_ptr,
		                            statics().sigh, Session_label(args->label));
}


void genode_terminal_destroy(struct genode_terminal *terminal_ptr)
{
	destroy(*statics().alloc_ptr, terminal_ptr);
}
