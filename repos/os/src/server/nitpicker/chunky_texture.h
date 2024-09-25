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
#include <blit/painter.h>

/* local includes */
#include <buffer.h>

namespace Nitpicker { template <typename> class Chunky_texture; }


template <typename PT>
class Nitpicker::Chunky_texture : Buffer, public Texture<PT>
{
	private:

		Framebuffer::Mode const _mode;

		/**
		 * Return base address of alpha channel or 0 if no alpha channel exists
		 */
		static uint8_t *_alpha_base(Buffer &buffer, Framebuffer::Mode mode)
		{
			uint8_t *result = nullptr;
			mode.with_alpha_bytes(buffer, [&] (Byte_range_ptr const &bytes) {
				result = (uint8_t *)bytes.start; });
			return result;
		}

		void _with_alpha_texture(auto const &fn) const
		{
			Buffer const &buffer = *this;
			_mode.with_alpha_bytes(buffer, [&] (Byte_range_ptr const &bytes) {
				Texture<Pixel_alpha8> texture { (Pixel_alpha8 *)bytes.start, nullptr, _mode.area };
				fn(texture); });
		}

		void _with_input_texture(auto const &fn) const
		{
			Buffer const &buffer = *this;
			_mode.with_input_bytes(buffer, [&] (Byte_range_ptr const &bytes) {
				Texture<Pixel_input8> texture { (Pixel_input8 *)bytes.start, nullptr, _mode.area };
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

		using Buffer::cap;

		Chunky_texture(Ram_allocator &ram, Region_map &rm, Framebuffer::Mode mode)
		:
			Buffer(ram, rm, mode.num_bytes()),
			Texture<PT>((PT *)Buffer::bytes().start, _alpha_base(*this, mode), mode.area),
			_mode(mode)
		{ }

		void with_input_mask(auto const &fn) const
		{
			Buffer const &buffer = *this;
			_mode.with_input_bytes(buffer, [&] (Byte_range_ptr const &bytes) {
				Const_byte_range_ptr const_bytes { bytes.start, bytes.num_bytes };
				fn(const_bytes); });
		}

		void blit(Rect from, Point to)
		{
			Buffer &buffer = *this;

			_mode.with_pixel_surface(buffer, [&] (Surface<Pixel_rgb888> &surface) {
				_blit_channel(surface, *this, from, to); });

			_mode.with_alpha_surface(buffer, [&] (Surface<Pixel_alpha8> &surface) {
				_with_alpha_texture([&] (Texture<Pixel_alpha8> &texture) {
					_blit_channel(surface, texture, from, to); }); });

			_mode.with_input_surface(buffer, [&] (Surface<Pixel_input8> &surface) {
				_with_input_texture([&] (Texture<Pixel_input8> &texture) {
					_blit_channel(surface, texture, from, to); }); });
		}
};

#endif /* _CHUNKY_TEXTURE_H_ */
