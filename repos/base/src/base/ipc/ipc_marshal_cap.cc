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
void Ipc_marshaller::insert(Native_capability const &cap)
{
	insert((char const *)&cap, sizeof(Native_capability));
}


/**
 * Unmarshalling of capabilities as plain data representation
 */
void Ipc_unmarshaller::extract(Native_capability &cap)
{
	struct Raw { char buf[sizeof(Native_capability)]; } raw;
	extract(raw);
	(Raw &)(cap) = raw;
}
