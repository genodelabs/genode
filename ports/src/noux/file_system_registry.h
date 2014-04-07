/*
 * \brief  Registry of file systems
 * \author Norman Feske
 * \date   2014-04-07
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__FILE_SYSTEM_REGISTRY_H_
#define _NOUX__FILE_SYSTEM_REGISTRY_H_

/* Noux includes */
#include <file_system.h>

namespace Noux {

	class File_system_registry
	{
		public:

			struct Entry : List<Entry>::Element
			{
				virtual File_system *create(Xml_node node) = 0;

				virtual bool matches(Xml_node node) = 0;
			};

		private:

			List<Entry> _list;

		public:

			void insert(Entry &entry)
			{
				_list.insert(&entry);
			}

			Entry *lookup(Xml_node node)
			{
				for (Entry *e = _list.first(); e; e = e->next())
					if (e->matches(node))
						return e;

				return 0;
			}
	};
}

#endif /* _NOUX__FILE_SYSTEM_REGISTRY_H_ */
