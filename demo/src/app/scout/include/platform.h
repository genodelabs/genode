/*
 * \brief   Platform abstraction
 * \date    2005-10-24
 * \author  Norman Feske <norman.feske@genode-labs.com>
 *
 * This interface specifies the target-platform-specific functions.
 */

/*
 * Copyright (C) 2005-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#include "event.h"

/*
 * We use two buffers, a foreground buffer that is displayed on screen and a
 * back buffer. While the foreground buffer must contain valid data all the
 * time, the back buffer can be used to prepare pixel data. For example,
 * drawing multiple pixel layers with alpha channel must be done in the back
 * buffer to avoid artifacts on the screen.
 */
class Screen_update
{
	public:

		virtual ~Screen_update() { }

		/**
		 * Request screen base address
		 */
		virtual void *scr_adr() = 0;

		/**
		 * Request back buffer address
		 */
		virtual void *buf_adr() { return scr_adr(); }

		/**
		 * Flip fore and back buffers
		 */
		virtual void flip_buf_scr() { }

		/**
		 * Copy background buffer to foreground
		 */
		virtual void copy_buf_to_scr(int x, int y, int w, int h) { }

		/**
		 * Flush pixels of specified screen area
		 */
		virtual void scr_update(int x, int y, int w, int h) = 0;
};


class Platform : public Screen_update
{
	private:

		int _max_vw, _max_vh;    /* maximum view size */

	public:

		enum pixel_format {
			UNDEFINED = 0,
			RGB565    = 1,
		};

		/**
		 * Constructor - initialize platform
		 *
		 * \param vx,vy   initial view position
		 * \param vw,vw   initial view width and height
		 * \param max_vw  maximum view width
		 *
		 * When using the default value for 'max_vw', the window's
		 * max width will correspond to the screen size.
		 */
		Platform(unsigned vx, unsigned vy, unsigned vw, unsigned vh,
		         unsigned max_vw = 0, unsigned max_vh = 0);

		/**
		 * Check initialization state of the platform
		 *
		 * \retval 1 platform was successfully initialized
		 * \retval 0 platform initialization failed
		 */
		int initialized();

		/**
		 * Request screen width and height
		 */
		int scr_w();
		int scr_h();

		/**
		 * Request pixel format
		 */
		pixel_format scr_pixel_format();

		/**
		 * Define geometry of viewport on screen
		 *
		 * The specified area is relative to the screen
		 * of the platform.
		 */
		void view_geometry(int x, int y, int w, int h, int do_redraw = 0,
		                   int buf_x = 0, int buf_y = 0);

		/**
		 * Bring Scouts view ontop
		 */
		void top_view();

		/**
		 * View geometry accessor functions
		 */
		int vx();
		int vy();
		int vw();
		int vh();
		int vbx();
		int vby();

		/**
		 * Get timer ticks in miilliseconds
		 */
		unsigned long timer_ticks();

		/**
		 * Request if an event is pending
		 *
		 * \retval 1  event is pending
		 * \retval 0  no event pending
		 */
		int event_pending();

		/**
		 * Request event
		 *
		 * \param e  destination where to store event information.
		 *
		 * If there is no event pending, this function blocks
		 * until there is an event to deliver.
		 */
		void get_event(Event *out_e);

		/**
		 * Screen update interface
		 */
		void *scr_adr();
		void *buf_adr();
		void  flip_buf_scr();
		void  copy_buf_to_scr(int x, int y, int w, int h);
		void  scr_update(int x, int y, int w, int h);

};

#endif /* _PLATFORM_H_ */
