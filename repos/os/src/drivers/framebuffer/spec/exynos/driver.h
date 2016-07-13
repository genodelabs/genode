/*
 * \brief  Framebuffer driver for Exynos5 HDMI
 * \author Martin Stein
 * \date   2013-08-09
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _DRIVER_H_
#define _DRIVER_H_

/* Genode includes */
#include <base/stdint.h>
#include <base/log.h>
#include <os/server.h>

namespace Framebuffer
{
	using namespace Genode;

	/**
	 * Framebuffer driver
	 */
	class Driver;
}

class Framebuffer::Driver
{
	public:

		enum Format { FORMAT_RGB565 };
		enum Output { OUTPUT_LCD, OUTPUT_HDMI };

	private:

		size_t _fb_width;
		size_t _fb_height;
		Format _fb_format;

		/**
		 * Driver-initialization backend for output HDMI
		 *
		 * \param fb_phys  physical base of framebuffer
		 *
		 * \retval -1  failed
		 * \retval  0  succeeded
		 */
		int _init_hdmi(addr_t fb_phys);

	public:

		/**
		 * Constructor
		 */
		Driver()
		:
			_fb_width(0),
			_fb_height(0),
			_fb_format(FORMAT_RGB565)
		{ }

		/**
		 * Return amount of bytes that is used for one pixel descriptor
		 *
		 * \param format  pixel format
		 *
		 * \retval  0  failed
		 * \retval >0  succeeded
		 */
		static size_t bytes_per_pixel(Format format)
		{
			switch (format) {
			case FORMAT_RGB565:
				return 2;
			default:
				error("unknown pixel format");
				return 0;
			}
		}

		/**
		 * Return size of framebuffer in bytes
		 *
		 * \param width   width of framebuffer in pixel
		 * \param height  height of framebuffer in pixel
		 * \param format  pixel format of framebuffer
		 *
		 * \retval  0  failed
		 * \retval >0  succeeded
		 */
		size_t buffer_size(size_t width, size_t height, Format format)
		{
			return bytes_per_pixel(format) * width * height;
		}

		/**
		 * Initialize driver
		 *
		 * \param width    width of screen and framebuffer in pixel
		 * \param height   height of screen and framebuffer in pixel
		 * \param format   pixel format of framebuffer
		 * \param output   selected output device
		 * \param fb_phys  physical base of framebuffer
		 *
		 * \retval -1  failed
		 * \retval  0  succeeded
		 */
		int init_drv(size_t width, size_t height, Format format,
		             Output output, addr_t fb_phys);
};

#endif /* _DRIVER_H_ */
