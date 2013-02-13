/*
 * \brief  Marshalling/unmarshalling of plain-data capabilities
 * \author Norman Feske
 * \date   2013-02-13
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/ipc.h>

using namespace Genode;


/**
 * Marshalling of capabilities as plain data representation
 */
void Ipc_ostream::_marshal_capability(Native_capability const &cap)
{
	_write_to_buf(cap);
}


/**
 * Unmarshalling of capabilities as plain data representation
 */
void Ipc_istream::_unmarshal_capability(Native_capability &cap)
{
	_read_from_buf(cap);
}
