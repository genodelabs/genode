/*
 * \brief  Common base of polygon painters
 * \author Norman Feske
 * \date   2015-06-19
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__POLYGON_GFX__POLYGON_PAINTER_BASE_H_
#define _INCLUDE__POLYGON_GFX__POLYGON_PAINTER_BASE_H_

/* Genode includes */
#include <base/allocator.h>
#include <util/geometry.h>
#include <util/misc_math.h>
#include <polygon_gfx/clipping.h>

namespace Polygon { class Painter_base; }


class Polygon::Painter_base
{
	private:

		/**
		 * Interpolate linearly between start value and end value
		 */
		static inline void _interpolate(int start, int end, int *dst,
		                                unsigned num_values)
		{
			/* sanity check */
			if (num_values == 0) return;

			int const ascent = ((end - start)<<16)/(int)num_values;

			for (int curr = start<<16; num_values--; curr += ascent)
				*dst++ = curr>>16;
		}

		/**
		 * Clip polygon against boundary
		 *
		 * \param src             Array of unclipped source-polygon points
		 * \param src_num_points  Number of source-polygon points
		 * \param dst             Destination buffer for clipped polygon points
		 * \param clip            Clipping boundary
		 * \return                Number of resulting polygon points
		 */
		template <typename CLIPPER, typename POINT>
		static inline int _clip_1d(POINT const *src, unsigned src_num_points,
		                           POINT *dst, int clip)
		{
			/*
			 * Walk along the polygon edges. Each time when crossing the
			 * clipping border, a new polygon point is created at the
			 * intersection point. All polygon points outside the clipping area
			 * are discarded.
			 */
			unsigned dst_num_points = 0;
			for (unsigned i = 0; i < src_num_points; i++) {

				POINT curr = *src++;
				POINT next = *src;

				bool curr_inside = CLIPPER::inside(curr, clip);
				bool next_inside = CLIPPER::inside(next, clip);

				/* add current point to resulting polygon if inside clipping area */
				if (curr_inside)
					dst[dst_num_points++] = curr;

				/* add point of intersection when walking outside of the clipping area */
				if (curr_inside && !next_inside)
					dst[dst_num_points++] = CLIPPER::clip(curr, next, clip);

				/* add point of intersection when walking inside to the clipping area */
				if (!curr_inside && next_inside)
					dst[dst_num_points++] = CLIPPER::clip(curr, next, clip);
			}

			/* store coordinates of the first point also at the end of the polygon */
			dst[dst_num_points] = dst[0];

			return dst_num_points;
		}

	public:

		typedef Genode::Rect<> Rect;
		typedef Genode::Area<> Area;

		/**
		 * Buffers used for storing interpolated attribute values along a left
		 * or right polygon edges.
		 *
		 * The edge buffers are partitioned into subsequent sub buffers with a
		 * size corresponding to the maximum y range of the polygon, which is
		 * the surface height. The different sub buffers are used to hold the
		 * interpolated edge values for the different polygon-point attributes.
		 *
		 * \param N  number of attributes
		 */
		template <unsigned N>
		class Edge_buffers
		{
			private:

				Genode::Allocator &_alloc;

				unsigned const _edge_len;

				Genode::size_t _edges_size() { return N*2*_edge_len*sizeof(int); }

				int * const _edges = (int *)_alloc.alloc(_edges_size());

			public:

				Edge_buffers(Genode::Allocator &alloc, unsigned edge_len)
				:
					_alloc(alloc), _edge_len(edge_len)
				{ }

				~Edge_buffers() { _alloc.free(_edges, _edges_size()); }

				/**
				 * Return size of a single edge buffer
				 */
				unsigned edge_len() const { return _edge_len; }

				/**
				 * Return left edge buffer for Nth attribute
				 */
				int *left(unsigned n) { return _edges + n*2*_edge_len; }

				/**
				 * Return right edge buffer for Nth attribute
				 */
				int *right(unsigned n) { return _edges + (n*2 + 1)*_edge_len; }
		};

		/**
		 * Calculate maximum number of points needed for clipped polygon
		 *
		 * When clipping the polygon with 'num_points' points against
		 * the canvas boundaries, the resulting polygon will have a
		 * maximum of 'num_points' + 4 points - in the worst case one
		 * additional point per canvas edge.  To ease the further
		 * processing, we add the coordinates of the first point as an
		 * additional point to the clipped polygon.
		 */
		static unsigned max_points_clipped(unsigned num_points)
		{
			return num_points + 4 + 1;
		}

		/**
		 * Clip polygon against clipping rectangle
		 *
		 * \param src_points  Array of unclipped polygon points
		 * \param num_points  Number of unclipped polygon points
		 * \param dst_points  Buffer for storing the clipped polygon
		 * \return            Number of points of resulting polygon
		 *
		 * During the clipping computation, the destination buffer 'dst_points'
		 * is used to store two polygons. Therefore, the buffer must be
		 * dimensioned at 2*'max_points_clipped()'. The end result of the
		 * computation is stored at the beginning of 'dst_points'.
		 */
		template <typename POINT>
		static int clip_polygon(POINT const *src_points, unsigned num_points,
		                        POINT *dst_points, Rect clip)
		{
			POINT *c0 = dst_points,
			      *c1 = dst_points + max_points_clipped(num_points);

			for (unsigned i = 0; i < num_points; i++)
				c0[i] = src_points[i];

			/* last point is connected to the first point */
			c0[num_points] = c0[0];

			/* clip against top, left, bottom, and right clipping boundaries */
			typedef Clipper_2d<POINT> Clipper;
			num_points = _clip_1d<typename Clipper::Top>   (c0, num_points, c1, clip.y1());
			num_points = _clip_1d<typename Clipper::Left>  (c1, num_points, c0, clip.x1());
			num_points = _clip_1d<typename Clipper::Bottom>(c0, num_points, c1, clip.y2());
			num_points = _clip_1d<typename Clipper::Right> (c1, num_points, c0, clip.x2());

			return num_points;
		}


		/**
		 * Determine bounding box of the specified polygon points
		 *
		 * \height maximum bounds
		 */
		template <typename POINT>
		static Rect bounding_box(POINT const points[], int num_points, Area area)
		{
			int x_min = area.w() - 1, x_max = 0;
			int y_min = area.h() - 1, y_max = 0;

			for (int i = 0; i < num_points; i++) {
				x_min = Genode::min(x_min, points[i].x());
				x_max = Genode::max(x_max, points[i].x());
				y_min = Genode::min(y_min, points[i].y());
				y_max = Genode::max(y_max, points[i].y());
			}

			return Rect(Point_base(x_min, y_min), Point_base(x_max, y_max));
		}


		/**
		 * Calculate edge buffers for a polygon
		 *
		 * \param N  number of edge attributes
		 */
		template <unsigned N, typename POINT>
		void fill_edge_buffers(Edge_buffers<N> &edges,
		                       POINT points[], unsigned num_points)
		{
			/* for each edge attribute */
			for (unsigned i = 0; i < N; i++) {

				int * const l_edge = edges.left(i);
				int * const r_edge = edges.right(i);

				for (unsigned j = 0; j < num_points; j++) {

					POINT const p1 = points[j];
					POINT const p2 = points[j + 1];

					/* request attribute values to interpolate */
					int const p1_attr = p1.edge_attr(i);
					int const p2_attr = p2.edge_attr(i);

					/* horizontal edge */
					if (p1.y() == p2.y());

					/* right edge */
					else if (p1.y() < p2.y())
						_interpolate(p1_attr, p2_attr, r_edge + p1.y(), p2.y() - p1.y());

					/* left edge */
					else
						_interpolate(p2_attr, p1_attr, l_edge + p2.y(), p1.y() - p2.y());
				}
			}
		}
};

#endif /* _INCLUDE__POLYGON_GFX__POLYGON_PAINTER_BASE_H_ */
