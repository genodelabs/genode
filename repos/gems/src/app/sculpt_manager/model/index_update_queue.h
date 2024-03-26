/*
 * \brief  Queue for tracking the update of depot index files
 * \author Norman Feske
 * \date   2023-01-27
 *
 * The update of a depot index takes two steps. First, the index must
 * be removed. Then, the index can be requested again via the depot-download
 * mechanism.
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__INDEX_UPDATE_QUEUE_H_
#define _MODEL__INDEX_UPDATE_QUEUE_H_

#include <model/download_queue.h>
#include <model/file_operation_queue.h>
#include <types.h>

namespace Sculpt { struct Index_update_queue; }


struct Sculpt::Index_update_queue : Noncopyable
{
	struct Update : Interface
	{
		Path const path;

		Verify const verify;

		enum class State { REMOVING, DOWNLOADING, DONE, FAILED } state;

		Update(Path const &path, Verify verify)
		:
			path(path), verify(verify), state(State::REMOVING)
		{ }

		bool active() const { return state == State::REMOVING
		                          || state == State::DOWNLOADING; }
	};

	Allocator            &_alloc;
	File_operation_queue &_file_operation_queue;
	Download_queue       &_download_queue;

	Registry<Registered<Update> > _updates { };

	/* used for detecting the start of new downloads */
	unsigned download_count = 0;

	Index_update_queue(Allocator            &alloc,
	                   File_operation_queue &file_operation_queue,
	                   Download_queue       &download_queue)
	:
		_alloc(alloc),
		_file_operation_queue(file_operation_queue),
		_download_queue(download_queue)
	{ }

	void add(Path const &path, Verify const verify)
	{
		if (!Depot::Archive::index(path) && !Depot::Archive::image_index(path)) {
			warning("attempt to add a non-index path '", path, "' to index-download queue");
			return;
		}

		bool already_exists = false;
		_updates.for_each([&] (Update const &update) {
			if (update.path == path)
				already_exists = true; });

		if (already_exists) {
			warning("index update triggered while update is already in progress");
			return;
		}

		new (_alloc) Registered<Update>(_updates, path, verify);

		_file_operation_queue.remove_file(Path("/rw/depot/",  path));
		_file_operation_queue.remove_file(Path("/rw/public/", path, ".xz"));
		_file_operation_queue.remove_file(Path("/rw/public/", path, ".xz.sig"));

		if (!_file_operation_queue.any_operation_in_progress())
			_file_operation_queue.schedule_next_operations();
	}

	void try_schedule_downloads()
	{
		/*
		 * Once the 'File_operation_queue' is empty, we know that no removal of
		 * any index file is still in progres.
		 */
		if (!_file_operation_queue.empty())
			return;

		_updates.for_each([&] (Update &update) {
			if (update.state == Update::State::REMOVING) {
				update.state =  Update::State::DOWNLOADING;
				_download_queue.add(update.path, update.verify);
				download_count++;
			}
		});
	}

	bool any_download_scheduled() const
	{
		bool result = false;
		_updates.for_each([&] (Update const &update) {
			if (update.state == Update::State::DOWNLOADING)
				result = true; });
		return result;
	}

	void with_update(Path const &path, auto const &fn) const
	{
		_updates.for_each([&] (Update const &update) {
			if (update.path == path)
				fn(update); });
	}

	void apply_update_state(Xml_node state)
	{
		state.for_each_sub_node([&] (Xml_node const &elem) {

			Path const path = elem.attribute_value("path", Path());
			_updates.for_each([&] (Update &update) {

				if (update.path != path)
					return;

				using State = String<16>;
				State const state = elem.attribute_value("state", State());

				if (state == "done")        update.state = Update::State::DONE;
				if (state == "failed")      update.state = Update::State::FAILED;
				if (state == "unavailable") update.state = Update::State::FAILED;
				if (state == "corrupted")   update.state = Update::State::FAILED;
			});
		});
	}

	void remove_inactive_updates()
	{
		_updates.for_each([&] (Update &update) {
			if (!update.active())
				destroy(_alloc, &update); });
	}

	void remove_completed_updates()
	{
		_updates.for_each([&] (Update &update) {
			if (update.state == Update::State::DONE)
				destroy(_alloc, &update); });
	}
};

#endif /* _MODEL__INDEX_UPDATE_QUEUE_H_ */
