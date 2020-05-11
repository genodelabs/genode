/*
 * \brief  Registry containing possible tracing subjects
 * \author Norman Feske
 * \date   2013-08-09
 *
 * Tracing subjects represent living or previously living tracing sources
 * that can have trace buffers attached. Each 'Trace::Subject' belongs to
 * a TRACE session and may point to a 'Trace::Source' (which is owned by
 * a CPU session).
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__TRACE__SUBJECT_REGISTRY_H_
#define _CORE__INCLUDE__TRACE__SUBJECT_REGISTRY_H_

/* Genode includes */
#include <util/list.h>
#include <util/string.h>
#include <base/mutex.h>
#include <base/trace/types.h>
#include <base/env.h>
#include <base/weak_ptr.h>
#include <dataspace/client.h>

/* core includes */
#include <trace/source_registry.h>

/* base-internal include */
#include <base/internal/trace_control.h>

namespace Genode { namespace Trace {
	class Subject;
	class Subject_registry;
} }


/**
 * Subject of tracing data
 */
class Genode::Trace::Subject
:
	public Genode::List<Genode::Trace::Subject>::Element,
	public Genode::Trace::Source_owner
{
	private:

		class Ram_dataspace
		{
			private:

				Ram_allocator           *_ram_ptr { nullptr };
				size_t                   _size    { 0 };
				Ram_dataspace_capability _ds      { };

				void _reset()
				{
					_ram_ptr = nullptr;
					_size    = 0;
					_ds      = Ram_dataspace_capability();
				}

				/*
				 * Noncopyable
				 */
				Ram_dataspace(Ram_dataspace const &);
				Ram_dataspace &operator = (Ram_dataspace const &);

			public:

				Ram_dataspace() { _reset(); }

				~Ram_dataspace() { flush(); }

				/**
				 * Allocate new dataspace
				 */
				void setup(Ram_allocator &ram, size_t size)
				{
					if (_size && _size == size)
						return;

					if (_size)
						_ram_ptr->free(_ds);

					_ds      = ram.alloc(size); /* may throw */
					_ram_ptr = &ram;
					_size    = size;
				}

				/**
				 * Clone dataspace into newly allocated dataspace
				 */
				bool setup(Ram_allocator &ram, Region_map &local_rm,
				           Dataspace_capability &from_ds, size_t size)
				{
					if (!from_ds.valid())
						return false;

					if (_size)
						flush();

					_ram_ptr = &ram;
					_size    = size;
					_ds      = ram.alloc(_size);

					/* copy content */
					void *src = local_rm.attach(from_ds),
					     *dst = local_rm.attach(_ds);

					memcpy(dst, src, _size);

					local_rm.detach(src);
					local_rm.detach(dst);

					return true;
				}

				/**
				 * Release dataspace
				 */
				size_t flush()
				{
					if (_ram_ptr)
						_ram_ptr->free(_ds);

					_reset();
					return 0;
				}

				Dataspace_capability dataspace() const { return _ds; }
		};

		friend class Subject_registry;

		Subject_id    const _id;
		unsigned      const _source_id;
		Weak_ptr<Source>    _source;
		Session_label const _label;
		Thread_name   const _name;
		Ram_dataspace       _buffer { };
		Ram_dataspace       _policy { };
		Policy_id           _policy_id { };
		size_t              _allocated_memory { 0 };

		Subject_info::State _state()
		{
			Locked_ptr<Source> source(_source);

			/* source vanished */
			if (!source.valid())
				return Subject_info::DEAD;

			if (source->enabled())
				return source->owned_by(*this) ? Subject_info::TRACED
				                               : Subject_info::FOREIGN;
			if (source->error())
				return Subject_info::ERROR;

			return Subject_info::UNTRACED;
		}

		void _traceable_or_throw()
		{
			switch(_state()) {
				case Subject_info::DEAD    : throw Source_is_dead();
				case Subject_info::FOREIGN : throw Traced_by_other_session();
				case Subject_info::ERROR   : throw Source_is_dead();
				case Subject_info::INVALID : throw Nonexistent_subject();
				case Subject_info::UNTRACED: return;
				case Subject_info::TRACED  : return;
			}
		}

	public:

		/**
		 * Constructor, called from 'Subject_registry' only
		 */
		Subject(Subject_id id, unsigned source_id, Weak_ptr<Source> &source,
		        Session_label const &label, Thread_name const &name)
		:
			_id(id), _source_id(source_id), _source(source),
			_label(label), _name(name)
		{ }

		/**
		 * Return registry-local ID
		 */
		Subject_id id() const { return _id; }

		/**
		 * Test if subject belongs to the specified unique source ID
		 */
		bool has_source_id(unsigned id) const { return id == _source_id; }

		size_t allocated_memory() const { return _allocated_memory; }
		void   reset_allocated_memory() { _allocated_memory = 0; }

		/**
		 * Start tracing
		 *
		 * \param size  trace buffer size
		 *
		 * \throw Out_of_ram
		 * \throw Out_of_caps
		 * \throw Already_traced
		 * \throw Source_is_dead
		 * \throw Traced_by_other_session
		 */
		void trace(Policy_id policy_id, Dataspace_capability policy_ds,
		           size_t policy_size, Ram_allocator &ram,
		           Region_map &local_rm, size_t size)
		{
			/* check state and throw error in case subject is not traceable */
			_traceable_or_throw();

			_buffer.setup(ram, size);
			if(!_policy.setup(ram, local_rm, policy_ds, policy_size))
				throw Already_traced();

			/* inform trace source about the new buffer */
			Locked_ptr<Source> source(_source);

			if (!source->try_acquire(*this))
				throw Traced_by_other_session();

			_policy_id = policy_id;

			_allocated_memory = policy_size + size;

			source->trace(_policy.dataspace(), _buffer.dataspace());
		}

		void pause()
		{
			/* inform trace source about the new buffer */
			Locked_ptr<Source> source(_source);

			if (source.valid())
				source->disable();
		}

		/**
		 * Resume tracing of paused source
		 *
		 * \throw Source_is_dead
		 */
		void resume()
		{
			/* inform trace source about the new buffer */
			Locked_ptr<Source> source(_source);

			if (!source.valid())
				throw Source_is_dead();

			source->enable();
		}

		Subject_info info()
		{
			Execution_time execution_time;
			Affinity::Location affinity;

			{
				Locked_ptr<Source> source(_source);

				if (source.valid()) {
					Trace::Source::Info const info = source->info();
					execution_time = info.execution_time;
					affinity       = info.affinity;
				}
			}

			return Subject_info(_label, _name, _state(), _policy_id,
			                    execution_time, affinity);
		}

		Dataspace_capability buffer() const { return _buffer.dataspace(); }

		size_t release()
		{
			/* inform trace source about the new buffer */
			Locked_ptr<Source> source(_source);

			/* source vanished */
			if (!source.valid())
				return 0;

			return _buffer.flush() + _policy.flush();
		}
};


/**
 * Registry to tracing subjects
 *
 * There exists one instance for each TRACE session.
 */
class Genode::Trace::Subject_registry
{
	private:

		typedef List<Subject> Subjects;

		Allocator       &_md_alloc;
		Ram_allocator   &_ram;
		Source_registry &_sources;
		unsigned         _id_cnt  { 0 };
		Mutex            _mutex   { };
		Subjects         _entries { };

		/**
		 * Functor for testing the existance of subjects for a given source
		 *
		 * This functor is invoked by 'Source_registry::export'.
		 */
		struct Tester
		{
			Subjects &subjects;

			Tester(Subjects &subjects) : subjects(subjects) { }

			bool operator () (unsigned source_id)
			{
				for (Subject *s = subjects.first(); s; s = s->next())
					if (s->has_source_id(source_id))
						return true;
				return false;
			}
		} _tester { _entries };

		/**
		 * Functor for inserting new subjects into the registry
		 *
		 * This functor is invoked by 'Source_registry::export'.
		 */
		struct Inserter
		{
			Subject_registry &registry;

			Inserter(Subject_registry &registry) : registry(registry) { }

			void operator () (unsigned source_id, Weak_ptr<Source> source,
			                  Session_label const &label, Thread_name const &name)
			{
				Subject *subject = new (&registry._md_alloc)
					Subject(Subject_id(++registry._id_cnt), source_id, source, label, name);

				registry._entries.insert(subject);
			}
		} _inserter { *this };

		/**
		 * Destroy subject, and release policy and trace buffers
		 *
		 * \return RAM resources released during destruction
		 */
		size_t _unsynchronized_destroy(Subject &s)
		{
			_entries.remove(&s);

			size_t const released_ram = s.release();

			destroy(&_md_alloc, &s);

			return released_ram;
		};

		/**
		 * Obtain subject from given session-local ID
		 *
		 * \throw Nonexistent_subject
		 */
		Subject &_unsynchronized_lookup_by_id(Subject_id id)
		{
			for (Subject *s = _entries.first(); s; s = s->next())
				if (s->id() == id)
					return *s;

			throw Nonexistent_subject();
		}

	public:

		/**
		 * Constructor
		 *
		 * \param md_alloc  meta-data allocator used for allocating 'Subject'
		 *                  objects.
		 * \param ram       allocator used for the allocation of trace
		 *                  buffers and policy dataspaces.
		 */
		Subject_registry(Allocator &md_alloc, Ram_allocator &ram,
		                 Source_registry &sources)
		:
			_md_alloc(md_alloc), _ram(ram), _sources(sources)
		{ }

		/**
		 * Destructor
		 */
		~Subject_registry()
		{
			Mutex::Guard guard(_mutex);

			while (Subject *s = _entries.first())
				_unsynchronized_destroy(*s);
		}

		/**
		 * \throw  Out_of_ram
		 */
		void import_new_sources(Source_registry &)
		{
			Mutex::Guard guard(_mutex);

			_sources.export_sources(_tester, _inserter);
		}

		/**
		 * Retrieve existing subject IDs
		 */
		size_t subjects(Subject_id *dst, size_t dst_len)
		{
			Mutex::Guard guard(_mutex);

			unsigned i = 0;
			for (Subject *s = _entries.first(); s && i < dst_len; s = s->next())
				dst[i++] = s->id();
			return i;
		}

		/**
		 * Retrieve Subject_infos batched
		 */
		size_t subjects(Subject_info * const dst, Subject_id * ids, size_t const len)
		{
			Mutex::Guard guard(_mutex);

			unsigned i = 0;
			for (Subject *s = _entries.first(); s && i < len; s = s->next()) {
				ids[i]   = s->id();
				dst[i++] = s->info();
			}

			return i;
		}

		/**
		 * Remove subject and release resources
		 *
		 * \return  RAM resources released as a side effect for removing the
		 *          subject (i.e., if the subject held a trace buffer or
		 *          policy dataspace). The value does not account for
		 *          memory allocated from the metadata allocator.
		 */
		size_t release(Subject_id subject_id)
		{
			Mutex::Guard guard(_mutex);

			Subject &subject = _unsynchronized_lookup_by_id(subject_id);
			return _unsynchronized_destroy(subject);
		}

		Subject &lookup_by_id(Subject_id id)
		{
			Mutex::Guard guard(_mutex);

			return _unsynchronized_lookup_by_id(id);
		}
};

#endif /* _CORE__INCLUDE__TRACE__SUBJECT_REGISTRY_H_ */
