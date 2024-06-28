/*
 * \brief  Simple text viewer and editor
 * \author Norman Feske
 * \date   2020-01-12
 */

/*
 * Copyright (C) 2020-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <dialog/runtime.h>
#include <dialog/text_area_widget.h>
#include <os/reporter.h>
#include <os/vfs.h>

namespace Text_area {

	using namespace Dialog;

	struct Main;
}


struct Text_area::Main : Text_area_widget::Action
{
	Env &_env;

	Heap _heap { _env.ram(), _env.rm() };

	Attached_rom_dataspace _config { _env, "config" };

	Root_directory _vfs { _env, _heap, _config.xml().sub_node("vfs") };

	Runtime _runtime { _env, _heap };

	struct Main_dialog : Top_level_dialog
	{
		Main &_main;

		Hosted<Frame, Button, Float, Text_area_widget> text { Id { "text" }, _main._heap };

		Main_dialog(Main &main) : Top_level_dialog("text_area"), _main(main) { }

		void view(Scope<> &s) const override
		{
			s.sub_scope<Frame>([&] (Scope<Frame> &s) {
				s.sub_scope<Button>([&] (Scope<Frame, Button> &s) {

					if (s.hovered())
						s.attribute("hovered", "yes");

					s.sub_scope<Float>([&] (Scope<Frame, Button, Float> &s) {
						s.attribute("north", "yes");
						s.attribute("east",  "yes");
						s.attribute("west",  "yes");
						s.widget(text);
					});
				});
			});
		}

		void click(Clicked_at const &at) override { text.propagate(at); }
		void clack(Clacked_at const &at) override { text.propagate(at, _main); }
		void drag (Dragged_at const &at) override { text.propagate(at); }

	} _dialog { *this };

	Runtime::View _view { _runtime, _dialog };

	/* handler used to respond to keyboard input */
	Runtime::Event_handler<Main> _event_handler { _runtime, *this, &Main::_handle_event };

	void _handle_event(::Dialog::Event const &event)
	{
		bool const orig_modified = _modified();

		_dialog.text.handle_event(event, *this);

		event.event.handle_press([&] (Input::Keycode const key, Codepoint) {

			/* paste on middle mouse click */
			bool const middle_click = (key == Input::BTN_MIDDLE);
			if (middle_click) {
				_view.if_hovered([&] (Hovered_at const &at) {
					_dialog.text.move_cursor_to(at);
					trigger_paste();
					_view.refresh();
					return true;
				});
			}
		});

		if (_modified() != orig_modified)
			_generate_saved_report();
	}

	Constructible<Expanding_reporter> _saved_reporter { };

	struct Saved_version { unsigned value; } _saved_version { 0 };

	/*
	 * The dialog's modification count at the time of last saving
	 */
	struct Modification_count { unsigned value; } _saved_modification_count { 0 };

	bool _modified() const
	{
		return _dialog.text.modification_count() != _saved_modification_count.value;
	}

	void _generate_saved_report()
	{
		if (!_saved_reporter.constructed())
			return;

		_saved_reporter->generate([&] (Xml_generator &xml) {
			xml.attribute("version", _saved_version.value);
			if (_modified())
				xml.attribute("modified", "yes");
		});
	}

	Directory::Path _path() const
	{
		return _config.xml().attribute_value("path", Directory::Path());
	}

	void _watch(bool enabled)
	{
		_watch_handler.conditional(enabled, _env.ep(), _vfs, _path(), *this,
		                           &Main::_handle_watch);
	}

	bool _editable() const { return !_watch_handler.constructed(); }

	void _load()
	{
		struct Max_line_len_exceeded : Exception { };

		try {
			File_content content(_heap, _vfs, _path(), File_content::Limit{1024*1024});

			enum { MAX_LINE_LEN = 1000 };
			using Content_line = String<MAX_LINE_LEN + 1>;

			_dialog.text.clear();
			content.for_each_line<Content_line>([&] (Content_line const &line) {

				if (line.length() == Content_line::capacity()) {
					warning("maximum line length ", (size_t)MAX_LINE_LEN, " exceeded");
					throw Max_line_len_exceeded();
				}

				_dialog.text.append_newline();

				for (Utf8_ptr utf8(line.string()); utf8.complete(); utf8 = utf8.next())
					_dialog.text.append_character(utf8.codepoint());
			});

		}
		catch (...) {
			warning("failed to load file ", _path());
			_dialog.text.clear();
		}

		_view.refresh();
	}

	Constructible<Watch_handler<Main>> _watch_handler { };

	void _handle_watch() { _load(); }

	/*
	 * Copy
	 */

	Constructible<Expanding_reporter> _clipboard_reporter { };

	/**
	 * Text_area::Dialog::Action interface
	 */
	void trigger_copy() override
	{
		if (!_clipboard_reporter.constructed())
			return;

		_clipboard_reporter->generate([&] (Xml_generator &xml) {
			_dialog.text.gen_clipboard_content(xml); });
	}

	/*
	 * Paste
	 */

	Constructible<Attached_rom_dataspace> _clipboard_rom { };

	enum { PASTE_BUFFER_SIZE = 64*1024 };
	struct Paste_buffer { char buffer[PASTE_BUFFER_SIZE]; } _paste_buffer { };

	/**
	 * Text_area::Dialog::Action interface
	 */
	void trigger_paste() override
	{
		if (!_editable())
			return;

		if (!_clipboard_rom.constructed())
			return;

		_clipboard_rom->update();

		_paste_buffer = { };

		/* leave last byte as zero-termination in tact */
		size_t const max_len = sizeof(_paste_buffer.buffer) - 1;
		size_t const len =
			_clipboard_rom->xml().decoded_content(_paste_buffer.buffer, max_len);

		if (len == max_len) {
			warning("clipboard content exceeds paste buffer");
			return;
		}

		for (Utf8_ptr utf8(_paste_buffer.buffer); utf8.complete(); utf8 = utf8.next())
			_dialog.text.insert_at_cursor_position(utf8.codepoint());

		_view.refresh();
	}

	/*
	 * Save
	 */

	void _save_to_file(Directory::Path const &path)
	{
		bool write_error = false;

		try {
			New_file new_file(_vfs, path);

			auto write = [&] (char const *cstring)
			{
				switch (new_file.append(cstring, strlen(cstring))) {
				case New_file::Append_result::OK: break;
				case New_file::Append_result::WRITE_ERROR:
					write_error = true;
					break;
				}
			};

			Buffered_output<1024, decltype(write)> output(write);

			_dialog.text.for_each_character([&] (Codepoint c) { print(output, c); });
		}
		catch (New_file::Create_failed) {
			error("file creation failed while saving file"); }

		if (write_error) {
			error("write error while saving ", _path());
			return;
		}

		_saved_modification_count.value = _dialog.text.modification_count();

		_generate_saved_report();
	}

	/**
	 * Text_area::Dialog::Action interface
	 */
	void trigger_save() override
	{
		if (!_editable())
			return;

		_saved_version.value++;
		_save_to_file(_path());
	}

	bool _initial_config = true;

	void _handle_config()
	{
		_config.update();

		Xml_node const config = _config.xml();

		bool const copy_enabled  = config.attribute_value("copy",  false);
		bool const paste_enabled = config.attribute_value("paste", false);

		_clipboard_reporter.conditional(copy_enabled,  _env, "clipboard", "clipboard");
		_clipboard_rom     .conditional(paste_enabled, _env, "clipboard");

		_dialog.text.max_lines(config.attribute_value("max_lines", ~0U));

		_watch(config.attribute_value("watch", false));

		_dialog.text.editable(_editable());

		if (_editable()) {
			bool const orig_saved_reporter_enabled = _saved_reporter.constructed();

			config.with_optional_sub_node("report", [&] (Xml_node const &node) {
				_saved_reporter.conditional(node.attribute_value("saved", false),
				                            _env, "saved", "saved"); });

			bool const saved_report_out_of_date =
				!orig_saved_reporter_enabled && _saved_reporter.constructed();

			Saved_version const orig_saved_version = _saved_version;

			config.with_optional_sub_node("save", [&] (Xml_node const &node) {
				_saved_version.value =
					node.attribute_value("version", _saved_version.value); });

			bool const saved_version_changed =
				(_saved_version.value != orig_saved_version.value);

			if (saved_version_changed || saved_report_out_of_date) {

				if (!_initial_config)
					_save_to_file(_path());
				else
					_generate_saved_report();
			}
		}

		_initial_config = false;
	}

	Signal_handler<Main> _config_handler {
		_env.ep(), *this, &Main::_handle_config };

	/**
	 * Text_area::Dialog::Action interface
	 */
	void refresh_text_area() override { _view.refresh(); }

	Main(Env &env)
	:
		_env(env)
	{
		/*
		 * The '_load' must be performed before '_handle_config' because
		 * '_handle_config' may call '_save_to_file' if the <config> contains a
		 * <saved> node.
		 */
		_load();

		_config.sigh(_config_handler);
		_handle_config();
	}
};


void Component::construct(Genode::Env &env) { static Text_area::Main main(env); }

