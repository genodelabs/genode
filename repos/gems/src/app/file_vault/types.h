/*
 * \brief  Common types
 * \author Martin Stein
 * \date   2021-02-25
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TYPES_H_
#define _TYPES_H_

/* Genode includes */
#include <tresor/types.h>

namespace Genode { }

namespace File_vault {

	using namespace Tresor;
	using namespace Genode;

	using Node_name = String<32>;
	using File_path = String<32>;

	class Tree_geometry
	{
		private:

			uint64_t const _nr_of_levels;
			uint64_t const _nr_of_children;
			uint64_t const _nr_of_leaves;

		public:

			Tree_geometry(
				uint64_t nr_of_levels,
				uint64_t nr_of_children,
				uint64_t nr_of_leaves)
			:
				_nr_of_levels   { nr_of_levels   },
				_nr_of_children { nr_of_children },
				_nr_of_leaves   { nr_of_leaves   }
			{ }

			uint64_t nr_of_levels()   const { return _nr_of_levels  ; }
			uint64_t nr_of_children() const { return _nr_of_children; }
			uint64_t nr_of_leaves()   const { return _nr_of_leaves  ; }
	};
}

#endif /* _TYPES_H_ */
