/*
 * \brief  Core implementation of the PD session extension
 * \author Stefan Kalkowski
 * \date   2011-04-14
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/native_types.h>

/* Core includes */
#include <pd_session_component.h>


Genode::Native_capability Genode::Pd_session_component::task_cap() {
	return Native_capability(_pd.native_task()); }
