/*
 * \brief  Linux-specific core implementation of the dataspace component
 * \author Stefan Kalkowski
 * \date   2015-09-25
 *
 * The Linux version of ROM session component does not use the
 * Rom_fs as provided as constructor argument. Instead, we map
 * rom modules directly to files of the host file system.
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Linux includes */
#include <core_linux_syscalls.h>
#include <sys/fcntl.h>

/* Genode includes */
#include <linux_dataspace/linux_dataspace.h>
#include <util/arg_string.h>
#include <root/root.h>
#include <base/session_label.h>

/* local includes */
#include "dataspace_component.h"

using namespace Genode;


Linux_dataspace::Filename Dataspace_component::_file_name(const char *args)
{
	Session_label const label = label_from_args(args);
	Linux_dataspace::Filename fname;

	if (label.last_element().length() > sizeof(fname.buf)) {
		Genode::error("file name too long: ", label.last_element());
		throw Service_denied();
	}

	strncpy(fname.buf, label.last_element().string(), sizeof(fname.buf));

	/* only files inside the current working directory are allowed */
	for (const char *c = fname.buf; *c; ++c)
		if (*c == '/') throw Service_denied();

	return fname;
}


Genode::size_t Dataspace_component::_file_size()
{
	struct stat64 s;
	if (lx_stat(_fname.buf, &s) < 0) throw Service_denied();

	return s.st_size;
}


Dataspace_component::Dataspace_component(const char *args)
: _fname(_file_name(args)),
  _size(_file_size()),
  _addr(0),
  _fd(lx_open(_fname.buf, O_RDONLY | LX_O_CLOEXEC, S_IRUSR | S_IXUSR)),
  _writable(false),
  _owner(0) { }
