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

		struct Mapped_dataspace : Noncopyable
		{
			using Local_rm = Local::Constrained_region_map;

			Dataspace_capability const _ds;

			Local_rm::Result mapped;

			Mapped_dataspace(Local_rm &rm, Dataspace_capability ds)
			:
				_ds(ds),
				mapped(rm.attach(ds, {
					.size       = { }, .offset    = { },
					.use_at     = { }, .at        = { },
					.executable = { }, .writeable = true
				}))
			{ }
		};

		struct Ram_dataspace : Noncopyable
		{
			using Setup_result = Attempt<Ok, Alloc_error>;
			using Attach_error = Region_map::Attach_error;

			Ram_allocator::Result ds { { } };

			Ram_dataspace() { }

			static Alloc_error alloc_error(Attach_error e)
			{
				switch (e) {
				case Attach_error::OUT_OF_RAM:  return Alloc_error::OUT_OF_RAM;
				case Attach_error::OUT_OF_CAPS: return Alloc_error::OUT_OF_CAPS;
				case Attach_error::REGION_CONFLICT:   break;
				case Attach_error::INVALID_DATASPACE: break;
				}
				return Alloc_error::DENIED;
			};

			Setup_result _copy_content(Local_rm &local_rm, size_t num_bytes,
			                           Dataspace_capability from_ds,
			                           Dataspace_capability to_ds)
			{
				Mapped_dataspace from { local_rm, from_ds },
				                 to   { local_rm, to_ds };

				return from.mapped.convert<Setup_result>(
					[&] (Local_rm::Attachment &from_range) {
						return to.mapped.convert<Setup_result>(
							[&] (Local_rm::Attachment &to_range) -> Setup_result {
								Genode::memcpy(to_range.ptr, from_range.ptr,
								               num_bytes);
								return Ok();
							},
							[&] (Attach_error e) -> Setup_result { return alloc_error(e); });
					},
					[&] (Attach_error e) -> Setup_result { return alloc_error(e); });
			}

			/**
			 * Allocate new dataspace
			 */
			[[nodiscard]] Setup_result setup(Ram_allocator &ram, size_t size)
			{
				ds = ram.try_alloc(size);
				return ds.convert<Setup_result>(
					[] (Ram::Allocation &) { return Ok(); },
					[] (Alloc_error e)     { return e; });
			}

			/**
			 * Clone dataspace into newly allocated dataspace
			 */
			[[nodiscard]] Setup_result setup(Ram_allocator        &ram,
			                                 Local_rm             &local_rm,
			                                 Dataspace_capability &from_ds,
			                                 size_t                size)
			{
				ds = ram.try_alloc(size);
				return ds.convert<Setup_result>(
					[&] (Ram::Allocation &allocation) {
						return _copy_content(local_rm, size, from_ds, allocation.cap);
					},
					[] (Alloc_error e) { return e; });
			}

			Ram::Capability dataspace() const
			{
				return ds.convert<Ram::Capability>(
					[&] (Ram::Allocation const &a) { return a.cap; },
					[&] (Ram::Error) { return Ram::Capability(); });
			}
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
		                   Local_rm &local_rm, Buffer_size size)
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

			using Setup_result = Ram_dataspace::Setup_result;

			auto trace_error = [] (Setup_result r)
			{
				return r.convert<Trace_result>(
					[] (Ok) /* never */ { return Trace_result::INVALID_SUBJECT; },
					[] (Alloc_error e) {
						switch (e) {
						case Alloc_error::OUT_OF_RAM:  return Trace_result::OUT_OF_RAM;
						case Alloc_error::OUT_OF_CAPS: return Trace_result::OUT_OF_CAPS;
						case Alloc_error::DENIED:      break;
						}
						return Trace_result::INVALID_SUBJECT;
					});
			};

			Setup_result buffer = _buffer.setup(ram, size.num_bytes);
			if (buffer.failed())
				return trace_error(buffer);

			Setup_result policy = _policy.setup(ram, local_rm, policy_ds, policy_size.num_bytes);
			if (policy.failed())
				return trace_error(policy);

			/* inform trace source about the new buffer */
			Locked_ptr<Source> source(_source);

			if (!source->try_acquire(*this)) {
				_policy.ds = { };
				_buffer.ds = { };
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

			_buffer.ds = { };
			_policy.ds = { };
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

		using Subject_alloc = Memory::Constrained_obj_allocator<Subject>;

		Subject_alloc _subject_alloc { _md_alloc };

		/**
		 * Destroy subject, and release policy and trace buffers
		 */
		void _unsynchronized_destroy(Subject &s)
		{
			_entries.remove(&s);
			s.release();
			_subject_alloc.destroy(s);
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

		using Import_result = Attempt<Ok, Alloc_error>;

		Import_result import_new_sources(Source_registry &)
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

			Import_result result = Ok();

			_sources.for_each_source([&] (Source &source) {
				if (result.failed())
					return;

				Source::Info const info = source.info();

				if (!filter_matches(info.label) || already_known(source.id()))
					return;

				Weak_ptr<Source> source_ptr = source.weak_ptr();

				_subject_alloc.create(Subject_id(_id_cnt + 1), source.id(),
				                      source_ptr, info.label, info.name).with_result(

					[&] (Subject_alloc::Allocation &a) {
						_entries.insert(&a.obj);
						_id_cnt++;
						a.deallocate = false;
					},
					[&] (Alloc_error e) { result = e; }
				);
			});

			return result;
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
