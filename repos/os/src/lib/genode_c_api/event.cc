/*
 * \brief  C interface to Genode's event session
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2021-09-29
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <base/registry.h>
#include <base/log.h>
#include <base/session_label.h>
#include <event_session/connection.h>
#include <genode_c_api/event.h>

using namespace Genode;

struct Statics
{
	Env                                *env_ptr;
	Allocator                          *alloc_ptr;
	Registry<Registered<genode_event>>  event_sessions { };
};


static Statics & statics()
{
	static Statics instance { };

	return instance;
};


struct genode_event : private Noncopyable, private Interface
{
	private:

		Env       &_env;
		Allocator &_alloc;

		Session_label const _session_label;

		Event::Connection _connection { _env, _session_label.string() };

	public:

		genode_event(Env &env, Allocator &alloc,
		             Session_label const &session_label)
		:
			_env(env), _alloc(alloc), _session_label(session_label)
		{ }

		template <typename FN>
		void with_batch(FN const &fn)
		{
			_connection.with_batch(fn);
		}

};


void genode_event_init(genode_env       *env_ptr,
                       genode_allocator *alloc_ptr)
{
	statics().env_ptr   = env_ptr;
	statics().alloc_ptr = alloc_ptr;
}


namespace {

	struct Submit : genode_event_submit
	{
		Event::Session_client::Batch &batch;

		template <typename FN>
		static void _with_batch(struct genode_event_submit *myself, FN const &fn)
		{
			fn(static_cast<Submit *>(myself)->batch);
		}

		static void _press(struct genode_event_submit *myself, unsigned keycode)
		{
			_with_batch(myself, [&] (Event::Session_client::Batch &batch) {
				batch.submit(Input::Press { Input::Keycode(keycode) }); });
		}

		static void _release(struct genode_event_submit *myself, unsigned keycode)
		{
			_with_batch(myself, [&] (Event::Session_client::Batch &batch) {
				batch.submit(Input::Release { Input::Keycode(keycode) }); });
		}

		static void _rel_motion(struct genode_event_submit *myself, int x, int y)
		{
			_with_batch(myself, [&] (Event::Session_client::Batch &batch) {
				batch.submit(Input::Relative_motion { x, y }); });
		}

		static void _abs_motion(struct genode_event_submit *myself, int x, int y)
		{
			_with_batch(myself, [&] (Event::Session_client::Batch &batch) {
				batch.submit(Input::Absolute_motion { x, y }); });
		}

		static void _touch(struct genode_event_submit *myself,
		                   struct genode_event_touch_args const *args)
		{
			Input::Touch_id id { args->finger };

			_with_batch(myself, [&] (Event::Session_client::Batch &batch) {
				batch.submit(Input::Touch { id, (float)args->xpos, (float)args->ypos }); });
		}

		static void _touch_release(struct genode_event_submit *myself,
		                           unsigned finger)
		{
			Input::Touch_id id { finger };

			_with_batch(myself, [&] (Event::Session_client::Batch &batch) {
				batch.submit(Input::Touch_release { id }); });
		}

		static void _wheel(struct genode_event_submit *myself, int x, int y)
		{
			_with_batch(myself, [&] (Event::Session_client::Batch &batch) {
				batch.submit(Input::Wheel { x, y }); });
		}

		Submit(Event::Session_client::Batch &batch)
		:
			batch(batch)
		{
			press         = _press;
			release       = _release;
			rel_motion    = _rel_motion;
			abs_motion    = _abs_motion;
			touch         = _touch;
			touch_release = _touch_release;
			wheel         = _wheel;
		};

	};
}


void genode_event_generate(struct genode_event               *event_session,
                           genode_event_generator_t           generator_fn,
                           struct genode_event_generator_ctx *ctx)
{
	event_session->with_batch([&] (Event::Session_client::Batch &batch) {

		Submit submit { batch };

		(*generator_fn)(ctx, &submit);
	});
}


struct genode_event *genode_event_create(struct genode_event_args const *args)
{
	if (!statics().env_ptr || !statics().alloc_ptr) {
		error("genode_event_create: missing call of genode_event_init");
		return nullptr;
	}

	return new (*statics().alloc_ptr)
		Registered<genode_event>(statics().event_sessions,
		                         *statics().env_ptr, *statics().alloc_ptr,
		                         Session_label(args->label));
}


void genode_event_destroy(struct genode_event *event_ptr)
{
	destroy(*statics().alloc_ptr, event_ptr);
}
