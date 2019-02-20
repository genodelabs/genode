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

	Archive::Path const path;

	Job(Archive::Path const &path) : path(path) { }

	struct Update_policy
	{
		typedef Job Element;

		Allocator &_alloc;

		Update_policy(Allocator &alloc) : _alloc(alloc) { }

		void destroy_element(Job &elem) { destroy(_alloc, &elem); }

		Job &create_element(Xml_node elem_node)
		{
			return *new (_alloc)
				Job(elem_node.attribute_value("path", Archive::Path()));
		}

		void update_element(Job &, Xml_node) { }

		static bool element_matches_xml_node(Job const &job, Xml_node node)
		{
			return node.attribute_value("path", Archive::Path()) == job.path;
		}

		static bool node_is_element(Xml_node) { return true; }
	};
};

#endif /* _JOB_H_ */
