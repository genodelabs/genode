/*
 * \brief  Menu view
 * \author Norman Feske
 * \date   2009-09-11
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* local includes */
#include "widgets.h"

/* Genode includes */
#include <input/event.h>
#include <os/reporter.h>

/* gems includes */
#include <gems/nitpicker_buffer.h>


struct Menu_view::Main
{
	Nitpicker::Connection nitpicker;

	Lazy_volatile_object<Nitpicker_buffer> buffer;

	Nitpicker::Session::View_handle view_handle = nitpicker.create_view();

	Point position;

	Rect _view_geometry;

	void _update_view()
	{
		if (_view_geometry.p1() == position
		 && _view_geometry.area() == buffer->size())
		 	return;

		/* display view behind all others */
		typedef Nitpicker::Session::Command Command;

		_view_geometry = Rect(position, buffer->size());
		nitpicker.enqueue<Command::Geometry>(view_handle, _view_geometry);
		nitpicker.enqueue<Command::To_front>(view_handle);
		nitpicker.execute();
	}

	Signal_receiver &sig_rec;

	/**
	 * Function called on config change or mode change
	 */
	void handle_dialog_update(unsigned);

	Signal_dispatcher<Main> dialog_update_dispatcher = {
		sig_rec, *this, &Main::handle_dialog_update};

	Style_database styles;

	Animator animator;

	Widget_factory widget_factory { *env()->heap(), styles, animator };

	Root_widget root_widget { widget_factory, Xml_node("<dialog/>"), Widget::Unique_id() };

	Attached_rom_dataspace dialog_rom { "dialog" };

	Attached_dataspace input_ds { nitpicker.input()->dataspace() };

	Widget::Unique_id hovered;

	void handle_config(unsigned);

	Signal_dispatcher<Main> config_dispatcher = {
		sig_rec, *this, &Main::handle_config};

	void handle_input(unsigned);

	Signal_dispatcher<Main> input_dispatcher = {
		sig_rec, *this, &Main::handle_input};

	/*
	 * Timer used for animating widgets
	 */
	struct Frame_timer : Timer::Connection
	{
		enum { PERIOD = 10 };

		unsigned curr_frame() const { return elapsed_ms() / PERIOD; }

		void schedule() { trigger_once(Frame_timer::PERIOD*1000); }

	} timer;

	void handle_frame_timer(unsigned);

	Signal_dispatcher<Main> frame_timer_dispatcher = {
		sig_rec, *this, &Main::handle_frame_timer};

	Genode::Reporter hover_reporter = { "hover" };

	bool schedule_redraw = false;

	/**
	 * Frame of last call of 'handle_frame_timer'
	 */
	unsigned last_frame = 0;

	/**
	 * Number of frames between two redraws
	 */
	enum { REDRAW_PERIOD = 4 };

	/**
	 * Counter used for triggering redraws. Incremented in each frame-timer
	 * period, wraps at 'REDRAW_PERIOD'. The redraw is performed when the
	 * counter wraps.
	 */
	unsigned frame_cnt = 0;

	Main(Signal_receiver &sig_rec) : sig_rec(sig_rec)
	{
		dialog_rom.sigh(dialog_update_dispatcher);
		config()->sigh(config_dispatcher);

		nitpicker.input()->sigh(input_dispatcher);

		timer.sigh(frame_timer_dispatcher);

		/* apply initial configuration */
		handle_config(0);
	}
};


void Menu_view::Main::handle_dialog_update(unsigned)
{
	try {
		position = Decorator::point_attribute(config()->xml_node());
	} catch (...) { }

	dialog_rom.update();

	try {
		Xml_node dialog_xml(dialog_rom.local_addr<char>());

		root_widget.update(dialog_xml);
		root_widget.size(root_widget.min_size());
	} catch (...) {
		Genode::error("failed to construct widget tree");
	}

	schedule_redraw = true;

	/*
	 * If we have not processed a period for at least one frame, perform the
	 * processing immediately. This way, we avoid latencies when the dialog
	 * model is updated sporadically.
	 */
	if (timer.curr_frame() != last_frame)
		handle_frame_timer(0);
	else
		timer.schedule();
}


void Menu_view::Main::handle_config(unsigned)
{
	config()->reload();

	try {
		hover_reporter.enabled(config()->xml_node().sub_node("report")
		                                           .attribute_value("hover", false));
	} catch (...) {
		hover_reporter.enabled(false);
	}

	handle_dialog_update(0);
}


void Menu_view::Main::handle_input(unsigned)
{
	nitpicker.input()->for_each_event([&] (Input::Event const &ev) {
		if (ev.absolute_motion()) {

			Point const at = Point(ev.ax(), ev.ay()) - position;
			Widget::Unique_id const new_hovered = root_widget.hovered(at);

			if (hovered != new_hovered) {

				if (hover_reporter.enabled()) {
					Genode::Reporter::Xml_generator xml(hover_reporter, [&] () {
						root_widget.gen_hover_model(xml, at);
					});
				}

				hovered = new_hovered;
			}
		}

		/*
		 * Reset hover model when losing the focus
		 */
		if ((ev.type() == Input::Event::FOCUS && ev.code() == 0)
		 || (ev.type() == Input::Event::LEAVE)) {

			hovered = Widget::Unique_id();

			if (hover_reporter.enabled()) {
				Genode::Reporter::Xml_generator xml(hover_reporter, [&] () { });
			}
		}
	});
}


void Menu_view::Main::handle_frame_timer(unsigned)
{
	frame_cnt++;

	unsigned const curr_frame = timer.curr_frame();

	if (animator.active()) {

		unsigned const passed_frames = curr_frame - last_frame;

		if (passed_frames > 0) {

			for (unsigned i = 0; i < passed_frames; i++)
				animator.animate();

			schedule_redraw = true;
		}
	}

	last_frame = curr_frame;

	if (schedule_redraw && frame_cnt >= REDRAW_PERIOD) {

		frame_cnt = 0;

		Area const old_size = buffer.constructed() ? buffer->size() : Area();
		Area const size     = root_widget.min_size();

		if (!buffer.constructed() || size != old_size)
			buffer.construct(nitpicker, size, *env()->ram_session());
		else
			buffer->reset_surface();

		root_widget.size(size);
		root_widget.position(Point(0, 0));

		Surface<Pixel_rgb888> pixel_surface = buffer->pixel_surface();
		Surface<Pixel_alpha8> alpha_surface = buffer->alpha_surface();

		// XXX restrict redraw to dirty regions
		//     don't perform a full dialog update
		root_widget.draw(pixel_surface, alpha_surface, Point(0, 0));

		buffer->flush_surface();
		nitpicker.framebuffer()->refresh(0, 0, buffer->size().w(), buffer->size().h());
		_update_view();

		schedule_redraw = false;
	}

	/*
	 * Deactivate timer periods when idle, activate timer when an animation is
	 * in progress or a redraw is pending.
	 */
	bool const redraw_pending = schedule_redraw && frame_cnt != 0;

	if (animator.active() || redraw_pending)
		timer.schedule();
}


/*
 * Silence debug messages
 */
extern "C" void _sigprocmask() { }

int main(int argc, char **argv)
{
	static Genode::Signal_receiver sig_rec;

	static Menu_view::Main application(sig_rec);

	/* process incoming signals */
	for (;;) {
		using namespace Genode;

		Signal sig = sig_rec.wait_for_signal();
		Signal_dispatcher_base *dispatcher =
			dynamic_cast<Signal_dispatcher_base *>(sig.context());

		if (dispatcher)
			dispatcher->dispatch(sig.num());
	}
}
