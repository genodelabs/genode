/*
 * \brief  Menu view
 * \author Norman Feske
 * \date   2009-09-11
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/sleep.h>
#include <os/reporter.h>
#include <timer_session/connection.h>
#include <os/vfs.h>

/* local includes */
#include <dialog.h>
#include <button_widget.h>
#include <label_widget.h>
#include <box_layout_widget.h>
#include <float_widget.h>
#include <frame_widget.h>
#include <depgraph_widget.h>

namespace Menu_view { struct Main; }


struct Menu_view::Main : Dialog::Action
{
	Env &_env;

	Attached_rom_dataspace _config { _env, "config" };

	Signal_handler<Main> _config_handler = {
		_env.ep(), *this, &Main::_handle_config};

	void _handle_config();

	Heap _heap { _env.ram(), _env.rm() };

	Vfs::Env &_vfs_env;

	Directory _root_dir   { _vfs_env };
	Directory _fonts_dir  { _root_dir, "fonts" };
	Directory _styles_dir { _root_dir, "styles" };

	Style_database _styles { _env.ep(), _env.ram(), _env.rm(), _heap,
	                         _fonts_dir, _styles_dir, _config_handler };

	Animator _global_animator { };

	Widget_factory _widget_factory { _heap, _styles, _global_animator };

	Dialogs _dialogs { };

	Widget::Hovered _reported_hovered { };

	struct Input_seq_number
	{
		Constructible<Input::Seq_number> _curr { };

		bool _reported = false;

		void update(Input::Seq_number const &seq)
		{
			_curr.construct(seq);
			_reported = false;
		}

		void generate(Xml_generator &xml) const
		{
			if (_curr.constructed())
				xml.attribute("seq_number", _curr->value);
		}

		void mark_as_reported() { _reported = true; }

		bool changed() const { return !_reported; }

	} _input_seq_number { };

	struct Frame { uint64_t count; };

	/*
	 * Timer used for animating widgets
	 */
	struct Frame_timer : Timer::Connection
	{
		enum { PERIOD = 10 };

		Frame curr_frame() const { return { elapsed_ms() / PERIOD }; }

		void schedule() { trigger_once((Genode::uint64_t)Frame_timer::PERIOD*1000); }

		Frame_timer(Env &env) : Timer::Connection(env) { }

	} _timer { _env };

	void _handle_frame_timer();

	/**
	 * Dialog::Action
	 */
	void trigger_redraw() override
	{
		/*
		 * If we have not processed a period for at least one frame, perform the
		 * processing immediately. This way, we avoid latencies when the dialog
		 * model is updated sporadically.
		 */
		Frame const curr_frame = _timer.curr_frame();
		if (curr_frame.count != _last_frame.count) {

			if (curr_frame.count - _last_frame.count > 10)
				_last_frame = curr_frame;

			_handle_frame_timer();
		} else {
			_timer.schedule();
		}
	}

	/**
	 * Dialog::Action
	 */
	void hover_changed() override { _update_hover_report(); }

	/**
	 * Dialog::Action
	 */
	void observed_seq_number(Input::Seq_number const seq) override
	{
		_input_seq_number.update(seq);
	}

	Signal_handler<Main> _frame_timer_handler = {
		_env.ep(), *this, &Main::_handle_frame_timer};

	Constructible<Genode::Expanding_reporter> _hover_reporter { };

	void _update_hover_report();

	/**
	 * Frame of last call of 'handle_frame_timer'
	 */
	Frame _last_frame { };

	/**
	 * Number of frames between two redraws
	 */
	static constexpr unsigned REDRAW_PERIOD = 2;

	/**
	 * Counter used for triggering redraws. Incremented in each frame-timer
	 * period, wraps at 'REDRAW_PERIOD'. The redraw is performed when the
	 * counter wraps.
	 */
	unsigned _frame_cnt = 0;

	Main(Env &env, Vfs::Env &libc_vfs_env)
	:
		_env(env), _vfs_env(libc_vfs_env)
	{
		_config.sigh(_config_handler);
		_config_handler.local_submit(); /* apply initial configuration */

		_timer.sigh(_frame_timer_handler);
	}
};


void Menu_view::Main::_update_hover_report()
{
	if (!_hover_reporter.constructed())
		return;

	unsigned hovered_dialogs = 0;

	_dialogs.for_each([&] (Dialog const &dialog) {

		if (!dialog.hovered())
			return;

		hovered_dialogs++;
		if (hovered_dialogs != 1)
			return;

		Widget::Hovered const hovered = dialog.hovered_widget();

		if ((hovered != _reported_hovered) || _input_seq_number.changed()) {

			_hover_reporter->generate([&] (Xml_generator &xml) {
				_input_seq_number.generate(xml);
				dialog.gen_hover(xml);
			});
			_reported_hovered = hovered;
			_input_seq_number.mark_as_reported();
		}
	});

	if (hovered_dialogs == 0)
		_hover_reporter->generate([&] (Xml_generator &) { });

	if (hovered_dialogs > 1)
		warning("more than one dialog unexpectedly hovered at the same time");
}


void Menu_view::Main::_handle_config()
{
	_config.update();

	Xml_node const config = _config.xml();

	config.with_optional_sub_node("report", [&] (Xml_node const &report) {
		_hover_reporter.conditional(report.attribute_value("hover", false),
		                            _env, "hover", "hover"); });

	config.with_optional_sub_node("vfs", [&] (Xml_node const &vfs_node) {
		_vfs_env.root_dir().apply_config(vfs_node); });

	_dialogs.update_from_xml(config,

		/* create */
		[&] (Xml_node const &node) -> Dialog & {
			return *new (_heap) Dialog(_env, _widget_factory, *this, node); },

		/* destroy */
		[&] (Dialog &d) { destroy(_heap, &d); },

		/* update */
		[&] (Dialog &d, Xml_node const &node) { d.update(node); }
	);

	/* re-assign font pointers in labels (needed due to font style change) */
	if (!_styles.up_to_date()) {
		_dialogs.for_each([&] (Dialog &dialog) {
			dialog._handle_dialog();

			/* fast-forward geometry animation on font changes */
			while (dialog.animation_in_progress())
				dialog.animate();
		});

		_styles.flush_outdated_styles();
	}
	trigger_redraw();
}


void Menu_view::Main::_handle_frame_timer()
{
	_frame_cnt++;

	Frame const curr_frame = _timer.curr_frame();

	unsigned const passed_frames =
		max(unsigned(curr_frame.count - _last_frame.count), 4U);

	if (passed_frames > 0)
		_dialogs.for_each([&] (Dialog &dialog) {
			for (unsigned i = 0; i < passed_frames; i++)
				dialog.animate(); });

	bool any_redraw_scheduled = false;
	_dialogs.for_each([&] (Dialog const &dialog) {
		any_redraw_scheduled |= dialog.redraw_scheduled(); });

	_last_frame = curr_frame;

	bool const redraw_skipped = any_redraw_scheduled && (_frame_cnt < REDRAW_PERIOD);

	if (!redraw_skipped) {
		_frame_cnt = 0;
		_dialogs.for_each([&] (Dialog &dialog) {
			dialog.redraw(); });
	}

	bool any_animation_in_progress = false;
	_dialogs.for_each([&] (Dialog const &dialog) {
		any_animation_in_progress |= dialog.animation_in_progress(); });

	/*
	 * Deactivate timer periods when idle, activate timer when an animation is
	 * in progress or a redraw is pending.
	 */
	if (any_animation_in_progress || redraw_skipped)
		_timer.schedule();
}


Menu_view::Widget &
Menu_view::Widget_factory::create(Xml_node const &node)
{
	Widget::Unique_id const unique_id(++_unique_id_cnt);

	if (node.has_type("label"))    return *new (alloc) Label_widget      (*this, node, unique_id);
	if (node.has_type("button"))   return *new (alloc) Button_widget     (*this, node, unique_id);
	if (node.has_type("vbox"))     return *new (alloc) Box_layout_widget (*this, node, unique_id);
	if (node.has_type("hbox"))     return *new (alloc) Box_layout_widget (*this, node, unique_id);
	if (node.has_type("frame"))    return *new (alloc) Frame_widget      (*this, node, unique_id);
	if (node.has_type("float"))    return *new (alloc) Float_widget      (*this, node, unique_id);
	if (node.has_type("depgraph")) return *new (alloc) Depgraph_widget   (*this, node, unique_id);

	/*
	 * This cannot occur because the 'List_model' ensures that 'create' is only
	 * called for nodes that passed 'node_type_known'.
	 */
	error("unknown widget type '", node.type(), "'");
	sleep_forever();
}


bool Menu_view::Widget_factory::node_type_known(Xml_node const &node)
{
	return node.has_type("label")
	    || node.has_type("button")
	    || node.has_type("vbox")
	    || node.has_type("hbox")
	    || node.has_type("frame")
	    || node.has_type("float")
	    || node.has_type("depgraph");
}


/*
 * Silence debug messages
 */
extern "C" void _sigprocmask() { }


void Libc::Component::construct(Libc::Env &env)
{
	static Menu_view::Main main(env, env.vfs_env());
}

