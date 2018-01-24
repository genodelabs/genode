/*
 * \brief  Monitoring of a trace subject
 * \author Martin Stein
 * \date   2018-01-12
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MONITOR_H_
#define _MONITOR_H_

/* local includes */
#include <avl_tree.h>
#include <trace_buffer.h>

/* Genode includes */
#include <base/trace/types.h>

namespace Genode { namespace Trace { class Connection; } }


/**
 * To attach and detach trace-buffer dataspace in the right moments
 */
class Monitor_base
{
	protected:

		Genode::Trace::Connection &_trace;
		Genode::Region_map        &_rm;
		Genode::Trace::Buffer     &_buffer_raw;

		Monitor_base(Genode::Trace::Connection &trace,
		             Genode::Region_map        &rm,
		             Genode::Trace::Subject_id  subject_id);

		~Monitor_base();
};


/**
 * Monitors tracing information of one tracing subject
 */
class Monitor : public Monitor_base,
                public Local::Avl_node<Monitor>
{
	private:

		enum { MAX_ENTRY_LENGTH = 256 };

		Genode::Trace::Subject_id const  _subject_id;
		Trace_buffer                     _buffer;
		unsigned long                    _report_id        { 0 };
		Genode::Trace::Subject_info      _info             { };
		unsigned long long               _recent_exec_time { 0 };
		char                             _curr_entry_data[MAX_ENTRY_LENGTH];

		void _update_info();

	public:

		Monitor(Genode::Trace::Connection &trace,
		        Genode::Region_map        &rm,
		        Genode::Trace::Subject_id  subject_id);

		void print(bool activity, bool affinity);


		/**************
		 ** Avl_node **
		 **************/

		Monitor &find_by_subject_id(Genode::Trace::Subject_id const subject_id);

		bool higher(Monitor *monitor) { return monitor->_subject_id.id > _subject_id.id; }


		/***************
		 ** Accessors **
		 ***************/

		Genode::Trace::Subject_id          subject_id() const { return _subject_id; }
		Genode::Trace::Subject_info const &info()       const { return _info; }
};


/**
 * AVL tree of monitors with their subject ID as index
 */
struct Monitor_tree : Local::Avl_tree<Monitor>
{
	struct No_match : Genode::Exception { };

	Monitor &find_by_subject_id(Genode::Trace::Subject_id const subject_id);
};


#endif /* _MONITOR_H_ */
