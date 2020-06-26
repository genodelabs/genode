/*
 * \brief  Capture session component
 * \author Norman Feske
 * \date   2020-06-27
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CAPTURE_SESSION_H_
#define _CAPTURE_SESSION_H_

/* Genode includes */
#include <base/session_object.h>
#include <capture_session/capture_session.h>

namespace Nitpicker { class Capture_session; }


class Nitpicker::Capture_session : public Session_object<Capture::Session>
{
	public:

		struct Handler : Interface
		{
			/**
			 * Prompt nitpicker to adjust the screen size depending on all
			 * present capture buffers.
			 */
			virtual void capture_buffer_size_changed() = 0;
		};

	private:

		Env &_env;

		Constrained_ram_allocator _ram;

		Handler &_handler;

		View_stack const &_view_stack;

		Area _buffer_size { };

		Constructible<Attached_ram_dataspace> _buffer { };

		Signal_context_capability _screen_size_sigh { };

		using Dirty_rect = Genode::Dirty_rect<Rect, Affected_rects::NUM_RECTS>;

		Dirty_rect _dirty_rect { };

	public:

		Capture_session(Env              &env,
		                Resources  const &resources,
		                Label      const &label,
		                Diag       const &diag,
		                Handler          &handler,
		                View_stack const &view_stack)
		:
			Session_object(env.ep(), resources, label, diag),
			_env(env),
			_ram(env.ram(), _ram_quota_guard(), _cap_quota_guard()),
			_handler(handler),
			_view_stack(view_stack)
		{
			_dirty_rect.mark_as_dirty(Rect(Point(0, 0), view_stack.size()));
		}

		~Capture_session() { }


		/*****************************************
		 ** Interface used by 'Nitpicker::Main' **
		 *****************************************/

		Area buffer_size() const { return _buffer_size; }

		void mark_as_damaged(Rect rect)
		{
			// XXX not called yet
			_dirty_rect.mark_as_dirty(rect);
		}

		void screen_size_changed()
		{
			if (_screen_size_sigh.valid())
				Signal_transmitter(_screen_size_sigh).submit();
		}


		/*******************************
		 ** Capture session interface **
		 *******************************/

		Area screen_size() const override { return _view_stack.size(); }

		void screen_size_sigh(Signal_context_capability sigh) override
		{
			_screen_size_sigh = sigh;
		}

		void buffer(Area size) override
		{
			_buffer_size = Area { };

			if (size.count() == 0) {
				_buffer.destruct();
				return;
			}

			try {
				_buffer.construct(_ram, _env.rm(), buffer_bytes(size));
				_buffer_size = size;
				_handler.capture_buffer_size_changed();
			} catch (...) {
				_handler.capture_buffer_size_changed();
				throw;
			}
		}

		Dataspace_capability dataspace() override
		{
			if (_buffer.constructed())
				return _buffer->cap();

			return Dataspace_capability();
		}

		Affected_rects capture_at(Point pos) override
		{
			if (!_buffer.constructed())
				return Affected_rects { };

			using Pixel = Pixel_rgb888;

			Canvas<Pixel> canvas = { _buffer->local_addr<Pixel>(), pos, _buffer_size };

			Rect const buffer_rect(Point(0, 0), _buffer_size);

			Affected_rects affected { };
			unsigned i = 0;
			_dirty_rect.flush([&] (Rect const &rect) {

				_view_stack.draw(canvas, rect);

				if (i < Affected_rects::NUM_RECTS) {
					Rect const translated(rect.p1() - pos, rect.area());
					Rect const clipped = Rect::intersect(translated, buffer_rect);
					affected.rects[i++] = clipped;
				}
			});

			return affected;
		}
};

#endif /* _CAPTURE_SESSION_H_ */
