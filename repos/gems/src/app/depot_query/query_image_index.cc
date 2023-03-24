/*
 * \brief  Querying system-image information from a depot
 * \author Norman Feske
 * \date   2023-01-26
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <os/buffered_xml.h>

/* local includes */
#include <main.h>


void Depot_query::Main::_query_image_index(Xml_node const &index_query,
                                           Require_verify require_verify,
                                           Xml_generator &xml)
{
	using User    = Archive::User;
	using Version = String<16>;
	using Os      = String<16>;
	using Board   = String<32>;

	User  const user  = index_query.attribute_value("user",  User());
	Os    const os    = index_query.attribute_value("os",    Os());
	Board const board = index_query.attribute_value("board", Board());

	struct Version_reverse : Version
	{
		using Version::Version;

		bool operator > (Version_reverse const &other) const
		{
			return strcmp(string(), other.string()) < 0;
		}
	};

	struct Image_info;

	using Image_dict = Dictionary<Image_info, Version_reverse>;

	struct Image_info : Image_dict::Element
	{
		Constructible<Buffered_xml> from_index { };

		enum Presence { PRESENT, ABSENT } const presence;

		Image_info(Image_dict &dict, Version_reverse const &version, Presence presence)
		: Image_dict::Element(dict, version), presence(presence) { }

		void generate(Xml_generator &xml) const
		{
			xml.node("image", [&] {
				xml.attribute("version", name);

				if (presence == PRESENT)
					xml.attribute("present", "yes");

				if (!from_index.constructed())
					return;

				from_index->xml().for_each_sub_node("info", [&] (Xml_node const &info) {
					using Text = String<160>;
					Text const text = info.attribute_value("text", Text());
					if (text.valid())
						xml.node("info", [&] {
							xml.attribute("text", text); });
				});
			});
		}
	};

	Image_dict images { };

	Directory::Path const prefix(os, "-", board, "-");

	/* return version part of the image-file name */
	auto version_from_name = [&prefix] (auto name)
	{
		size_t const prefix_chars = prefix.length() - 1;

		if (strcmp(prefix.string(), name.string(), prefix_chars))
			return Version_reverse(); /* prefix mismatch */

		return Version_reverse(name.string() + prefix_chars);
	};

	Directory::Path const image_path("depot/", user, "/image");
	if (_root.directory_exists(image_path)) {

		Directory(_root, image_path).for_each_entry([&] (Directory::Entry const &entry) {

			Directory::Entry::Name const name    = entry.name();
			Version_reverse        const version = version_from_name(name);

			if (entry.dir() && version.length() > 1)
				new (_heap) Image_info { images, version, Image_info::PRESENT };
		});
	}

	/*
	 * Supplement information found in the index file, if present
	 */
	Directory::Path const index_path("depot/", user, "/image/index");

	bool index_exists = _root.file_exists(index_path);

	if (index_exists) {
		try {
			File_content const
				file(_heap, _root, index_path, File_content::Limit{16*1024});

			file.xml([&] (Xml_node node) {

				node.for_each_sub_node("image", [&] (Xml_node const &image) {

					bool const os_and_board_match =
						(image.attribute_value("os",    Os())    == os) &&
						(image.attribute_value("board", Board()) == board);

					if (!os_and_board_match)
						return;

					Version_reverse const version {
						image.attribute_value("version", Version()).string() };

					if (!images.exists(version))
						new (_heap) Image_info(images, version, Image_info::ABSENT);

					images.with_element(version,
						[&] (Image_info &info) {
							info.from_index.construct(_heap, image); },
						[&] () { }
					);
				});
			});
		}
		catch (Directory::Nonexistent_file) {
			index_exists = false;
		}
	}

	/*
	 * Give feedback to depot_download_manager about the absence of the index
	 * file.
	 */
	xml.node(index_exists ? "present" : "missing", [&] () {
		xml.attribute("user", user);
		require_verify.gen_attr(xml);
	});

	/*
	 * Report aggregated image information with the newest version first.
	 */
	xml.node("user", [&] () {

		xml.attribute("name",  user);
		xml.attribute("os",    os);
		xml.attribute("board", board);

		images.for_each([&] (Image_info const &info) {
			info.generate(xml); });
	});

	auto destroy_image_info = [&] (Image_info &info) { destroy(_heap, &info); };

	while (images.with_any_element(destroy_image_info));
}
