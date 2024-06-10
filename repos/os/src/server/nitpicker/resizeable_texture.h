/*
 * \brief  Texture that preserves content across resize
 * \author Norman Feske
 * \date   2020-07-02
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _RESIZEABLE_TEXTURE_H_
#define _RESIZEABLE_TEXTURE_H_

/* Genode includes */
#include <blit/painter.h>
#include <os/pixel_alpha8.h>

/* local includes */
#include <chunky_texture.h>

namespace Nitpicker { template <typename> class Resizeable_texture; }


template <typename PT>
class Nitpicker::Resizeable_texture
{
	private:

		unsigned _current = 0;

		using Constructible_texture = Constructible<Chunky_texture<PT>>;

		struct Element : Constructible_texture
		{
			Element() : Constructible_texture() { }
		};

		Element _textures[2];

	public:

		bool valid() const { return _textures[_current].constructed(); }

		Area size() const
		{
			return valid() ? _textures[_current]->Texture_base::size()
			               : Area { };
		}

		void release_current() { _textures[_current].destruct(); }

		bool try_construct_next(Ram_allocator &ram, Region_map &rm,
		                        Area size, bool use_alpha)
		{
			try {
				unsigned const next = !_current;
				_textures[next].construct(ram, rm, size, use_alpha);
				return true;
			} catch (...) { }
			return false;
		}

		/**
		 * Make the next texture the current one
		 *
		 * This method destructs the previous current one.
		 */
		void switch_to_next()
		{
			unsigned const next = !_current;

			/* copy content from current to next texture */
			if (_textures[next].constructed() && _textures[_current].constructed()) {

				Surface<PT> surface(_textures[next]->pixel(),
				                    _textures[next]->Texture_base::size());

				Texture<PT> const &texture = *_textures[_current];

				Blit_painter::paint(surface, texture, Point(0, 0));

				/* copy alpha channel */
				if (_textures[_current]->alpha() && _textures[next]->alpha()) {

					using AT = Pixel_alpha8;

					Surface<AT> surface((AT *)_textures[next]->alpha(),
					                    _textures[next]->Texture_base::size());

					Texture<AT> const texture((AT *)_textures[_current]->alpha(), nullptr,
					                          _textures[_current]->Texture_base::size());

					Blit_painter::paint(surface, texture, Point(0, 0));
				}
			}

			_textures[_current].destruct();

			_current = next;
		}

		template <typename FN>
		void with_texture(FN const &fn) const
		{
			if (valid())
				fn(*_textures[_current]);
		}

		Dataspace_capability dataspace()
		{
			return valid() ? _textures[_current]->ds_cap() : Ram_dataspace_capability();
		}

		unsigned char const *input_mask_buffer() const
		{
			return valid() ? _textures[_current]->input_mask_buffer()
			               : nullptr;
		}
};

#endif /* _RESIZEABLE_TEXTURE_H_ */
