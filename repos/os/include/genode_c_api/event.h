/*
 * \brief  C interface to Genode's event session
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2021-09-29
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__GENODE_C_API__EVENT_H_
#define _INCLUDE__GENODE_C_API__EVENT_H_

#include <genode_c_api/base.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize event-session handling
 */
void genode_event_init(struct genode_env *,
                       struct genode_allocator *);


struct genode_event;  /* definition is private to the implementation */


/****************************
 ** Event-session lifetime **
 ****************************/

struct genode_event_args
{
	char const *label;
};

struct genode_event *genode_event_create(struct genode_event_args const *);

void genode_event_destroy(struct genode_event *);


/**********************
 ** Event submission **
 **********************/

struct genode_event_touch_args
{
	unsigned finger;
	unsigned xpos;
	unsigned ypos;
	unsigned width;
};


struct genode_event_submit;


/**
 * Interface called by 'genode_event_generator_t' to submit events
 *
 * Note, keycode values must conform to os/include/input/keycodes.h.
 */
struct genode_event_submit
{
	void (*press) (struct genode_event_submit *, unsigned keycode);
	void (*release) (struct genode_event_submit *, unsigned keycode);

	void (*rel_motion) (struct genode_event_submit *, int x, int y);

	void (*abs_motion) (struct genode_event_submit *, int x, int y);

	void (*touch) (struct genode_event_submit *,
	               struct genode_event_touch_args const *);

	void (*touch_release) (struct genode_event_submit *, unsigned finger);

	void (*wheel) (struct genode_event_submit *, int x, int y);
};


/**
 * Context private to 'genode_event_generator_t'
 */
struct genode_event_generator_ctx;


typedef void (*genode_event_generator_t)
	(struct genode_event_generator_ctx *, struct genode_event_submit *);


/**
 * Generate batch of events
 */
void genode_event_generate(struct genode_event *,
                           genode_event_generator_t,
                           struct genode_event_generator_ctx *);

#ifdef __cplusplus
}
#endif

#endif /* _INCLUDE__GENODE_C_API__EVENT_H_ */
