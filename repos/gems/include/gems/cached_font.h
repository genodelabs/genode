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

#ifndef _INCLUDE__GEMS__CACHED_FONT_T_
#define _INCLUDE__GEMS__CACHED_FONT_T_

#include <base/output.h>
#include <base/allocator.h>
#include <nitpicker_gfx/text_painter.h>

namespace Genode { class Cached_font; }


class Genode::Cached_font : public Text_painter::Font
{
	public:

		struct Stats
		{
			unsigned misses;
			unsigned hits;
			unsigned consumed_bytes;

			void print(Output &out) const
			{
				Genode::print(out, "used: ", consumed_bytes/1024, " KiB, "
				              "hits: ", hits, ", misses: ", misses);
			}
		};

	private:

		typedef Text_painter::Area  Area;
		typedef Text_painter::Font  Font;
		typedef Text_painter::Glyph Glyph;

		struct Time { unsigned value; };

		Allocator    &_alloc;
		Font   const &_font;
		size_t const  _limit;
		Time  mutable _now { 0 };
		Stats mutable _stats { };

		class Cached_glyph : Avl_node<Cached_glyph>
		{
			private:

				friend class Avl_node<Cached_glyph>;
				friend class Avl_tree<Cached_glyph>;
				friend class Cached_font;

				Codepoint const _codepoint;
				Glyph     const _glyph;
				Time            _last_used;

				Glyph::Opacity _values[];

				bool _higher(Codepoint const other) const
				{
					return _codepoint.value > other.value;
				}

				unsigned _importance(Time now) const
				{
					return now.value - _last_used.value;
				}

			public:

				Cached_glyph(Codepoint c, Glyph const &glyph, Time now)
				:
					_codepoint(c),
					_glyph({ .width   = glyph.width,
					         .height  = glyph.height,
					         .vpos    = glyph.vpos,
					         .advance = glyph.advance,
					         .values  = _values }),
					_last_used(now)
				{
					for (unsigned i = 0; i < glyph.num_values(); i++)
						_values[i] = glyph.values[i];
				}

				/**
				 * Avl_node interface
				 */
				bool higher(Cached_glyph *c) { return _higher(c->_codepoint); }

				Cached_glyph *find_by_codepoint(Codepoint requested)
				{
					if (_codepoint.value == requested.value) return this;

					Cached_glyph *c = Avl_node<Cached_glyph>::child(_higher(requested));

					return c ? c->find_by_codepoint(requested) : nullptr;
				}

				Cached_glyph *find_least_recently_used(Time now)
				{
					Cached_glyph *result = this;

					for (unsigned i = 0; i < 2; i++) {
						Cached_glyph *c = Avl_node<Cached_glyph>::child(i);
						if (c && c->_importance(now) > result->_importance(now))
							result = c;
					}
					return result;
				}

				void mark_as_used(Time now) { _last_used = now; }

				void apply(Font::Apply_fn const &fn) const { fn.apply(_glyph); }
		};

		Avl_tree<Cached_glyph> mutable _avl_tree { };

		/**
		 * Size of one cache entry in bytes
		 */
		size_t const _alloc_size = sizeof(Cached_glyph)
		                         + 4*_font.bounding_box().count();

		/**
		 * Add cache entry for the given glyph
		 *
		 * \throw Out_of_ram
		 * \throw Out_of_caps
		 */
		void _insert(Codepoint codepoint, Glyph const &glyph)
		{
			auto const cached_glyph_ptr = (Cached_glyph *)_alloc.alloc(_alloc_size);

			_stats.consumed_bytes += _alloc_size;

			memset(cached_glyph_ptr, 0, _alloc_size);

			construct_at<Cached_glyph>(cached_glyph_ptr, codepoint, glyph, _now);

			_avl_tree.insert(cached_glyph_ptr);
		}

		/**
		 * Evict glyph from cache
		 */
		void _remove(Cached_glyph &glyph)
		{
			_avl_tree.remove(&glyph);

			glyph.~Cached_glyph();

			_alloc.free(&glyph, _alloc_size);

			_stats.consumed_bytes -= _alloc_size;
		}

		Cached_glyph *_find_by_codepoint(Codepoint codepoint)
		{
			if (!_avl_tree.first())
				return nullptr;

			return _avl_tree.first()->find_by_codepoint(codepoint);
		}

		/**
		 * Evice least recently used glyph from cache
		 *
		 * \return true if a glyph was released
		 */
		bool _remove_least_recently_used()
		{
			if (!_avl_tree.first())
				return false;

			Cached_glyph *glyph = _avl_tree.first()->find_least_recently_used(_now);
			if (!glyph)
				return false; /* this should never happen */

			_remove(*glyph);
			_stats.misses++;
			return true;
		}

		void _remove_all()
		{
			while (Cached_glyph *glyph_ptr = _avl_tree.first())
				_remove(*glyph_ptr);
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
			_alloc(alloc), _font(font), _limit(limit.value)
		{ }

		~Cached_font() { _remove_all(); }

		Stats stats() const { return _stats; }

		void _apply_glyph(Codepoint c, Apply_fn const &fn) const override
		{
			_now.value++;

			/*
			 * Try to lookup glyph from the cache. If it is missing, fill cache
			 * with requested glyph and repeat the lookup. When under memory
			 * pressure, flush least recently used glyphs from cache.
			 *
			 * Even though '_apply_glyph' is a const method, the internal cache
			 * and stats must of course be mutated. Hence the 'const_cast'.
			 */
			Cached_font &mutable_this = const_cast<Cached_font &>(*this);

			/* retry once after handling a cache miss */
			for (int i = 0; i < 2; i++) {

				if (Cached_glyph *glyph_ptr = mutable_this._find_by_codepoint(c)) {
					glyph_ptr->apply(fn);
					glyph_ptr->mark_as_used(_now);
					_stats.hits += (i == 0);
					return;
				}

				while (_stats.consumed_bytes + _alloc_size > _limit)
					if (!mutable_this._remove_least_recently_used())
						break;

				_font.apply_glyph(c, [&] (Glyph const &glyph) {
					mutable_this._insert(c, glyph); });
			}
		}

		Advance_info advance_info(Codepoint c) const override
		{
			unsigned                      width = 0;
			Text_painter::Fixpoint_number advance { 0 };

			/* go through the '_apply_glyph' cache-fill mechanism */
			Font::apply_glyph(c, [&] (Glyph const &glyph) {
				width = glyph.width, advance = glyph.advance; });

			return Advance_info { .width = width, .advance = advance };
		}

		unsigned baseline() const override { return _font.baseline(); }
		unsigned height()   const override { return _font.height(); }
		Area bounding_box() const override { return _font.bounding_box(); }
};

#endif /* _INCLUDE__GEMS__CACHED_FONT_T_ */
