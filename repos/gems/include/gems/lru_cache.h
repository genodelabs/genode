/*
 * \brief  Cache with a least-recently-used eviction policy
 * \author Norman Feske
 * \date   2019-01-12
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__GEMS__LRU_CACHE_H_
#define _INCLUDE__GEMS__LRU_CACHE_H_

#include <base/output.h>
#include <base/allocator.h>


namespace Genode { template <typename, typename> class Lru_cache; }


template <typename KEY, typename ELEM>
class Genode::Lru_cache : Noncopyable
{
	public:

		struct Stats
		{
			unsigned hits, evictions;

			void print(Output &out) const
			{
				Genode::print(out, "hits: ", hits, ", evictions: ", evictions);
			}
		};

	private:

		struct Time { unsigned value; };

		Allocator     &_alloc;
		unsigned const _max_elements;
		unsigned       _used_elements = 0;
		Time           _now { 0 };
		Stats          _stats { };

		class Tag
		{
			protected:

				KEY const _key;

				Time _last_used;

				Tag(KEY const &key, Time now) : _key(key), _last_used(now) { }
		};

		/*
		 * The '_key' and '_last_used' attributes are supplemented as the 'Tag'
		 * base class to the 'Element'instead of being 'Element' member
		 * variables to allow 'ELEM' to be at the trailing end of the object.
		 * This way, 'ELEM' can be a variable-length type (using a flexible
		 * array member).
		 */

		class Element : private Tag, private Avl_node<Element>, public ELEM
		{
			private:

				friend class Avl_node<Element>;
				friend class Avl_tree<Element>;
				friend class Lru_cache;

				bool _higher(KEY const &other) const
				{
					return Tag::_key.value > other.value;
				}

				unsigned _importance(Time now) const
				{
					return now.value - Tag::_last_used.value;
				}

			public:

				template <typename... ARGS>
				Element(KEY key, Time now, ARGS &&... args)
				: Tag(key, now), ELEM(args...) { }

				/**
				 * Avl_node interface
				 */
				bool higher(Element *e) { return _higher(e->_key); }

				template <typename FN>
				bool try_apply(KEY const &key, FN const &fn)
				{
					if (Tag::_key.value == key.value) {
						fn(*this);
						return true;
					}

					Element * const e = Avl_node<Element>::child(_higher(key));

					return e && e->try_apply(key, fn);
				}

				template <typename FN>
				void with_least_recently_used(Time now, FN const fn)
				{
					Element *result = this;

					for (unsigned i = 0; i < 2; i++) {
						Element *e = Avl_node<Element>::child(i);
						if (e && e->_importance(now) > result->_importance(now))
							result = e;
					}

					if (result)
						fn(*result);
				}

				void mark_as_used(Time now) { Tag::_last_used = now; }
		};

		Avl_tree<Element> mutable _avl_tree { };

		/**
		 * Add cache entry for the given key
		 *
		 * \param ARGS  constructor arguments passed to new ELEM
		 *
		 * \throw Out_of_ram
		 * \throw Out_of_caps
		 */
		template <typename... ARGS>
		void _insert(KEY key, ARGS &... args)
		{
			auto const element_ptr = (Element *)_alloc.alloc(sizeof(Element));

			_used_elements++;

			construct_at<Element>(element_ptr, key, _now, args...);

			_avl_tree.insert(element_ptr);
		}

		/**
		 * Evict element from cache
		 */
		void _remove(Element &element)
		{
			_avl_tree.remove(&element);

			element.~Element();

			_alloc.free(&element, sizeof(Element));

			_used_elements--;
			_stats.evictions++;
		}

		template <typename FN>
		bool _try_apply(KEY const &key, FN const &fn)
		{
			if (!_avl_tree.first())
				return false;

			return _avl_tree.first()->try_apply(key, fn);
		}

		/**
		 * Evict least recently used element from cache
		 *
		 * \return true if an element was released
		 */
		bool _remove_least_recently_used()
		{
			if (!_avl_tree.first())
				return false;

			_avl_tree.first()->with_least_recently_used(_now, [&] (Element &e) {
				_remove(e); });

			return true;
		}

		void _remove_all()
		{
			while (Element *element_ptr = _avl_tree.first())
				_remove(*element_ptr);
		}

	public:

		struct Size { size_t value; };

		/**
		 * Constructor
		 *
		 * \param alloc  backing store for cache elements
		 * \param size   maximum number of cache elements
		 */
		Lru_cache(Allocator &alloc, Size size)
		: _alloc(alloc), _max_elements(size.value) { }

		~Lru_cache() { _remove_all(); }

		/**
		 * Return size of a single cache entry including the meta data
		 *
		 * The returned value is useful for cache-dimensioning calculations.
		 */
		static constexpr size_t element_size() { return sizeof(Element); }

		/**
		 * Return usage stats
		 */
		Stats stats() const { return _stats; }

		/**
		 * Interface presented to the cache-miss handler to construct an
		 * element
		 */
		class Missing_element : Noncopyable
		{
			private:

				friend class Lru_cache;

				Lru_cache &_cache;
				KEY const &_key;

				Missing_element(Lru_cache &cache, KEY const &key)
				: _cache(cache), _key(key) { }

			public:

				/**
				 * Populate cache with new element
				 *
				 * \param ARGS  arguments passed to the 'ELEM' constructor
				 */
				template <typename... ARGS>
				void construct(ARGS &... args) { _cache._insert(_key, args...); }
		};

		/**
		 * Apply functor 'hit_fn' to element with matching 'key'
		 *
		 * \param miss_fn  cache-miss handler
		 *
		 * \return true  if 'hit_fn' got executed
		 *
		 * If the requested 'key' is not present in the cache, the cache-miss
		 * handler is called with the interface 'Missing_element &' as
		 * argument. By calling 'Missing_element::construct', the handler is
		 * able to fill the cache with the missing element. After resolving a
		 * cache miss, 'hit_fn' is called for the freshly inserted element.
		 *
		 * If an occurring cache miss is not handled, 'hit_fn' is not called.
		 */
		template <typename HIT_FN, typename MISS_FN>
		bool try_apply(KEY key, HIT_FN const &hit_fn, MISS_FN const &miss_fn)
		{
			_now.value++;

			/*
			 * Try to look up element from the cache. If it is missing, fill
			 * cache with requested element and repeat the lookup. When under
			 * memory pressure, evict the least recently used element from the
			 * cache.
			 */

			/* retry once after handling a cache miss */
			for (unsigned i = 0; i < 2; i++) {

				bool const hit = _try_apply(key, [&] (Element &element) {
					hit_fn(element);
					element.mark_as_used(_now);
					_stats.hits += (i == 0);
				});

				if (hit)
					return true;

				/*
				 * Handle cache miss
				 */

				/* evict element if the cache is fully populated */
				while (_used_elements >= _max_elements)
					if (!_remove_least_recently_used())
						break;

				/* fetch missing element into the cache */
				Missing_element missing_element { *this, key };
				miss_fn(missing_element);
			}

			return false;
		}
};

#endif /* _INCLUDE__GEMS__LRU_CACHE_H_ */
