/*
 * \brief  List of depot downloads that are currently in flight
 * \author Norman Feske
 * \date   2019-02-25
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__DOWNLOAD_QUEUE_H_
#define _MODEL__DOWNLOAD_QUEUE_H_

#include <base/registry.h>
#include <types.h>

namespace Sculpt { struct Download_queue; }


struct Sculpt::Download_queue : Noncopyable
{
	struct Download : Interface
	{
		Path const path;

		enum class State { DOWNLOADING, FAILED, DONE } state;

		Download(Path const &path) : path(path), state(State::DOWNLOADING) { }

		void gen_installation_entry(Xml_generator &xml) const
		{
			if (state != State::DOWNLOADING)
				return;

			if (Depot::Archive::index(path))
				xml.node("index", [&] () {
					xml.attribute("path", path); });
			else
				xml.node("archive", [&] () {
					xml.attribute("path", path);
					xml.attribute("source", "no"); });
		}
	};

	Allocator &_alloc;

	Registry<Registered<Download> > _downloads { };

	Download_queue(Allocator &alloc) : _alloc(alloc) { }

	void add(Path const &path)
	{
		log("add to download queue: ", path);
		bool already_exists = false;
		_downloads.for_each([&] (Download const &download) {
			if (download.path == path)
				already_exists = true; });

		if (already_exists)
			return;

		new (_alloc) Registered<Download>(_downloads, path);
	}

	bool in_progress(Path const &path) const
	{
		bool result = false;
		_downloads.for_each([&] (Download const &download) {
			if (download.path == path && download.state == Download::State::DOWNLOADING)
				result = true; });

		return result;
	}

	void apply_update_state(Xml_node state)
	{
		/* 'elem' may be of type 'index' or 'archive' */
		state.for_each_sub_node([&] (Xml_node elem) {

			Path const path = elem.attribute_value("path", Path());

			_downloads.for_each([&] (Download &download) {

				if (download.path != path)
					return;

				typedef String<16> State;
				State const state = elem.attribute_value("state", State());

				if (state == "done")        download.state = Download::State::DONE;
				if (state == "failed")      download.state = Download::State::FAILED;
				if (state == "unavailable") download.state = Download::State::FAILED;
				if (state == "corrupted")   download.state = Download::State::FAILED;
			});
		});
	}

	void remove_inactive_downloads()
	{
		_downloads.for_each([&] (Download &download) {
			if (download.state != Download::State::DOWNLOADING)
				destroy(_alloc, &download); });
	}

	void gen_installation_entries(Xml_generator &xml) const
	{
		_downloads.for_each([&] (Download const &download) {
			download.gen_installation_entry(xml); });
	}

	bool any_active_download() const
	{
		bool result = false;
		_downloads.for_each([&] (Download const &download) {
			if (!result && download.state == Download::State::DOWNLOADING)
				result = true; });

		return result;
	}
};

#endif /* _MODEL__DOWNLOAD_QUEUE_H_ */
