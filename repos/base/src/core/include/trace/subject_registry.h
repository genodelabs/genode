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
#include <base/mutex.h>
#include <base/trace/types.h>
#include <base/env.h>
#include <base/weak_ptr.h>
#include <dataspace/client.h>

/* base-internal include */
#include <base/internal/trace_control.h>

/* core includes */
#include <types.h>
#include <trace/source_registry.h>

namespace Core { namespace Trace {

	using namespace Genode::Trace;

	class Subject;
	class Subject_registry;
} }


/**
 * Subject of tracing data
 */
class Core::Trace::Subject
:
	public List<Subject>::Element,
	public Source_owner
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
				void setup(Ram_allocator &ram, Region_map &local_rm,
				           Dataspace_capability &from_ds, size_t size)
				{
					if (_size)
						flush();

					_ds      = ram.alloc(size); /* may throw */
					_ram_ptr = &ram;
					_size    = size;

					/* copy content */
					Attached_dataspace from { local_rm, from_ds },
					                   to   { local_rm, _ds };

					Genode::memcpy(to.local_addr<char>(), from.local_addr<char const>(), _size);
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
		Source::Id    const _source_id;
		Weak_ptr<Source>    _source;
		Session_label const _label;
		Thread_name   const _name;
		Ram_dataspace       _buffer { };
		Ram_dataspace       _policy { };
		Policy_id           _policy_id { };

		Subject_info::State _state()
		{
			Locked_ptr<Source> source(_source);

			/* source vanished */
			if (!source.valid())
				return Subject_info::DEAD;

			if (source->error())
				return Subject_info::ERROR;

			if (source->enabled() && !source->owned_by(*this))
				return Subject_info::FOREIGN;

			if (source->owned_by(*this))
				return source->enabled() ? Subject_info::TRACED
				                         : Subject_info::ATTACHED;

			return Subject_info::UNATTACHED;
		}

	public:

		/**
		 * Constructor, called from 'Subject_registry' only
		 */
		Subject(Subject_id id, Source::Id source_id, Weak_ptr<Source> &source,
		        Session_label const &label, Thread_name const &name)
		:
			_id(id), _source_id(source_id), _source(source),
			_label(label), _name(name)
		{ }

		/**
		 * Destructor, releases ownership of associated source
		 */
		~Subject() { release(); }

		/**
		 * Return registry-local ID
		 */
		Subject_id id() const { return _id; }

		/**
		 * Test if subject belongs to the specified unique source ID
		 */
		bool has_source_id(Source::Id id) const { return id.value == _source_id.value; }

		enum class Trace_result { OK, OUT_OF_RAM, OUT_OF_CAPS, FOREIGN,
		                          SOURCE_IS_DEAD, INVALID_SUBJECT };

		/**
		 * Start tracing
		 *
		 * \param size  trace buffer size
		 */
		Trace_result trace(Policy_id policy_id, Dataspace_capability policy_ds,
		                   Policy_size policy_size, Ram_allocator &ram,
		                   Region_map &local_rm, Buffer_size size)
		{
			/* check state and return error if subject is not traceable */
			switch(_state()) {
			case Subject_info::DEAD:      return Trace_result::SOURCE_IS_DEAD;
			case Subject_info::ERROR:     return Trace_result::SOURCE_IS_DEAD;
			case Subject_info::FOREIGN:   return Trace_result::FOREIGN;
			case Subject_info::INVALID:   return Trace_result::INVALID_SUBJECT;
			case Subject_info::UNATTACHED:
			case Subject_info::ATTACHED:
			case Subject_info::TRACED:    break;
			}

			try {
				_buffer.setup(ram, size.num_bytes);
			}
			catch (Out_of_ram)  { return Trace_result::OUT_OF_RAM;  }
			catch (Out_of_caps) { return Trace_result::OUT_OF_CAPS; }

			try {
				_policy.setup(ram, local_rm, policy_ds, policy_size.num_bytes);
			}
			catch (Out_of_ram)  { _buffer.flush(); return Trace_result::OUT_OF_RAM;  }
			catch (Out_of_caps) { _buffer.flush(); return Trace_result::OUT_OF_CAPS; }

			/* inform trace source about the new buffer */
			Locked_ptr<Source> source(_source);

			if (!source->try_acquire(*this)) {
				_policy.flush();
				_buffer.flush();
				return Trace_result::FOREIGN;
			}

			_policy_id = policy_id;

			source->trace(_policy.dataspace(), _buffer.dataspace());
			return Trace_result::OK;
		}

		void pause()
		{
			Locked_ptr<Source> source(_source);

			if (source.valid())
				source->disable();
		}

		/**
		 * Resume tracing of paused source
		 */
		void resume()
		{
			Locked_ptr<Source> source(_source);

			if (source.valid())
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

		void release()
		{
			Locked_ptr<Source> source(_source);

			/* source vanished */
			if (!source.valid())
				return;

			source->disable();
			source->release_ownership(*this);

			_buffer.flush();
			_policy.flush();
		}
};


/**
 * Registry to tracing subjects
 *
 * There exists one instance for each TRACE session.
 */
class Core::Trace::Subject_registry
{
	private:

		using Subjects = List<Subject>;

		Allocator       &_md_alloc;
		Source_registry &_sources;
		Filter     const _filter;
		unsigned         _id_cnt  { 0 };
		Mutex            _mutex   { };
		Subjects         _entries { };

		/**
		 * Destroy subject, and release policy and trace buffers
		 */
		void _unsynchronized_destroy(Subject &s)
		{
			_entries.remove(&s);
			s.release();
			destroy(&_md_alloc, &s);
		};

		/**
		 * Obtain subject from given session-local ID
		 */
		void _with_subject_unsynchronized(Subject_id id, auto const &fn)
		{
			Subject *ptr = _entries.first();
			for (; ptr && (ptr->id().id != id.id); ptr = ptr->next());
			if (ptr)
				fn(*ptr);
		}

	public:

		Subject_registry(Allocator &md_alloc, Source_registry &sources, Filter const &filter)
		:
			_md_alloc(md_alloc), _sources(sources), _filter(filter)
		{ }

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

			auto already_known = [&] (Source::Id const unique_id)
			{
				for (Subject *s = _entries.first(); s; s = s->next())
					if (s->has_source_id(unique_id))
						return true;
				return false;
			};

			auto filter_matches = [&] (Session_label const &label)
			{
				return strcmp(_filter.string(), label.string(), _filter.length() - 1) == 0;
			};

			_sources.for_each_source([&] (Source &source) {

				Source::Info const info = source.info();

				if (!filter_matches(info.label) || already_known(source.id()))
					return;

				Weak_ptr<Source> source_ptr = source.weak_ptr();

				_entries.insert(new (_md_alloc)
					Subject(Subject_id(++_id_cnt), source.id(),
					        source_ptr, info.label, info.name));
			});
		}

		/**
		 * Retrieve existing subject IDs
		 */
		unsigned subjects(Subject_id *dst, size_t dst_len)
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
		unsigned subjects(Subject_info * const dst, Subject_id * ids, size_t const len)
		{
			Mutex::Guard guard(_mutex);

			auto filtered = [&] (Session_label const &label) -> Session_label
			{
				return (label.length() <= _filter.length() || !_filter.length())
				        ? Session_label("") /* this cannot happen */
				        : Session_label(label.string() + _filter.length() - 1);
			};

			unsigned i = 0;
			for (Subject *s = _entries.first(); s && i < len; s = s->next()) {
				ids[i] = s->id();

				Subject_info const info = s->info();

				/* strip filter prefix from reported trace-subject label */
				dst[i++] = {
					filtered(info.session_label()), info.thread_name(), info.state(),
					info.policy_id(), info.execution_time(), info.affinity()
				};
			}
			return i;
		}

		/**
		 * Remove subject and release resources
		 */
		void release(Subject_id subject_id)
		{
			Mutex::Guard guard(_mutex);

			_with_subject_unsynchronized(subject_id, [&] (Subject &subject) {
				_unsynchronized_destroy(subject); });
		}

		void with_subject(Subject_id id, auto const &fn)
		{
			Mutex::Guard guard(_mutex);

			return _with_subject_unsynchronized(id, fn);
		}
};

#endif /* _CORE__INCLUDE__TRACE__SUBJECT_REGISTRY_H_ */
