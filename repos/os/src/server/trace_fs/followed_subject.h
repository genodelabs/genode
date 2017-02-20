/*
 * \brief  Facility for managing the trace subjects
 * \author Josef Soentgen
 * \date   2014-01-22
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SUBJECT_REGISTRY_H_
#define _SUBJECT_REGISTRY_H_

#include <base/allocator.h>
#include <base/lock.h>
#include <base/trace/types.h>

#include <directory.h>
#include <trace_files.h>

namespace Trace_fs {

	typedef Genode::size_t size_t;

	struct Followed_subject : public File_system::Directory
	{
		public:

			/**
			 * This class manages the access to the trace subject's trace buffer
			 */
			class Trace_buffer_manager
			{
				public:

					class Already_managed { };
					class Not_managed     { };

					struct Process_entry
					{
						virtual size_t operator()(Genode::Trace::Buffer::Entry&) = 0;
					};

				private:

					Genode::Trace::Buffer       *buffer;
					Genode::Trace::Buffer::Entry current_entry;


				public:

				Trace_buffer_manager(Genode::Region_map           &rm,
				                     Genode::Dataspace_capability  ds_cap)
				:
					buffer(rm.attach(ds_cap)),
					current_entry(buffer->first())
				{ }

				size_t dump_entry(Process_entry &process)
				{
					size_t len = process(current_entry);

					current_entry = buffer->next(current_entry);
					return len;
				}

				bool last_entry() const
				{
					return current_entry.last();
				}

				void rewind() { current_entry = buffer->first(); }
			};


		private:

			Genode::Allocator         &_md_alloc;

			Genode::Region_map        &_rm;

			int                        _handle;

			Genode::Trace::Subject_id  _id;
			Genode::Trace::Policy_id   _policy_id;

			bool                       _was_traced;

			Trace_buffer_manager      *_buffer_manager;

		public:

			File_system::Active_file      active_file;
			File_system::Buffer_size_file buffer_size_file;
			File_system::Cleanup_file     cleanup_file;
			File_system::Enable_file      enable_file;
			File_system::Events_file      events_file;
			File_system::Policy_file      policy_file;

			Followed_subject(Genode::Allocator &md_alloc, char const *name,
			                 Genode::Region_map &rm,
			                 Genode::Trace::Subject_id &id, int handle)
			:
				Directory(name),
				_md_alloc(md_alloc),
				_rm(rm),
				_handle(handle),
				_id(id),
				_was_traced(false),
				_buffer_manager(0),
				active_file(_id),
				buffer_size_file(),
				cleanup_file(_id),
				enable_file(_id),
				events_file(_id, _md_alloc),
				policy_file(_id, _md_alloc)
			{
				adopt_unsynchronized(&active_file);
				adopt_unsynchronized(&cleanup_file);
				adopt_unsynchronized(&enable_file);
				adopt_unsynchronized(&events_file);
				adopt_unsynchronized(&buffer_size_file);
				adopt_unsynchronized(&policy_file);
			}

			~Followed_subject()
			{
				discard_unsynchronized(&active_file);
				discard_unsynchronized(&cleanup_file);
				discard_unsynchronized(&enable_file);
				discard_unsynchronized(&events_file);
				discard_unsynchronized(&buffer_size_file);
				discard_unsynchronized(&policy_file);
			}

			bool marked_for_cleanup() const { return cleanup_file.cleanup(); }
			bool was_traced() const { return _was_traced; }

			Trace_buffer_manager* trace_buffer_manager() { return _buffer_manager; }

			void manage_trace_buffer(Genode::Dataspace_capability ds_cap)
			{
				if (_buffer_manager != 0)
					throw Trace_buffer_manager::Already_managed();

				_buffer_manager = new (&_md_alloc) Trace_buffer_manager(_rm, ds_cap);
			}

			void unmanage_trace_buffer()
			{
				if (_buffer_manager == 0)
					throw Trace_buffer_manager::Not_managed();

				destroy(&_md_alloc, _buffer_manager);
				_buffer_manager = 0;
			}

			const Genode::Trace::Subject_id id() const { return _id; }

			const Genode::Trace::Policy_id policy_id() const { return _policy_id; }
			void policy_id(Genode::Trace::Policy_id &id) { _policy_id.id = id.id; }
			bool policy_valid() const { return (_policy_id.id != 0); }
			void invalidate_policy() { _policy_id = Genode::Trace::Policy_id(); }

			int handle() const { return _handle; }
	};


	/**
	 * This registry contains all current followed trace subjects
	 */
	class Followed_subject_registry
	{
		public:

			class Invalid_subject { };
			class Out_of_subject_handles { };

		private:

			/* XXX abitrary limit - needs to be revisited when highly
			 * dynamic scenarios are executed */
			enum { MAX_SUBJECTS = 1024U };

			Followed_subject *_subjects[MAX_SUBJECTS];

			Genode::Allocator &_md_alloc;

			/**
			 * Find free subject handle
			 *
			 * \throw Out_of_subject_handles
			 */
			int _find_free_handle()
			{
				for (unsigned i = 0; i < MAX_SUBJECTS; i++)
					if (!_subjects[i]) {
						return i;
					}

				throw Out_of_subject_handles();
			}

			bool _in_range(int handle) const
			{
				return ((handle >= 0) && (handle < MAX_SUBJECTS));
			}

		public:

			Followed_subject_registry(Genode::Allocator &md_alloc)
			:
				_md_alloc(md_alloc)
			{
				for (unsigned i = 0; i < MAX_SUBJECTS; i++)
					_subjects[i] = 0;
			}

			/**
			 * Return maximal number of subject that can be stored in the registry
			 *
			 * \return maximal number of subjects
			 */
			unsigned max_subjects() const { return MAX_SUBJECTS; }

			/**
			 * Allocate new subject
			 *
			 * \param name name of subject
			 * \param id subject id of tracre subject
			 */
			Followed_subject *alloc(char const *name, Genode::Trace::Subject_id &id,
			                        Genode::Region_map &rm)
			{
				int handle = _find_free_handle();

				_subjects[handle] = new (&_md_alloc) Followed_subject(_md_alloc, name, rm, id, handle);

				return _subjects[handle];
			}

			/**
			 * Free subject
			 *
			 * \param subject pointer to subject
			 */
			void free(Followed_subject *subject)
			{
				int handle = subject->handle();

				if (!_in_range(handle))
					return;

				if(!_subjects[handle])
					return;

				_subjects[handle] = 0;
				destroy(&_md_alloc, subject);
			}

			/**
			 * Lookup subject by using the id
			 *
			 * \throw Invalid_subject();
			 */
			Followed_subject *lookup(Genode::Trace::Subject_id const &sid)
			{
				for (unsigned i = 0; i < MAX_SUBJECTS; i++)
					if (_subjects[i]) {
						if (_subjects[i]->id().id == sid.id)
							return _subjects[i];
					}

				throw Invalid_subject();
			}
	};
}

#endif /* _SUBJECT_REGISTRY_H_ */
