/*
 * \brief  Trace probes
 * \author Johannes Schlatow
 * \date   2021-12-01
 *
 * Convenience macros for creating user-defined trace checkpoints.
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__TRACE__PROBE_H_
#define _INCLUDE__TRACE__PROBE_H_

#include <base/trace/events.h>

namespace Genode { namespace Trace {

	class Duration
	{
		private:

			char          const *_name;
			unsigned long const  _data;

			Duration(Duration const &) = delete;

			Duration & operator = (Duration const &) = delete;

		public:

			Duration(char const * name, unsigned long data)
			: _name(name), _data(data)
			{ Checkpoint(_name, _data, nullptr, Checkpoint::Type::START); }

			~Duration()
			{ Checkpoint(_name, _data, nullptr, Checkpoint::Type::END); }
	};

} }


/**
 * Trace a single checkpoint named after the current function.
 *
 * The argument 'data' specifies the payload as an unsigned value.
 */
#define GENODE_TRACE_CHECKPOINT(data) \
	Genode::Trace::Checkpoint(__PRETTY_FUNCTION__, (unsigned long)data, nullptr);


/**
 * Variant of 'GENODE_TRACE_CHECKPOINT' that accepts the name of the checkpoint as argument.
 *
 * The argument 'data' specifies the payload as an unsigned value.
 * The argument 'name' specifies the name of the checkpoint.
 */
#define GENODE_TRACE_CHECKPOINT_NAMED(data, name) \
	Genode::Trace::Checkpoint(name, (unsigned long)data, nullptr);


/**
 * Trace a pair of checkpoints when entering and leaving the current scope.
 *
 * The argument 'data' specifies the payload as an unsigned value.
 */
#define GENODE_TRACE_DURATION(data) \
	Genode::Trace::Duration duration(__PRETTY_FUNCTION__, (unsigned long)data);


/**
 * Variant of 'GENODE_TRACE_DURATION' that accepts the name of the checkpoints as argument.
 *
 * The argument 'data' specifies the payload as an unsigned value.
 * The argument 'name' specifies the names of the checkpoints
 */
#define GENODE_TRACE_DURATION_NAMED(data, name) \
	Genode::Trace::Duration duration(name, (unsigned long)data);


#endif /* _INCLUDE__TRACE__PROBE_H_ */
