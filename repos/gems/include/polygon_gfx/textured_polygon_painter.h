/*
 * \brief  Functor for painting textured polygons
 * \author Norman Feske
 * \date   2015-06-29
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__POLYGON_GFX__TEXTURED_POLYGON_PAINTER_H_
#define _INCLUDE__POLYGON_GFX__TEXTURED_POLYGON_PAINTER_H_

#include <os/surface.h>
#include <os/texture.h>
#include <polygon_gfx/polygon_painter_base.h>
#include <polygon_gfx/texturize_rgba.h>

namespace Polygon { class Textured_painter; }


class Polygon::Textured_painter : public Polygon::Painter_base
{
	private:

		/**
		 * Edge attribute IDs
		 */
		enum Edge_attr { ATTR_X, ATTR_U, ATTR_V, NUM_ATTR };

		Edge_buffers<NUM_ATTR> _edges;

	public:

		/**
		 * Polygon point used for textured polygons
		 */
		struct Point : Point_base
		{
			int u = 0, v = 0;

			Point() { }
			Point(int x, int y, int u, int v) : Point_base(x, y), u(u), v(v) { }

			enum { NUM_EDGE_ATTRIBUTES = NUM_ATTR };

			inline int edge_attr(int id) const
			{
				switch (id) {
				default:
				case ATTR_X: return Point_base::edge_attr(id);
				case ATTR_U: return u;
				case ATTR_V: return v;
				}
			}

			inline void edge_attr(int id, int value)
			{
				switch (id) {
				case ATTR_X: Point_base::edge_attr(id, value); return;
				case ATTR_U: u = value; return;
				case ATTR_V: v = value; return;
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
		Textured_painter(Genode::Allocator &alloc, unsigned max_height)
		:
			_edges(alloc, max_height)
		{ }

		/**
		 * Draw textured polygon
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
		           Point const points[], unsigned num_points,
		           Genode::Texture<PT> const &texture)
		{
			Point clipped[2*max_points_clipped(num_points)];
			num_points = clip_polygon<Point>(points, num_points, clipped,
			                                 pixel_surface.clip());

			Rect const bbox = bounding_box(clipped, num_points, pixel_surface.size());

			fill_edge_buffers(_edges, clipped, num_points);

			int * const x_l_edge = _edges.left (ATTR_X);
			int * const x_r_edge = _edges.right(ATTR_X);
			int * const u_l_edge = _edges.left (ATTR_U);
			int * const u_r_edge = _edges.right(ATTR_U);
			int * const v_l_edge = _edges.left (ATTR_V);
			int * const v_r_edge = _edges.right(ATTR_V);

			unsigned      const  src_w     = texture.size().w();
			PT            const *src_pixel = texture.pixel();
			unsigned char const *src_alpha = texture.alpha();

			/* calculate begin of destination scanline */
			unsigned const dst_w = pixel_surface.size().w();
			PT        *dst_pixel = pixel_surface.addr() + dst_w*bbox.y1();
			AT        *dst_alpha = alpha_surface.addr() + dst_w*bbox.y1();

			for (int y = bbox.y1(); y < bbox.y2(); y++) {

				/*
				 * Read left and right texture coordinates (u,v) from
				 * corresponding edge buffers.
				 */
				Genode::Point<> const l_texpos(u_l_edge[y], v_l_edge[y]);
				Genode::Point<> const r_texpos(u_r_edge[y], v_r_edge[y]);

				int const x_l = x_l_edge[y];
				int const x_r = x_r_edge[y];

				if (x_l < x_r)
					texturize_rgba(l_texpos, r_texpos,
					               dst_pixel + x_l, (unsigned char *)dst_alpha + x_l,
					               x_r - x_l, src_pixel, src_alpha, src_w);

				dst_pixel += dst_w;
				dst_alpha += dst_w;
			}

			pixel_surface.flush_pixels(bbox);
		}
};

#endif /* _INCLUDE__POLYGON_GFX__TEXTURED_POLYGON_PAINTER_H_ */
