/*
 * \brief  Glyph cache
 * \author Norman Feske
 * \date   2018-03-27
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__GEMS__CACHED_FONT_H_
#define _INCLUDE__GEMS__CACHED_FONT_H_

#include <gems/lru_cache.h>
#include <nitpicker_gfx/text_painter.h>

namespace Genode { class Cached_font; }


class Genode::Cached_font : public Text_painter::Font
{
	private:

		typedef Text_painter::Area  Area;
		typedef Text_painter::Font  Font;
		typedef Text_painter::Glyph Glyph;

		struct Cached_glyph : Glyph, Noncopyable
		{
			Glyph::Opacity _values[];

			/*
			 * The number of values is not statically known but runtime-
			 * dependent. The values are stored directly after the
			 * 'Cached_glyph' object.
			 */

			Cached_glyph(Glyph const &glyph)
			:
				Glyph({ .width   = glyph.width,
				        .height  = glyph.height,
				        .vpos    = glyph.vpos,
				        .advance = glyph.advance,
				        .values  = _values })
			{
				for (unsigned i = 0; i < glyph.num_values(); i++)
					_values[i] = glyph.values[i];
			}
		};

		Font const &_font;

		size_t const _opacity_values_size = 4*_font.bounding_box().count();

		/**
		 * Allocator wrapper that inflates each allocation with a byte padding
		 */
		struct Padding_allocator : Allocator
		{
			size_t const _padding_bytes;

			Allocator &_alloc;

			size_t _consumed_bytes = 0;

			size_t _padded(size_t size) const { return size + _padding_bytes; }

			Padding_allocator(Allocator &alloc, size_t padding_bytes)
			: _padding_bytes(padding_bytes), _alloc(alloc) { }

			size_t consumed_bytes() const { return _consumed_bytes; }

			bool alloc(size_t size, void **out_addr) override
			{
				size = _padded(size);

				bool const result = _alloc.alloc(size, out_addr);

				if (result) {
					memset(*out_addr, 0, size);
					_consumed_bytes += size + overhead(size);
				}

				return result;
			}

			size_t consumed() const override { return _alloc.consumed(); }

			size_t overhead(size_t size) const override { return _alloc.overhead(size); };

			void free(void *addr, size_t size) override
			{
				size = _padded(size);

				_alloc.free(addr, size);
				_consumed_bytes -= size + overhead(size);
			}

			bool need_size_for_free() const override { return _alloc.need_size_for_free(); }
		};

		Padding_allocator _padding_alloc;

		typedef Lru_cache<Codepoint, Cached_glyph> Cache;

		Cache mutable _cache;

		/**
		 * Return number of cache elements that fit in 'avail_bytes'
		 */
		Cache::Size _cache_size(size_t const avail_bytes)
		{
			size_t const element_size = Cache::element_size() + _opacity_values_size;

			size_t const bytes_per_element = element_size + _padding_alloc.overhead(element_size);

			/* bytes_per_element can never be zero */
			return Cache::Size { avail_bytes / bytes_per_element };
		}

	public:

		struct Limit { size_t value; };

		/**
		 * Constructor
		 *
		 * \param alloc  backing store for cached glyphs
		 * \param font   original (uncached) font
		 * \param limit  maximum cache size in bytes
		 */
		Cached_font(Allocator &alloc, Font const &font, Limit limit)
		:
			_font(font),
			_padding_alloc(alloc, _opacity_values_size),
			_cache(_padding_alloc, _cache_size(limit.value))
		{ }

		struct Stats
		{
			Cache::Stats cache_stats;
			size_t       consumed_bytes;

			void print(Output &out) const
			{
				Genode::print(out, "used: ", consumed_bytes/1024, " KiB, ", cache_stats);
			}
		};

		Stats stats() const
		{
			return Stats { _cache.stats(), _padding_alloc.consumed_bytes() };
		}

		void _apply_glyph(Codepoint c, Apply_fn const &fn) const override
		{
			auto hit_fn  = [&] (Glyph const &glyph) { fn.apply(glyph); };

			auto miss_fn = [&] (Cache::Missing_element &missing_element)
			{
				_font.apply_glyph(c, [&] (Glyph const &glyph) {
					missing_element.construct(glyph); });
			};

			(void)_cache.try_apply(c, hit_fn, miss_fn);
		}

		Advance_info advance_info(Codepoint c) const override
		{
			unsigned                      width = 0;
			Text_painter::Fixpoint_number advance { 0 };

			Font::apply_glyph(c, [&] (Glyph const &glyph) {
				width = glyph.width, advance = glyph.advance; });

			return Advance_info { .width = width, .advance = advance };
		}

		unsigned baseline() const override { return _font.baseline(); }
		unsigned height()   const override { return _font.height(); }
		Area bounding_box() const override { return _font.bounding_box(); }
};

#endif /* _INCLUDE__GEMS__CACHED_FONT_H_ */
