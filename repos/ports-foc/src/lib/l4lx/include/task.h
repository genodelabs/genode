/*
 * \brief  Tasks for l4lx support library.
 * \author Stefan Kalkowski
 * \date   2011-04-27
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _L4LX__TASK_H_
#define _L4LX__TASK_H_

/* Genode includes */
#include <pd_session/connection.h>
#include <foc_native_pd/client.h>
#include <util/avl_tree.h>

namespace Fiasco {
#include <l4/sys/types.h>
#include <l4/sys/consts.h>
#include <l4/sys/task.h>
}

namespace L4lx {


	class Task : public Genode::Avl_node<Task>
	{
		private:

			Fiasco::l4_cap_idx_t         _ref;
			Genode::Pd_connection        _pd;
			Genode::Foc_native_pd_client _native_pd { _pd.native_pd() };
			Genode::Native_capability    _cap;

		public:

			/*****************
			 ** Constructor **
			 *****************/

			Task(Fiasco::l4_cap_idx_t ref)
			:
				_ref(ref), _cap(_native_pd.task_cap())
			{
				using namespace Fiasco;

				/* remap task cap to given cap slot */
				l4_task_map(L4_BASE_TASK_CAP, L4_BASE_TASK_CAP,
							l4_obj_fpage(Genode::Capability_space::kcap(_cap),
							             0, L4_FPAGE_RWX),
							_ref | L4_ITEM_MAP);
			}


			/***************
			 ** Accessors **
			 ***************/

			Fiasco::l4_cap_idx_t ref() { return _ref;  }


			/************************
			 ** Avl_node interface **
			 ************************/

			bool higher(Task *n) { return n->_ref > _ref; }

			Task *find_by_ref(Fiasco::l4_cap_idx_t ref)
			{
				if (_ref == ref) return this;

				Task *n = Genode::Avl_node<Task>::child(ref > _ref);
				return n ? n->find_by_ref(ref) : 0;
			}
	};


	class Task_tree : public Genode::Avl_tree<Task>
	{
		public:

			Task* find_by_ref(Fiasco::l4_cap_idx_t ref) {
				return this->first() ? this->first()->find_by_ref(ref) : 0; }
	};
}

#endif /* _L4LX__TASK_H_ */
