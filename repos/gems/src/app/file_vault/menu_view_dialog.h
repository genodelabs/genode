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

#ifndef _MENU_VIEW_DIALOG_H_
#define _MENU_VIEW_DIALOG_H_

/* Genode includes */
#include <util/xml_generator.h>

/* local includes */
#include <types.h>
#include <input.h>

namespace File_vault {

	void gen_normal_font_attribute(Xml_generator &xml);

	void gen_frame_title(Xml_generator &xml,
	                     char    const *name,
	                     unsigned long  min_width,
	                     bool           jent_avail);

	template <typename GEN_FRAME_CONTENT>
	void gen_main_frame(Xml_generator           &xml,
	                    bool                     jent_avail,
	                    char              const *name,
	                    unsigned long            min_width,
	                    GEN_FRAME_CONTENT const &gen_frame_content)
	{
		xml.node("frame", [&] () {
			xml.attribute("name", name);

			xml.node("vbox", [&] () {

				gen_frame_title(xml, "title", min_width, jent_avail);
				gen_frame_content(xml);
			});
		});
	}

	template <typename GEN_FRAME_CONTENT>
	void gen_controls_frame(Xml_generator           &xml,
	                        bool                     jent_avail,
	                        char              const *name,
	                        GEN_FRAME_CONTENT const &gen_frame_content)
	{
		xml.node("frame", [&] () {
			xml.attribute("name", name);

			xml.node("vbox", [&] () {

				if (!jent_avail)
					gen_frame_title(xml, "title", 0, jent_avail);

				gen_frame_content(xml);
			});
		});
	}

	template <typename GEN_FRAME_CONTENT>
	void gen_untitled_frame(Xml_generator           &xml,
	                        char              const *name,
	                        GEN_FRAME_CONTENT const &gen_frame_content)
	{
		xml.node("frame", [&] () {
			xml.attribute("name", name);

			xml.node("float", [&] () {
				xml.attribute("name", "xxx");
				xml.attribute("east",  "yes");
				xml.attribute("west",  "yes");
				xml.attribute("north",  "yes");

				xml.node("vbox", [&] () {

					gen_frame_content(xml);
				});
			});
		});
	}

	void gen_info_frame(Xml_generator &xml,
	                    bool           jent_avail,
	                    char const    *name,
	                    char const    *info,
	                    unsigned long  min_width);

	void gen_action_button_at_bottom(Xml_generator &xml,
	                                 char const    *name,
	                                 char const    *label,
	                                 bool           hovered,
	                                 bool           selected);

	void gen_action_button_at_bottom(Xml_generator &xml,
	                                 char const    *label,
	                                 bool           hovered,
	                                 bool           selected);

	void gen_text_input(Xml_generator     &xml,
	                    char        const *name,
	                    String<256> const &text,
	                    bool               selected);

	void gen_titled_text_input(Xml_generator     &xml,
	                           char        const *name,
	                           char        const *title,
	                           String<256> const &text,
	                           bool               selected);

	void gen_info_line(Xml_generator     &xml,
	                   char        const *name,
	                   char        const *text);

	void gen_centered_info_line(Xml_generator     &xml,
	                            char        const *name,
	                            char        const *text);

	void gen_empty_line(Xml_generator     &xml,
	                    char        const *name,
	                    size_t             min_width);

	void gen_multiple_choice_entry(Xml_generator     &xml,
	                               char        const *name,
	                               char        const *text,
	                               bool               hovered,
	                               bool               selected);

	void gen_menu_title(Xml_generator &xml,
	                    char    const *name,
	                    char    const *label,
	                    char    const *label_annex,
	                    bool           hovered,
	                    bool           selected);

	void gen_closed_menu(Xml_generator &xml,
	                     char    const *label,
	                     char    const *label_annex,
	                     bool           hovered);

	template <typename GEN_CONTENT>
	void gen_opened_menu(Xml_generator       &xml,
	                     char          const *label,
	                     char          const *label_annex,
	                     bool                 hovered,
	                     GEN_CONTENT   const &gen_content)
	{
		xml.node("vbox", [&] () {
			xml.attribute("name", label);

			gen_menu_title(xml, "Leave", label, label_annex, hovered, true);
			gen_content(xml);
		});
	}

	void gen_input_passphrase(Xml_generator          &xml,
	                          size_t                  max_width,
	                          Input_passphrase const &passphrase,
	                          bool                    input_selected,
	                          bool                    show_hide_button_hovered,
	                          bool                    show_hide_button_selected);

	void gen_action_button(Xml_generator &xml,
	                       char const    *name,
	                       char const    *label,
	                       bool           hovered,
	                       bool           selected,
	                       size_t         min_ex = 0);

	void gen_global_controls(Xml_generator &xml,
	                         size_t         min_width,
	                         size_t         tresor_image_size,
	                         size_t         client_fs_size,
	                         size_t         nr_of_clients,
	                         bool           lock_button_hovered,
	                         bool           lock_button_selected);
}

#endif /* _MENU_VIEW_DIALOG_H_ */
