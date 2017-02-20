/*
 * \brief  Functor for painting shaded polygons
 * \author Norman Feske
 * \date   2015-06-19
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__POLYGON_GFX__SHADED_POLYGON_PAINTER_H_
#define _INCLUDE__POLYGON_GFX__SHADED_POLYGON_PAINTER_H_

#include <os/surface.h>
#include <polygon_gfx/polygon_painter_base.h>
#include <polygon_gfx/interpolate_rgba.h>

namespace Polygon { class Shaded_painter; }


class Polygon::Shaded_painter : public Polygon::Painter_base
{
	private:

		/**
		 * Edge attribute IDs
		 */
		enum Edge_attr { ATTR_X, ATTR_R, ATTR_G, ATTR_B, ATTR_A, NUM_ATTR };

		Edge_buffers<NUM_ATTR> _edges;

	public:

		/**
		 * Polygon point used for RGBA-shaded polygons
		 */
		struct Point : Point_base
		{
			Color color;

			Point() { }
			Point(int x, int y, Color color) : Point_base(x, y), color(color) { }

			enum { NUM_EDGE_ATTRIBUTES = NUM_ATTR };

			inline int edge_attr(int id) const
			{
				switch (id) {
				default:
				case ATTR_X: return Point_base::edge_attr(id);
				case ATTR_R: return color.r;
				case ATTR_G: return color.g;
				case ATTR_B: return color.b;
				case ATTR_A: return color.a;
				}
			}

			inline void edge_attr(int id, int value)
			{
				switch (id) {
				case ATTR_X: Point_base::edge_attr(id, value); return;
				case ATTR_R: color.r = value; return;
				case ATTR_G: color.g = value; return;
				case ATTR_B: color.b = value; return;
				case ATTR_A: color.a = value; return;
				}
			}
		};

		/**
		 * Constructor
		 *
		 * \param alloc       allocator used for allocating edge buffers
		 * \param max_height  maximum size of polygon to draw, used to dimension
		 *                    the edge buffers
		 */
		Shaded_painter(Genode::Allocator &alloc, unsigned max_height)
		:
			_edges(alloc, max_height)
		{ }

		/**
		 * Draw polygon with linearly interpolated color
		 *
		 * \param points      Array of polygon points
		 * \param num_points  Number of polygon points
		 *
		 * The pixel surface and the alpha surface must have the same
		 * dimensions.
		 */
		template <typename PT, typename AT>
		void paint(Genode::Surface<PT> &pixel_surface,
		           Genode::Surface<AT> &alpha_surface,
		           Point const points[], unsigned num_points)
		{
			Point clipped[2*max_points_clipped(num_points)];
			num_points = clip_polygon<Point>(points, num_points, clipped,
			                                 pixel_surface.clip());

			Rect const bbox = bounding_box(clipped, num_points, pixel_surface.size());

			fill_edge_buffers(_edges, clipped, num_points);

			int * const x_l_edge = _edges.left (ATTR_X);
			int * const x_r_edge = _edges.right(ATTR_X);
			int * const r_l_edge = _edges.left (ATTR_R);
			int * const r_r_edge = _edges.right(ATTR_R);
			int * const g_l_edge = _edges.left (ATTR_G);
			int * const g_r_edge = _edges.right(ATTR_G);
			int * const b_l_edge = _edges.left (ATTR_B);
			int * const b_r_edge = _edges.right(ATTR_B);
			int * const a_l_edge = _edges.left (ATTR_A);
			int * const a_r_edge = _edges.right(ATTR_A);

			/* calculate begin of first destination scanline */
			unsigned const dst_w = pixel_surface.size().w();
			PT        *dst_pixel = pixel_surface.addr() + dst_w*bbox.y1();
			AT        *dst_alpha = alpha_surface.addr() + dst_w*bbox.y1();

			for (int y = bbox.y1(); y < bbox.y2(); y++) {

				/* read left and right color values from corresponding edge buffers */
				Color l_color = Color(r_l_edge[y], g_l_edge[y], b_l_edge[y], a_l_edge[y]);
				Color r_color = Color(r_r_edge[y], g_r_edge[y], b_r_edge[y], a_r_edge[y]);

				int const x_l = x_l_edge[y];
				int const x_r = x_r_edge[y];

				if (x_l < x_r)
					interpolate_rgba(l_color, r_color, dst_pixel + x_l,
					                 (unsigned char *)dst_alpha + x_l,
					                 x_r - x_l, x_l, y);

				dst_pixel += dst_w;
				dst_alpha += dst_w;
			}

			pixel_surface.flush_pixels(bbox);
		}
};

#endif /* _INCLUDE__POLYGON_GFX__SHADED_POLYGON_PAINTER_H_ */
