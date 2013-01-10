/*
 * \brief  Genode C API framebuffer functions needed by Linux
 * \author Stefan Kalkowski
 * \date   2009-06-08
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__GENODE__FRAMEBUFFER_H_
#define _INCLUDE__GENODE__FRAMEBUFFER_H_


#include <l4/sys/compiler.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Returns the count of screens (framebuffer + input devices)
 * given by config-file information
 */
L4_CV int genode_screen_count(void);

/**
 * Get the size of the framebuffer
 *
 * \param screen  the index of the framebuffer
 * \return        the size
 */
L4_CV unsigned long genode_fb_size(unsigned int screen);

/**
 * Attach the framebuffer to our address space
 *
 * \param screen  the index of the framebuffer
 * \return        the virtual address the framebuffer was attached to
 */
L4_CV void *genode_fb_attach(unsigned int screen);

/**
 * Get information about the resolution of the framebuffer
 *
 * \param screen the index of the framebuffer
 * \param out_w  resulting pointer to the horizontal resolution info
 * \param out_h  resulting pointer to the vertical resolution info
 */
L4_CV void genode_fb_info(unsigned int screen, int *out_w, int *out_h);

/**
 * Refresh a specified area of the framebuffer
 *
 * \param screen the index of the framebuffer
 * \param x      x-coordinate of the area's position
 * \param y      y-coordinate of the area's position
 * \param w      width of the area
 * \param h      height of the area
 */
L4_CV void genode_fb_refresh(unsigned int screen, int x, int y, int w, int h);

/**
 * Close the framebuffer session
 *
 * \param screen  the index of the framebuffer
 */
L4_CV void genode_fb_close(unsigned screen);

//int genode_nit_view_create(unsigned screen, unsigned view);
//
//void genode_nit_view_destroy(unsigned screen, unsigned view);
//
//void genode_nit_view_back(unsigned screen, unsigned view);
//
//void genode_nit_view_place(unsigned screen, unsigned view, int x,
//                           int y, int w, int h);
//
//void genode_nit_view_stack(unsigned int screen, unsigned view,
//                           unsigned neighbor, int behind);
//
///**
// * Close all views of a nitpicker session
// *
// * \param screen  the index of the framebuffer
// */
//void genode_nit_close_all_views(unsigned screen);

#ifdef __cplusplus
}
#endif


#endif //_INCLUDE__GENODE__FRAMEBUFFER_H_
