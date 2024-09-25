/*
 * \brief  Texture allocated as RAM dataspace
 * \author Norman Feske
 * \date   2017-11-16
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CHUNKY_TEXTURE_H_
#define _CHUNKY_TEXTURE_H_

/* Genode includes */
#include <os/pixel_rgb888.h>
#include <os/pixel_alpha8.h>
#include <blit/painter.h>

/* local includes */
#include <buffer.h>

namespace Nitpicker { template <typename> class Chunky_texture; }


template <typename PT>
class Nitpicker::Chunky_texture : public Buffer, public Texture<PT>
{
	private:

		/**
		 * Return base address of alpha channel or 0 if no alpha channel exists
		 */
		unsigned char *_alpha_base(Framebuffer::Mode mode)
		{
			if (!mode.alpha) return nullptr;

			/* alpha values come right after the pixel values */
			return (unsigned char *)local_addr()
			     + calc_num_bytes({ .area = mode.area, .alpha = false });
		}

		Area _area() const { return Texture<PT>::size(); }

		void _with_pixel_surface(auto const &fn)
		{
			Surface<Pixel_rgb888> pixel { (Pixel_rgb888 *)local_addr(), _area() };
			fn(pixel);
		}

		static void _with_alpha_ptr(auto &obj, auto const &fn)
		{
			Pixel_alpha8 * const ptr = (Pixel_alpha8 *)(obj.Texture<PT>::alpha());
			if (ptr)
				fn(ptr);
		}

		void _with_alpha_surface(auto const &fn)
		{
			_with_alpha_ptr(*this, [&] (Pixel_alpha8 * const ptr) {
				Surface<Pixel_alpha8> alpha { ptr, _area() };
				fn(alpha); });
		}

		void _with_alpha_texture(auto const &fn) const
		{
			_with_alpha_ptr(*this, [&] (Pixel_alpha8 * const ptr) {
				Texture<Pixel_alpha8> texture { ptr, nullptr, _area() };
				fn(texture); });
		}

		static void _with_input_ptr(auto &obj, auto const &fn)
		{
			Pixel_alpha8 * const ptr = (Pixel_alpha8 *)(obj.input_mask_buffer());
			if (ptr)
				fn(ptr);
		}

		void _with_input_surface(auto const &fn)
		{
			_with_input_ptr(*this, [&] (Pixel_alpha8 * const ptr) {
				Surface<Pixel_alpha8> input { ptr, _area() };
				fn(input); });
		}

		void _with_input_texture(auto const &fn) const
		{
			_with_input_ptr(*this, [&] (Pixel_alpha8 * const ptr) {
				Texture<Pixel_alpha8> texture { ptr, nullptr, _area() };
				fn(texture); });
		}

		template <typename T>
		void _blit_channel(Surface<T> &surface, Texture<T> const &texture,
		                   Rect const from, Point const to)
		{
			surface.clip({ to, from.area });
			Blit_painter::paint(surface, texture, to - from.p1());
		}

	public:

		/**
		 * Constructor
		 */
		Chunky_texture(Ram_allocator &ram, Region_map &rm, Framebuffer::Mode mode)
		:
			Buffer(ram, rm, mode.area, calc_num_bytes(mode)),
			Texture<PT>((PT *)local_addr(), _alpha_base(mode), mode.area)
		{ }

		static size_t calc_num_bytes(Framebuffer::Mode mode)
		{
			/*
			 * If using an alpha channel, the alpha buffer follows the
			 * pixel buffer. The alpha buffer is followed by an input
			 * mask buffer. Hence, we have to account one byte per
			 * alpha value and one byte for the input mask value.
			 */
			size_t bytes_per_pixel = sizeof(PT) + (mode.alpha ? 2 : 0);
			return bytes_per_pixel*mode.area.count();
		}

		uint8_t const *input_mask_buffer() const
		{
			if (!Texture<PT>::alpha()) return 0;

			/* input-mask values come right after the alpha values */
			Framebuffer::Mode const mode { .area = _area(), .alpha = false };
			return (uint8_t const *)local_addr() + calc_num_bytes(mode) + _area().count();
		}

		void blit(Rect from, Point to)
		{
			_with_pixel_surface([&] (Surface<Pixel_rgb888> &surface) {
				_blit_channel(surface, *this, from, to); });

			_with_alpha_surface([&] (Surface<Pixel_alpha8> &surface) {
				_with_alpha_texture([&] (Texture<Pixel_alpha8> &texture) {
					_blit_channel(surface, texture, from, to); }); });

			_with_input_surface([&] (Surface<Pixel_alpha8> &surface) {
				_with_input_texture([&] (Texture<Pixel_alpha8> &texture) {
					_blit_channel(surface, texture, from, to); }); });
		}
};

#endif /* _CHUNKY_TEXTURE_H_ */
