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

/* local includes */
#include <chunky_texture.h>

namespace Nitpicker { template <typename> class Resizeable_texture; }


template <typename PT>
class Nitpicker::Resizeable_texture
{
	private:

		unsigned _current = 0;

		Constructible<Chunky_texture<PT>> _textures[2] { };

		static void _with_texture(auto &obj, auto const &fn)
		{
			if (obj.valid())
				fn(*obj._textures[obj._current]);
		}

	public:

		Point panning { 0, 0 };

		bool valid() const { return _textures[_current].constructed(); }

		Area size() const
		{
			return valid() ? _textures[_current]->Texture_base::size()
			               : Area { };
		}

		bool alpha() const { return valid() && _textures[_current]->alpha(); }

		void release_current() { _textures[_current].destruct(); }

		bool try_construct_next(Ram_allocator &ram, Region_map &rm, Framebuffer::Mode mode)
		{
			try {
				unsigned const next = !_current;
				_textures[next].construct(ram, rm, mode);
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

		void with_texture(auto const &fn) const { _with_texture(*this, fn); }
		void with_texture(auto const &fn)       { _with_texture(*this, fn); }

		Dataspace_capability dataspace()
		{
			return valid() ? _textures[_current]->cap() : Ram_dataspace_capability();
		}

		void with_input_mask(auto const &fn) const
		{
			if (valid())
				_textures[_current]->with_input_mask(fn);
		}

		void blit(Rect from, Point to)
		{
			with_texture([&] (Chunky_texture<PT> &texture) {
				texture.blit(from, to); });
		}
};

#endif /* _RESIZEABLE_TEXTURE_H_ */
