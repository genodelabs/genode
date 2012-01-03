/*
 * \brief  Capability allocation service
 * \author Stefan Kalkowski
 * \date   2011-01-13
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__CAP_SESSION_COMPONENT_H_
#define _CORE__INCLUDE__CAP_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/allocator_avl.h>
#include <base/exception.h>
#include <base/lock.h>
#include <base/sync_allocator.h>
#include <base/rpc_server.h>

namespace Genode {

	class Cap_session_component : public Rpc_object<Cap_session>
	{
		private:

			static long _unique_id_cnt; /* TODO: remove this from generic core code */

		public:

			Native_capability alloc(Native_capability ep);

			void free(Native_capability cap);

			static Native_capability alloc(Cap_session_component *session,
			                               Native_capability ep);
	};


	class Badge_allocator
	{
		private:

			enum {
				BADGE_RANGE   = ~0UL,
				BADGE_MASK    = ~3UL,
				BADGE_NUM_MAX = BADGE_MASK >> 2,
				BADGE_OFFSET  = 1 << 2
			};

			Synchronized_range_allocator<Allocator_avl> _id_alloc;
			Lock                                        _lock;

			Badge_allocator();

		public:

			class Out_of_badges : Exception {};

			unsigned long alloc();
			void free(unsigned long badge);

			static Badge_allocator* allocator();
	};


	class Platform_thread;
	class Capability_node : public Avl_node<Capability_node>
	{
		private:

			unsigned long          _badge;
			Cap_session_component *_cap_session;
			Platform_thread       *_pt;
			Native_thread          _gate;

		public:

			Capability_node(unsigned long          badge,
			                Cap_session_component *cap_session,
			                Platform_thread       *pt,
			                Native_thread          gate);

			bool higher(Capability_node *n);

			Capability_node *find_by_badge(unsigned long badge);

			unsigned long          badge()       { return _badge;       }
			Cap_session_component *cap_session() { return _cap_session; }
			Platform_thread       *pt()          { return _pt;          }
			Native_thread          gate()        { return _gate;        }
	};


	class Capability_tree : public Avl_tree<Capability_node>
	{
		private:

			Lock _lock;

			Capability_tree() {}

		public:

			void insert(Avl_node<Capability_node> *node);

			void remove(Avl_node<Capability_node> *node);

			Capability_node *find_by_badge(unsigned long badge);

			static Capability_tree* tree();
	};
}

#endif /* _CORE__INCLUDE__CAP_SESSION_COMPONENT_H_ */
