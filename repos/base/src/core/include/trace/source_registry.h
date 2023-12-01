/*
 * \brief  Registry containing possible sources of tracing data
 * \author Norman Feske
 * \date   2013-08-09
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__TRACE__SOURCE_REGISTRY_H_
#define _CORE__INCLUDE__TRACE__SOURCE_REGISTRY_H_

/* Genode includes */
#include <util/list.h>
#include <base/mutex.h>
#include <base/trace/types.h>
#include <base/weak_ptr.h>

/* base-internal include */
#include <base/internal/trace_control.h>

/* core-internal includes */
#include <types.h>

namespace Core { namespace Trace {

	using namespace Genode::Trace;

	using Filter = String<Session_label::capacity()>;

	class Source;
	class Source_owner;
	class Source_registry;

	/**
	 * Return singleton instance of trace-source registry
	 */
	Source_registry &sources();
} }


struct Core::Trace::Source_owner { };


/**
 * Source of tracing data
 *
 * There is one instance per thread.
 */
class Core::Trace::Source
:
	public Weak_object<Source>, public List<Source>::Element
{
	public:

		struct Info
		{
			Session_label      label;
			Thread_name        name;
			Execution_time     execution_time;
			Affinity::Location affinity;
		};

		/**
		 * Interface for querying trace-source information
		 */
		struct Info_accessor : Interface
		{
			virtual Info trace_source_info() const = 0;
		};

		struct Id { unsigned value; };

	private:

		Id            const  _unique_id;
		Info_accessor const &_info;
		Control             &_control;
		Dataspace_capability _policy { };
		Dataspace_capability _buffer { };
		Source_owner  const *_owner_ptr = nullptr;

		static Id _alloc_unique_id();

		/*
		 * Noncopyable
		 */
		Source(Source const &);
		Source &operator = (Source const &);

	public:

		Source(Info_accessor const &info, Control &control)
		:
			_unique_id(_alloc_unique_id()), _info(info), _control(control)
		{ }

		~Source()
		{
			/* invalidate weak pointers to this object */
			lock_for_destruction();
		}

		/*************************************
		 ** Interface used by TRACE service **
		 *************************************/

		Info const info() const { return _info.trace_source_info(); }

		void trace(Dataspace_capability policy, Dataspace_capability buffer)
		{
			_buffer = buffer;
			_policy = policy;
			_control.trace();
		}

		void enable()  { _control.enable(); }
		void disable() { _control.disable(); }

		bool try_acquire(Source_owner const &new_owner)
		{
			if (_owner_ptr && _owner_ptr != &new_owner)
				return false;

			_owner_ptr = &new_owner;
			return true;
		}

		bool owned_by(Source_owner const &owner) { return &owner == _owner_ptr; }

		void release_ownership(Source_owner const &owner)
		{
			if (owned_by(owner))
				_owner_ptr = nullptr;
		}

		bool error()   const { return _control.has_error(); }
		bool enabled() const { return _control.enabled(); }


		/***********************************
		 ** Interface used by CPU service **
		 ***********************************/

		Dataspace_capability buffer()    const { return _buffer; }
		Dataspace_capability policy()    const { return _policy; }
		Id                   id()        const { return _unique_id; }
};


/**
 * Registry to tracing sources
 *
 * There is a single instance within core.
 */
class Core::Trace::Source_registry
{
	private:

		Mutex        _mutex   { };
		List<Source> _entries { };

	public:

		/***********************************
		 ** Interface used by CPU service **
		 ***********************************/

		void insert(Source *entry)
		{
			Mutex::Guard guard(_mutex);

			_entries.insert(entry);
		}

		void remove(Source *entry)
		{
			Mutex::Guard guard(_mutex);
			_entries.remove(entry);
		}


		/*************************************
		 ** Interface used by TRACE service **
		 *************************************/

		void for_each_source(auto const &fn)
		{
			for (Source *s = _entries.first(); s; s = s->next())
				fn(*s);
		}
};

#endif /* _CORE__INCLUDE__TRACE__SOURCE_REGISTRY_H_ */
