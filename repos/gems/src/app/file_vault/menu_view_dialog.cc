/*
 * \brief  Local utilities for the menu view dialog
 * \author Martin Stein
 * \date   2021-02-24
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <menu_view_dialog.h>
#include <capacity.h>

using namespace File_vault;


void File_vault::gen_normal_font_attribute(Xml_generator &xml)
{
	xml.attribute("font", "text/regular");
}


void File_vault::gen_frame_title(Xml_generator &xml,
                                 char    const *name,
                                 unsigned long  min_width,
                                 bool           jent_avail)
{

	xml.node("float", [&] () {
		xml.attribute("name", name);
		xml.attribute("west", "yes");
		xml.attribute("north", "yes");

		if (jent_avail) {
			xml.node("label", [&] () {
				xml.attribute("text", "" );
				xml.attribute("min_ex", min_width);
			});
		} else {
			xml.node("vbox", [&] () {
				xml.node("label", [&] () {
					xml.attribute("name", "warning_0");
					xml.attribute("font", "title/regular");
					xml.attribute("text", " Warning: Insecure mode, no entropy source! " );
					xml.attribute("min_ex", min_width);
				});
				xml.node("label", [&] () {
					xml.attribute("name", "warning_1");
					xml.attribute("text", " " );
				});
			});
		}
	});
}

void File_vault::gen_info_frame(Xml_generator &xml,
                                bool           jent_avail,
                                char const    *name,
                                char const    *info,
                                unsigned long  min_width)
{
	gen_main_frame(xml, jent_avail, name, min_width, [&] (Xml_generator &xml) {

		gen_centered_info_line(xml, "info", info);
		gen_info_line(xml, "pad_1", "");
	});
}

void File_vault::gen_action_button_at_bottom(Xml_generator &xml,
                                             char const    *name,
                                             char const    *label,
                                             bool           hovered,
                                             bool           selected)
{
	xml.node("float", [&] () {
		xml.attribute("name", name);
		xml.attribute("east",  "yes");
		xml.attribute("west",  "yes");
		xml.attribute("south",  "yes");

		xml.node("button", [&] () {

			if (hovered) {
				xml.attribute("hovered", "yes");
			}
			if (selected) {
				xml.attribute("selected", "yes");
			}

			xml.node("float", [&] () {

				xml.node("label", [&] () {
					gen_normal_font_attribute(xml);
					xml.attribute("text", label);
				});
			});
		});
	});
}

void File_vault::gen_action_button_at_bottom(Xml_generator &xml,
                                             char const    *label,
                                             bool           hovered,
                                             bool           selected)
{
	gen_action_button_at_bottom(xml, label, label, hovered, selected);
}

void File_vault::gen_action_button(Xml_generator &xml,
                                   char const    *name,
                                   char const    *label,
                                   bool           hovered,
                                   bool           selected,
                                   size_t         min_ex)
{
	xml.node("button", [&] () {
		xml.attribute("name", name);

		if (hovered) {
			xml.attribute("hovered", "yes");
		}
		if (selected) {
			xml.attribute("selected", "yes");
		}
		xml.node("label", [&] () {

			if (min_ex != 0) {
				xml.attribute("min_ex", min_ex);
			}
			xml.attribute("text", label);
		});
	});
}

void File_vault::gen_text_input(Xml_generator     &xml,
                                char        const *name,
                                String<256> const &text,
                                bool               selected)
{
	String<256> const padded_text { " ", text };

	xml.node("frame", [&] () {
		xml.attribute("name", name);
		xml.node("float", [&] () {
			xml.attribute("west", "yes");
			xml.node("label", [&] () {
				gen_normal_font_attribute(xml);
				xml.attribute("text", padded_text);

				if (selected) {
					xml.node("cursor", [&] () {
						xml.attribute("at", padded_text.length() - 1 );
					});
				}
			});
		});
	});
}

void
File_vault::
gen_input_passphrase(Xml_generator          &xml,
                     size_t                  max_width,
                     Input_passphrase const &passphrase,
                     bool                    input_selected,
                     bool                    show_hide_button_hovered,
                     bool                    show_hide_button_selected)
{
	char const *show_hide_button_label;
	size_t cursor_at;
	if (passphrase.hide()) {

		show_hide_button_label = "Show";
		cursor_at = passphrase.length() + 1;

	} else {

		show_hide_button_label = "Hide";
		cursor_at = passphrase.length() + 1;
	}
	xml.node("float", [&] () {
		xml.attribute("name", "Passphrase Label");
		xml.attribute("west", "yes");

		xml.node("label", [&] () {
			gen_normal_font_attribute(xml);
			xml.attribute("text", " Passphrase: ");
		});
	});
	xml.node("hbox", [&] () {

		String<256> const padded_text { " ", passphrase, " " };
		xml.node("frame", [&] () {
			xml.attribute("name", "Passphrase");
			xml.node("float", [&] () {
				xml.attribute("west", "yes");
				xml.node("label", [&] () {
					xml.attribute("min_ex", max_width - 11);
					gen_normal_font_attribute(xml);
					xml.attribute("text", padded_text);


					if (input_selected) {
						xml.node("cursor", [&] () {
							xml.attribute("at", cursor_at );
						});
					}
				});
			});
		});
		xml.node("float", [&] () {
			xml.attribute("name", "1");
			xml.attribute("east", "yes");

			gen_action_button(
				xml, "Show Hide", show_hide_button_label,
				show_hide_button_hovered, show_hide_button_selected, 5);
		});
	});
}

void File_vault::gen_titled_text_input(Xml_generator     &xml,
                                       char        const *name,
                                       char        const *title,
                                       String<256> const &text,
                                       bool               selected)
{
	xml.node("float", [&] () {
		xml.attribute("name", String<64> { name, "_label" });
		xml.attribute("west", "yes");

		xml.node("label", [&] () {
			gen_normal_font_attribute(xml);
			xml.attribute("text", String<64> { " ", title, ": " });
		});
	});
	gen_text_input(xml, name, text, selected);
}

void File_vault::gen_empty_line(Xml_generator     &xml,
                                char        const *name,
                                size_t             min_width)
{
	xml.node("label", [&] () {
		xml.attribute("name", name);
		xml.attribute("min_ex", min_width);
		xml.attribute("text", "");
	});
}

void File_vault::gen_info_line(Xml_generator     &xml,
                               char        const *name,
                               char        const *text)
{
	xml.node("float", [&] () {
		xml.attribute("name", name);
		xml.attribute("west",  "yes");
		xml.node("label", [&] () {
			gen_normal_font_attribute(xml);
			xml.attribute("text", String<256> { " ", text, " "});
		});
	});
}

void File_vault::gen_centered_info_line(Xml_generator     &xml,
                                        char        const *name,
                                        char        const *text)
{
	xml.node("float", [&] () {
		xml.attribute("name", name);
		xml.node("label", [&] () {
			gen_normal_font_attribute(xml);
			xml.attribute("text", String<256> { " ", text, " "});
		});
	});
}

void File_vault::gen_multiple_choice_entry(Xml_generator &xml,
                                           char     const *name,
                                           char     const *text,
                                           bool            hovered,
                                           bool            selected)
{
	xml.node("float", [&] () {
		xml.attribute("name", name);
		xml.attribute("west", "yes");

		xml.node("hbox", [&] () {

			xml.node("button", [&] () {
				if (selected) {
					xml.attribute("selected", "yes");
				}
				if (hovered) {
					xml.attribute("hovered", "yes");
				}
				xml.attribute("style", "radio");

				xml.node("hbox", [&] () { });
			});
			xml.node("label", [&] () {
				gen_normal_font_attribute(xml);
				xml.attribute("text", String<64> { " ", text });
			});
		});
	});
}

void File_vault::gen_menu_title(Xml_generator &xml,
                                char    const *name,
                                char    const *label,
                                char    const *label_annex,
                                bool           hovered,
                                bool           selected)
{
	xml.node("hbox", [&] () {
		xml.attribute("name", name);

		xml.node("float", [&] () {
			xml.attribute("name", "0");
			xml.attribute("west", "yes");

			xml.node("hbox", [&] () {

				xml.node("button", [&] () {
					if (selected) {
						xml.attribute("style", "back");
						xml.attribute("selected", "yes");
					} else {
						xml.attribute("style", "radio");
					}
					if (hovered) {
						xml.attribute("hovered", "yes");
					}
					xml.attribute("hovered", "no");

					xml.node("hbox", [&] () { });
				});
				xml.node("label", [&] () {
					if (selected) {
						xml.attribute("font", "title/regular");
					}
					xml.attribute("text", String<64> { " ", label });
				});
			});
		});
		xml.node("float", [&] () {
			xml.attribute("name", "2");
			xml.attribute("east", "yes");

			xml.node("label", [&] () {
				xml.attribute("font", "title/regular");
				xml.attribute(
					"text", label_annex);
			});
		});
	});
}


void File_vault::gen_closed_menu(Xml_generator &xml,
                                 char    const *label,
                                 char    const *label_annex,
                                 bool           hovered)
{
	xml.node("vbox", [&] () {
		xml.attribute("name", label);

		gen_menu_title(xml, "Enter", label, label_annex, hovered, false);
	});
}


void File_vault::gen_global_controls(Xml_generator &xml,
                                     size_t         min_width,
                                     size_t         tresor_image_size,
                                     size_t         client_fs_size,
                                     size_t         nr_of_clients,
                                     bool           lock_button_hovered,
                                     bool           lock_button_selected)
{
	gen_empty_line(xml, "Status 0", min_width);
	gen_centered_info_line(xml, "Status 1",
		String<256> {
			" Image: ",
			Capacity_string { tresor_image_size },
			", Client FS: ",
			Capacity_string { client_fs_size },
			", Clients: ",
			nr_of_clients
		}.string()
	);

	gen_empty_line(xml, "Status 3", 0);

	xml.node("hbox", [&] () {
		gen_action_button(
			xml, "Lock", "Lock", lock_button_hovered, lock_button_selected);
	});
}
