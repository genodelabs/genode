/*
 * \brief  Failure state of jobs submitted via the 'installation'
 * \author Norman Feske
 * \date   2019-02-21
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _JOB_H_
#define _JOB_H_

/* Genode includes */
#include <util/list_model.h>
#include <base/allocator.h>

#include "types.h"

namespace Depot_download_manager {
	using namespace Depot;
	struct Job;
}


struct Depot_download_manager::Job : List_model<Job>::Element
{
	bool started = false;
	bool failed  = false;
	bool done    = false;

	Archive::Path const path;

	Job(Archive::Path const &path) : path(path) { }

	bool matches(Xml_node const &node) const
	{
		return node.attribute_value("path", Archive::Path()) == path;
	}

	static bool type_matches(Xml_node const &) { return true; }
};

#endif /* _JOB_H_ */
