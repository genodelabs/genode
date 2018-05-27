/*
 * \brief  Data structure for tracking the state of imported archives
 * \author Norman Feske
 * \date   2018-01-11
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _IMPORT_H_
#define _IMPORT_H_

/* Genode includes */
#include <util/xml_node.h>
#include <util/xml_generator.h>
#include <base/registry.h>
#include <base/allocator.h>

#include "types.h"

namespace Depot_download_manager {
	using namespace Depot;
	struct Import;
}


class Depot_download_manager::Import
{
	public:

		/**
		 * Interface for obtaining the download progress for a given archive
		 */
		struct Download_progress : Interface
		{
			struct Info
			{
				typedef String<32> Bytes;
				Bytes total, now;
			};

			virtual Info download_progress(Archive::Path const &) const = 0;
		};

	private:

		struct Item
		{
			Registry<Item>::Element _element;

			Archive::Path const path;

			enum State { DOWNLOAD_IN_PROGRESS,
			             DOWNLOAD_COMPLETE,
			             DOWNLOAD_UNAVAILABLE,
			             VERIFIED,
			             VERIFICATION_FAILED,
			             UNPACKED };

			State state = DOWNLOAD_IN_PROGRESS;

			Item(Registry<Item> &registry, Archive::Path const &path)
			:
				_element(registry, *this), path(path)
			{ }

			char const *state_text() const
			{
				switch (state) {
				case DOWNLOAD_IN_PROGRESS: return "download";
				case DOWNLOAD_COMPLETE:    return "verify";
				case DOWNLOAD_UNAVAILABLE: return "unavailable";
				case VERIFIED:             return "extract";
				case VERIFICATION_FAILED:  return "verification failed";
				case UNPACKED:             return "done";
				};
				return "";
			}
		};

		Allocator &_alloc;

		Registry<Item> _items { };

		template <typename FN>
		void _for_each_item(Item::State state, FN const &fn) const
		{
			_items.for_each([&] (Item const &item) {
				if (item.state == state)
					fn(item.path); });
		}

		/**
		 * Return true if at least one item is in the given 'state'
		 */
		bool _item_state_exists(Item::State state) const
		{
			bool result = false;
			_items.for_each([&] (Item const &item) {
				if (!result && item.state == state)
					result = true; });
			return result;
		}

	public:

		/**
		 * Constructor
		 *
		 * \param user  depot origin to use for the import
		 * \param node  XML node containing any number of '<missing>' sub nodes
		 *
		 * The import constructor considers only those '<missing>' sub nodes as
		 * items that match the 'user'. The remaining sub nodes are imported in
		 * a future iteration.
		 */
		Import(Allocator &alloc, Archive::User const &user, Xml_node node)
		:
			_alloc(alloc)
		{
			node.for_each_sub_node("missing", [&] (Xml_node item) {
				Archive::Path const path = item.attribute_value("path", Archive::Path());
				if (Archive::user(path) == user)
					new (alloc) Item(_items, path);
			});
		}

		~Import()
		{
			_items.for_each([&] (Item &item) { destroy(_alloc, &item); });
		}

		bool downloads_in_progress() const
		{
			return _item_state_exists(Item::DOWNLOAD_IN_PROGRESS);
		}

		bool unverified_archives_available() const
		{
			return _item_state_exists(Item::DOWNLOAD_COMPLETE);
		}

		bool verified_archives_available() const
		{
			return _item_state_exists(Item::VERIFIED);
		}

		template <typename FN>
		void for_each_download(FN const &fn) const
		{
			_for_each_item(Item::DOWNLOAD_IN_PROGRESS, fn);
		}

		template <typename FN>
		void for_each_unverified_archive(FN const &fn) const
		{
			_for_each_item(Item::DOWNLOAD_COMPLETE, fn);
		}

		template <typename FN>
		void for_each_verified_archive(FN const &fn) const
		{
			_for_each_item(Item::VERIFIED, fn);
		}

		template <typename FN>
		void for_each_ready_archive(FN const &fn) const
		{
			_for_each_item(Item::UNPACKED, fn);
		}

		void all_downloads_completed()
		{
			_items.for_each([&] (Item &item) {
				if (item.state == Item::DOWNLOAD_IN_PROGRESS)
					item.state =  Item::DOWNLOAD_COMPLETE; });
		}

		void all_downloads_unavailable()
		{
			_items.for_each([&] (Item &item) {
				if (item.state == Item::DOWNLOAD_IN_PROGRESS)
					item.state =  Item::DOWNLOAD_UNAVAILABLE; });
		}

		void archive_verified(Archive::Path const &archive)
		{
			_items.for_each([&] (Item &item) {
				if (item.state == Item::DOWNLOAD_COMPLETE)
					if (item.path == archive)
						item.state = Item::VERIFIED; });
		}

		void archive_verification_failed(Archive::Path const &archive)
		{
			_items.for_each([&] (Item &item) {
				if (item.state == Item::DOWNLOAD_COMPLETE)
					if (item.path == archive)
						item.state = Item::VERIFICATION_FAILED; });
		}

		void all_verified_archives_extracted()
		{
			_items.for_each([&] (Item &item) {
				if (item.state == Item::VERIFIED)
					item.state = Item::UNPACKED; });
		}

		void report(Xml_generator &xml, Download_progress const &progress) const
		{
			_items.for_each([&] (Item const &item) {
				xml.node("archive", [&] () {
					xml.attribute("path",  item.path);
					xml.attribute("state", item.state_text());

					if (item.state == Item::DOWNLOAD_IN_PROGRESS) {
						Download_progress::Info const info =
							progress.download_progress(item.path);
						xml.attribute("total", info.total);
						xml.attribute("now",   info.now);
					}
				});
			});
		}
};

#endif /* _IMPORT_H_ */
