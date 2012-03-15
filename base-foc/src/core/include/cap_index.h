/*
 * \brief  Core-specific capability index.
 * \author Stefan Kalkowski
 * \date   2012-02-22
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__CAP_INDEX_H_
#define _CORE__INCLUDE__CAP_INDEX_H_

/* Genode includes */
#include <base/cap_map.h>

/* Core includes */
#include <cap_session_component.h>

namespace Genode {

	class Platform_thread;
	class Core_cap_index : public Cap_index
	{
		private:

			Cap_session_component *_session;
			Platform_thread       *_pt;
			Native_thread          _gate;

		public:

			Core_cap_index(Cap_session_component *session = 0,
			               Platform_thread       *pt      = 0,
			               Native_thread          gate    = Native_thread() )
			: _session(session), _pt(pt), _gate(gate) {}

			Cap_session_component *session() { return _session; }
			Platform_thread       *pt()      { return _pt;      }
			Native_thread          gate()    { return _gate;    }

			void session(Cap_session_component* c) { _session = c; }
			void pt(Platform_thread* t)            { _pt = t;      }
	};
}

#endif /* _CORE__INCLUDE__CAP_INDEX_H_ */
