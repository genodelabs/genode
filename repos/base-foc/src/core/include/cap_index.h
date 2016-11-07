/*
 * \brief  Core-specific capability index
 * \author Stefan Kalkowski
 * \date   2012-02-22
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__CAP_INDEX_H_
#define _CORE__INCLUDE__CAP_INDEX_H_

/* base-internal includes */
#include <base/internal/native_thread.h>
#include <base/internal/cap_map.h>

namespace Genode {

	class Platform_thread;
	class Pd_session_component;
	class Core_cap_index;
}


class Genode::Core_cap_index : public Native_capability::Data
{
	private:

		Pd_session_component  *_session;
		Platform_thread const *_pt;
		Native_thread          _gate;

	public:

		Core_cap_index(Pd_session_component *session = 0,
		               Platform_thread      *pt      = 0,
		               Native_thread         gate    = Native_thread() )
		: _session(session), _pt(pt), _gate(gate) {}

		Pd_session_component const *session() const { return _session; }
		Platform_thread      const *pt()      const { return _pt; }
		Native_thread        gate()           const { return _gate; }

		void session(Pd_session_component *c) { _session = c; }
		void pt(Platform_thread const *t)     { _pt = t;      }
};

#endif /* _CORE__INCLUDE__CAP_INDEX_H_ */
