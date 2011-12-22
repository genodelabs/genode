/*
 * \brief  Interface to 'PWD' environment variable
 * \author Norman Feske
 * \date   2011-02-17
 */

/*
 * Copyright (C) 2011 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__PWD_H_
#define _NOUX__PWD_H_

/* Genode includes */
#include <util/string.h>

/* Noux includes */
#include <noux_session/sysio.h>
#include <path.h>

namespace Noux {

	struct Pwd
	{
		typedef Noux::Path<Sysio::MAX_PATH_LEN> Path;

		/**
		 * Lookup process work directory from environment
		 */
		virtual char const *pwd() = 0;

		/**
		 * Set current work directory
		 */
		virtual void pwd(char const *pwd) = 0;
	};
}

#endif /* _NOUX__PWD_H_ */
