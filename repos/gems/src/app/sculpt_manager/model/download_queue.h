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

#include <depot/archive.h>
#include <base/registry.h>
#include <types.h>

namespace Sculpt { struct Download_queue; }


struct Sculpt::Download_queue : Noncopyable
{
	struct Download : Interface
	{
		Path const path;

		bool const verify;

		enum class State { DOWNLOADING, FAILED, DONE } state;

		unsigned percent = 0;

		Download(Path const &path, Verify verify)
		:
			path(path), verify(verify.value), state(State::DOWNLOADING)
		{ }

		void gen_installation_entry(Xml_generator &xml) const
		{
			if (state != State::DOWNLOADING)
				return;

			auto gen_verify_attr = [&] {
				if (!verify)
					xml.attribute("verify", "no"); };

			auto gen_install_node = [&] (auto type) {
				xml.node(type, [&] {
					xml.attribute("path", path);
					gen_verify_attr(); }); };

			if (Depot::Archive::index(path))
				gen_install_node("index");
			else if (Depot::Archive::image_index(path))
				gen_install_node("image_index");
			else if (Depot::Archive::image(path))
				gen_install_node("image");
			else
				xml.node("archive", [&] {
					xml.attribute("path", path);
					xml.attribute("source", "no");
					gen_verify_attr(); });
		}
	};

	Allocator &_alloc;

	Registry<Registered<Download> > _downloads { };

	Download_queue(Allocator &alloc) : _alloc(alloc) { }

	bool _state_present(Download::State const state) const
	{
		bool result = false;
		_downloads.for_each([&] (Download const &download) {
			if (!result && download.state == state)
				result = true; });

		return result;
	}

	void add(Path const &path, Verify const verify)
	{
		bool already_exists = false;
		_downloads.for_each([&] (Download const &download) {
			if (download.path == path)
				already_exists = true; });

		if (already_exists)
			return;

		new (_alloc) Registered<Download>(_downloads, path, verify);
	}

	void with_download(Path const &path, auto const &fn) const
	{
		_downloads.for_each([&] (Download const &download) {
			if (download.path == path)
				fn(download); });
	}

	bool in_progress(Path const &path) const
	{
		bool result = false;
		_downloads.for_each([&] (Download const &download) {
			if (download.path == path && download.state == Download::State::DOWNLOADING)
				result = true; });
		return result;
	}

	void apply_update_state(Xml_node const &state)
	{
		/* 'elem' may be of type 'index' or 'archive' */
		state.for_each_sub_node([&] (Xml_node elem) {

			Path     const path    = elem.attribute_value("path", Path());
			size_t   const total   = elem.attribute_value("total", 0UL);
			size_t   const now     = elem.attribute_value("now",   0UL);
			unsigned const percent = unsigned(total ? (now*100)/total : 0UL);

			_downloads.for_each([&] (Download &download) {

				if (download.path != path)
					return;

				typedef String<16> State;
				State const state = elem.attribute_value("state", State());

				download.percent = percent;

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

	void remove_completed_downloads()
	{
		_downloads.for_each([&] (Download &download) {
			if (download.state == Download::State::DONE)
				destroy(_alloc, &download); });
	}

	void reset()
	{
		_downloads.for_each([&] (Download &download) {
			destroy(_alloc, &download); });
	}

	void gen_installation_entries(Xml_generator &xml) const
	{
		_downloads.for_each([&] (Download const &download) {
			download.gen_installation_entry(xml); });
	}

	bool any_active_download()    const { return _state_present(Download::State::DOWNLOADING); }
	bool any_completed_download() const { return _state_present(Download::State::DONE); }
	bool any_failed_download()    const { return _state_present(Download::State::FAILED); }

	void for_each_failed_download(auto const &fn) const
	{
		_downloads.for_each([&] (Download const &download) {
			if (download.state == Download::State::FAILED)
				fn(download.path); });
	}
};

#endif /* _MODEL__DOWNLOAD_QUEUE_H_ */
