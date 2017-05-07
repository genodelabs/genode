/*
 * \brief  Session
 * \author Norman Feske
 * \date   2011-05-15
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SESSION__SESSION_H_
#define _INCLUDE__SESSION__SESSION_H_

#include <base/quota_guard.h>
#include <base/session_label.h>
#include <util/arg_string.h>

/*
 * Each session interface declares an RPC interface and, therefore, relies on
 * the RPC framework. By including 'base/rpc.h' here, we relieve the interfaces
 * from including 'base/rpc.h' in addition to 'session/session.h'.
 */
#include <base/rpc.h>

namespace Genode {

	struct Session;

	/*
	 * Exceptions that may occur during the session creation
	 */
	struct Insufficient_ram_quota : Exception { };
	struct Insufficient_cap_quota : Exception { };
	struct Service_denied         : Exception { };
}


/**
 * Base class of session interfaces
 */
struct Genode::Session
{
	struct Resources
	{
		Ram_quota ram_quota;
		Cap_quota cap_quota;
	};

	struct Diag { bool enabled; };

	typedef Session_label Label;

	/*
	 * Each session interface must implement the class function 'service_name'
	 * ! static const char *service_name();
	 * This function returns the name of the service provided via the session
	 * interface.
	 */

	virtual ~Session() { }
};


namespace Genode {

	static inline Ram_quota ram_quota_from_args(char const *args)
	{
		return { Arg_string::find_arg(args, "ram_quota").ulong_value(0) };
	}

	static inline Cap_quota cap_quota_from_args(char const *args)
	{
		return { Arg_string::find_arg(args, "cap_quota").ulong_value(0) };
	}

	static inline Session::Label session_label_from_args(char const *args)
	{
		return label_from_args(args);
	}

	static inline Session::Resources session_resources_from_args(char const *args)
	{
		return { ram_quota_from_args(args), cap_quota_from_args(args) };
	}

	static inline Session::Diag session_diag_from_args(char const *args)
	{
		return { Arg_string::find_arg(args, "diag").bool_value(false) };
	}
}

#endif /* _INCLUDE__SESSION__SESSION_H_ */
