/*
 * \brief   Generic interface of graphics backend and chunky template
 * \date    2010-09-27
 * \author  Norman Feske
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__NANO3D__CANVAS_H_
#define _INCLUDE__NANO3D__CANVAS_H_

#include <nano3d/color.h>
#include <nano3d/geometry.h>

namespace Nano3d {

	class Allocator;

	class Canvas : public Area
	{
		protected:

			Rect     _clip;      /* clipping area */
			unsigned _capacity;  /* max number of pixels */

		public:

			/**
			 * Constructor
			 */
			Canvas(unsigned capacity, Area size)
			: Area(size), _capacity(capacity)
			{ }

			virtual ~Canvas() { }

			/**
			 * Define clipping rectangle
			 */
			void clip(Rect rect)
			{
				/* constrain new clipping region by canvas boundaries */
				_clip = Rect::intersect(Rect(Point(0, 0), *this), rect);
			}

			/**
			 * Set logical size of canvas
			 */
			int set_size(Area new_size)
			{
				if (new_size.num_pixels() > _capacity) return -1;

				/* assign new size to ourself */
				*(Area *)(this) = new_size;

				/* set clipping */
				clip(Rect(Point(0, 0), new_size));
				return 0;
			}

			/**
			 * Return base address
			 */
			virtual void *addr() = 0;

			/**
			 * Define base address of pixel data
			 */
			virtual void addr(void *) = 0;

			/**
			 * Anonymous texture struct
			 */
			class Texture;

			/**
			 * Allocate texture container
			 */
			virtual Texture *alloc_texture(Allocator *alloc, Area size) = 0;

			/**
			 * Free texture container
			 */
			virtual void free_texture(Texture *texture) = 0;

			/**
			 * Assign rgba values to texture line
			 */
			virtual void set_rgba_texture(Texture *dst, unsigned char *rgba, int len, int y) = 0;
	};


	template <typename ST, int R_MASK, int R_SHIFT,
	                       int G_MASK, int G_SHIFT,
	                       int B_MASK, int B_SHIFT,
	                       int A_MASK, int A_SHIFT>
	class Pixel_rgba
	{
		private:

			/**
			 * Shift left with positive or negative shift value
			 */
			inline int shift(int value, int shift)
			{
				return shift > 0 ? value << shift : value >> -shift;
			}

		public:

			static const int r_mask = R_MASK, r_shift = R_SHIFT,
			                 g_mask = G_MASK, g_shift = G_SHIFT,
			                 b_mask = B_MASK, b_shift = B_SHIFT,
			                 a_mask = A_MASK, a_shift = A_SHIFT;

			ST pixel;

			/**
			 * Constructors
			 */
			Pixel_rgba() {}

			Pixel_rgba(int red, int green, int blue, int alpha = 255)
			{
				rgba(red, green, blue, alpha);
			}

			/**
			 * Assign new rgba values
			 */
			void rgba(int red, int green, int blue, int alpha = 255)
			{
				pixel = (shift(red,   r_shift) & r_mask)
				      | (shift(green, g_shift) & g_mask)
				      | (shift(blue,  b_shift) & b_mask)
				      | (shift(alpha, a_shift) & a_mask);
			}

			inline int r() { return shift(pixel & r_mask, -r_shift); }
			inline int g() { return shift(pixel & g_mask, -g_shift); }
			inline int b() { return shift(pixel & b_mask, -b_shift); }

			/**
			 * Multiply pixel with alpha value
			 */
			static inline Pixel_rgba blend(Pixel_rgba pixel, int alpha);

			/**
			 * Mix two pixels at the ratio specified as alpha
			 */
			static inline Pixel_rgba mix(Pixel_rgba p1, Pixel_rgba p2, int alpha);

			/**
			 * Compute average color value of two pixels
			 */
			static inline Pixel_rgba avr(Pixel_rgba p1, Pixel_rgba p2);

			/**
			 * Compute average color value of four pixels
			 */
			static inline Pixel_rgba avr(Pixel_rgba p1, Pixel_rgba p2,
			                             Pixel_rgba p3, Pixel_rgba p4)
			{
				return avr(avr(p1, p2), avr(p3, p4));
			}
	} __attribute__((packed));


	/**
	 * Polygon point used for flat polygons
	 */
	class Polypoint : public Point
	{
		public:

			/**
			 * Constructors
			 */
			Polypoint() { }
			Polypoint(int x, int y): Point(x, y) { }

			enum { NUM_EDGE_ATTRIBUTES = 1 };

			/**
			 * Return edge attribute by ID
			 */
			inline int edge_attr(int id) { return _x; }

			/**
			 * Assign value to edge attribute with specified ID
			 */
			inline void edge_attr(int id, int new_value) { _x = new_value; }
	};


	/**
	 * Polygon point used for rgba-shaded polygons
	 */
	class Colored_polypoint : public Polypoint
	{
		public:

			Color color;

			/**
			 * Constructors
			 */
			Colored_polypoint() { }
			Colored_polypoint(int x, int y, Color color_init)
			: Polypoint(x, y), color(color_init) { }


			/*************************
			 ** Polypoint interface **
			 *************************/

			enum { NUM_EDGE_ATTRIBUTES = 5 };

			inline int edge_attr(int id)
			{
				switch (id) {
				default:
				case 0: return _x;
				case 1: return color.r;
				case 2: return color.g;
				case 3: return color.b;
				case 4: return color.a;
				}
			}

			inline void edge_attr(int id, int new_value)
			{
				switch (id) {
				case 0: _x      = new_value; return;
				case 1: color.r = new_value; return;
				case 2: color.g = new_value; return;
				case 3: color.b = new_value; return;
				case 4: color.a = new_value; return;
				}
			}
	};


	/**
	 * Polygon point used for textured polygons
	 */
	class Textured_polypoint : public Polypoint
	{
		public:

			int tx, ty;

			/**
			 * Constructors
			 */
			Textured_polypoint() { }
			Textured_polypoint(int x, int y, Point texture_pos)
			: Polypoint(x, y), tx(texture_pos.x()), ty(texture_pos.y()) { }


			/*************************
			 ** Polypoint interface **
			 *************************/

			enum { NUM_EDGE_ATTRIBUTES = 3 };

			inline int edge_attr(int id)
			{
				switch (id) {
				default:
				case 0: return _x;
				case 1: return tx;
				case 2: return ty;
				}
			}

			inline void edge_attr(int id, int new_value)
			{
				switch (id) {
				case 0: _x = new_value; return;
				case 1: tx = new_value; return;
				case 2: ty = new_value; return;
				}
			}
	};


	/**********************
	 ** Clipping support **
	 **********************/

	/**
	 * Calculate ratio of range intersection
	 *
	 * \param v_start   Start of range
	 * \param v_end     End of range
	 * \param v_cut     Range cut (should be in interval v_start...v_end)
	 * \return          Ratio of intersection as 16.16 fixpoint
	 *
	 * The input arguments 'v_start', 'v_end', 'v_cut' must use only
	 * the lower 16 bits of the int type.
	 */
	inline int intersect_ratio(int v_start, int v_end, int v_cut)
	{
		int dv     = v_end - v_start,
		    dv_cut = v_cut - v_start;

		return dv ? (dv_cut<<16)/dv : 0;
	}


	/**
	 * Support for vertical clipping boundary
	 */
	template <typename POLYPOINT>
	class Clipper_vertical
	{
		public:

			/**
			 * Select clipping-sensitive attribute from polygon point
			 */
			static int clip_value(POLYPOINT p) { return p.x(); }

			/**
			 * Calculate intersection point
			 */
			static POLYPOINT clip(POLYPOINT p1, POLYPOINT p2, int clip)
			{
				/*
				 * Enforce unique x order of points to apply rounding errors
				 * consistently also when edge points are specified in reverse.
				 * Typically the same edge is used in reverse direction by
				 * each neighboured polygon.
				 */
				if (clip_value(p1) > clip_value(p2)) { POLYPOINT tmp = p1; p1 = p2; p2 = tmp; }

				/* calculate ratio of the intersection of edge and clipping boundary */
				int ratio = intersect_ratio(p1.x(), p2.x(), clip);

				/* calculate y value at intersection point */
				POLYPOINT result;
				*(Point *)&result = Point(clip, p1.y() + ((ratio*(p2.y() - p1.y()))>>16));

				/* calculate intersection values for edge attributes other than x */
				for (int i = 1; i < POLYPOINT::NUM_EDGE_ATTRIBUTES; i++) {
					int v1 = p1.edge_attr(i),
					    v2 = p2.edge_attr(i);
					result.edge_attr(i, v1 + ((ratio*(v2 - v1))>>16));
				}
				return result;
			}
	};


	/**
	 * Support for horizontal clipping boundary
	 */
	template <typename POLYPOINT>
	class Clipper_horizontal
	{
		public:

			/**
			 * Select clipping-sensitive attribute from polygon point
			 */
			static int clip_value(POLYPOINT p) { return p.y(); }

			/**
			 * Calculate intersection point
			 */
			static POLYPOINT clip(POLYPOINT p1, POLYPOINT p2, int clip)
			{
				if (clip_value(p1) > clip_value(p2)) { POLYPOINT tmp = p1; p1 = p2; p2 = tmp; }

				/* calculate ratio of the intersection of edge and clipping boundary */
				int ratio = intersect_ratio(clip_value(p1), clip_value(p2), clip);

				/* calculate y value at intersection point */
				POLYPOINT result;
				*(Point *)&result = Point(p1.x() + ((ratio*(p2.x() - p1.x()))>>16), clip);

				/* calculate intersection values for edge attributes other than x */
				for (int i = 1; i < POLYPOINT::NUM_EDGE_ATTRIBUTES; i++) {
					int v1 = p1.edge_attr(i),
					    v2 = p2.edge_attr(i);
					result.edge_attr(i, v1 + ((ratio*(v2 - v1))>>16));
				}
				return result;
			}
	};


	/**
	 * Support for clipping against a lower boundary
	 */
	class Clipper_min
	{
		public:

			static bool inside(int value, int boundary) { return value >= boundary; }
	};


	/**
	 * Support for clipping against a higher boundary
	 */
	class Clipper_max
	{
		public:

			static bool inside(int value, int boundary) { return value <= boundary; }
	};


	/**
	 * One-dimensional clipping
	 *
	 * This template allows for the aggregation of the policies
	 * defined above to build specialized 1D-clipping functions for
	 * upper/lower vertical/horizontal clipping boundaries and for
	 * polygon points with different attributes.
	 */
	template <typename CLIPPER_DIRECTION, typename CLIPPER_MINMAX, typename POLYPOINT>
	class Clipper : public CLIPPER_DIRECTION, public CLIPPER_MINMAX
	{
		public:

			/**
			 * Check whether point is inside the clipping area or not
			 */
			static bool inside(POLYPOINT p, int clip)
			{
				return CLIPPER_MINMAX::inside(CLIPPER_DIRECTION::clip_value(p), clip);
			}
	};


	/**
	 * Create clipping helpers
	 *
	 * This class is used as a compound containing all rules to
	 * clip a polygon against a 2d region such as the clipping
	 * region of a Canvas.
	 */
	template <typename POLYPOINT>
	class Clipper_2d
	{
		public:

			typedef Clipper<Clipper_horizontal<POLYPOINT>, Clipper_min, POLYPOINT> Top;
			typedef Clipper<Clipper_horizontal<POLYPOINT>, Clipper_max, POLYPOINT> Bottom;
			typedef Clipper<Clipper_vertical<POLYPOINT>,   Clipper_min, POLYPOINT> Left;
			typedef Clipper<Clipper_vertical<POLYPOINT>,   Clipper_max, POLYPOINT> Right;
	};


	/**
	 * Interpolate linearly between start value and end value
	 */
	static inline void interpolate(int start, int end, int *dst, int num_values)
	{
		/* sanity check */
		if (num_values <= 0) return;

		int ascent = ((end - start)<<16)/num_values;

		for (int curr = start<<16; num_values--; curr += ascent)
			*dst++ = curr>>16;
	}


	/**
	 * Interpolate color values
	 */
	template <typename PT>
	static inline void interpolate_colors(Color start, Color end, PT *dst,
	                                      unsigned char *dst_alpha, int num_values,
	                                      int, int)
	{
		/* sanity check */
		if (num_values <= 0) return;

		/* use 16.16 fixpoint values for the calculation */
		int r_ascent = ((end.r - start.r)<<16)/num_values,
		    g_ascent = ((end.g - start.g)<<16)/num_values,
		    b_ascent = ((end.b - start.b)<<16)/num_values,
		    a_ascent = ((end.a - start.a)<<16)/num_values;

		/* set start values for color components */
		int r = start.r<<16,
		    g = start.g<<16,
		    b = start.b<<16,
		    a = start.a<<16;

		for ( ; num_values--; dst++, dst_alpha++) {

			/* combine current color value with existing pixel via alpha blending */
			*dst        = PT::mix(*dst, PT(r>>16, g>>16, b>>16), a>>16);
			*dst_alpha += ((255 - *dst_alpha)*a) >> (16 + 8);

			/* increment color-component values by ascent */
			r += r_ascent;
			g += g_ascent;
			b += b_ascent;
			a += a_ascent;
		}
	}


	/**
	 * Texturize scanline
	 */
	template <typename PT>
	static inline void texturize_scanline(Point start, Point end, PT *dst,
	                                      unsigned char *dst_alpha,
	                                      int num_values, PT *texture_base,
	                                      unsigned char *alpha_base,
	                                      int texture_width)
	{
		/* sanity check */
		if (num_values <= 0) return;

		/* use 16.16 fixpoint values for the calculation */
		int tx_ascent = ((end.x() - start.x())<<16)/num_values,
		    ty_ascent = ((end.y() - start.y())<<16)/num_values;

		/* set start values for color components */
		int tx = start.x()<<16,
		    ty = start.y()<<16;

		for ( ; num_values--; dst++, dst_alpha++) {

			/* copy pixel from texture */
			unsigned long src_offset = (ty>>16)*texture_width + (tx>>16);
			*dst       = texture_base[src_offset];
			*dst_alpha = alpha_base[src_offset];

			/* walk through texture */
			tx += tx_ascent;
			ty += ty_ascent;
		}
	}


	template <typename PT>
	class Chunky_canvas : public Canvas
	{
		protected:

			/**
			 * Base address of pixel buffer
			 */
			PT *_addr;

			/**
			 * Base address of alpha buffer, or 0 if no alpha
			 * channel is used
			 */
			unsigned char *_alpha;

			int *_l_edge;  /* temporary values of left polygon edge  */
			int *_r_edge;  /* temporary values of right polygon edge */

			/**
			 * Clip polygon against boundary
			 *
			 * \param src             Array of unclipped source-polygon points
			 * \param src_num_points  Number of source-polygon points
			 * \param dst             Destination buffer for clipped polygon points
			 * \param clip            Clipping boundary
			 * \return                Number of resulting polygon points
			 */
			template <typename CLIPPER, typename POLYPOINT>
			static inline int _clip_1d(POLYPOINT *src, int src_num_points,
			                           POLYPOINT *dst, int clip)
			{
				/*
				 * Walk along the polygon edges. Each time when
				 * crossing the clipping border, a new polygon
				 * point is created at the intersection point.
				 * All polygon points outside the clipping area
				 * are discarded.
				 */
				int dst_num_points = 0;
				for (int i = 0; i < src_num_points; i++) {

					POLYPOINT curr = *src++;
					POLYPOINT next = *src;

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
			int _max_points_clipped(int num_points) {
				return num_points + 4 + 1; }

			/**
			 * Clip polygon against active clipping region
			 *
			 * \param src_points  Array of unclipped polygon points
			 * \param num_points  Number of unclipped polygon points
			 * \param dst_points  Buffer for storing the clipped polygon
			 * \return            Number of points of resulting polygon
			 *
			 * During the clipping computation, the destination buffer
			 * 'dst_points' is used to store two polygons. Therefore,
			 * the buffer must be dimensioned at 2*'max_points_clipped()'.
			 * The end result of the computation is stored at the
			 * beginning of 'dst_points'.
			 */
			template <typename POLYPOINT>
			int _clip_polygon(POLYPOINT *src_points, int num_points,
			                  POLYPOINT *dst_points)
			{
				POLYPOINT *clipped0 = dst_points,
				          *clipped1 = dst_points + _max_points_clipped(num_points);

				for (int i = 0; i < num_points; i++)
					clipped0[i] = src_points[i];

				clipped0[num_points] = clipped0[0];

				/* clip against top, left, bottom, and right clipping boundaries */
				num_points = _clip_1d<typename Clipper_2d<POLYPOINT>::Top>   (clipped0, num_points, clipped1, _clip.y1());
				num_points = _clip_1d<typename Clipper_2d<POLYPOINT>::Left>  (clipped1, num_points, clipped0, _clip.x1());
				num_points = _clip_1d<typename Clipper_2d<POLYPOINT>::Bottom>(clipped0, num_points, clipped1, _clip.y2());
				num_points = _clip_1d<typename Clipper_2d<POLYPOINT>::Right> (clipped1, num_points, clipped0, _clip.x2());

				return num_points;
			}

			/**
			 * Determine y range spanned by the polygon
			 */
			template <typename POLYPOINT>
			void _calc_y_range(POLYPOINT *points, int num_points,
			                  int *out_y_min, int *out_y_max)
			{
				int y_min = h() - 1, y_max = 0;
				for (int i = 0; i < num_points; i++) {
					y_min = min(y_min, points[i].y());
					y_max = max(y_max, points[i].y());
				}
				*out_y_min = y_min;
				*out_y_max = y_max;
			}

			/**
			 * Calculate edge buffers for a polygon
			 *
			 * The edge buffers are partitioned into subsequent sub
			 * buffers with a size corresponding to the maximum y
			 * range of the polygon, which is the canvas height.
			 * The different sub buffers are used to hold the
			 * interpolated edge values for the different polygon-point
			 * attributes.
			 */
			template <typename POLYPOINT>
			void _fill_edge_buffers(POLYPOINT *points, int num_points)
			{
				int *l_edge = _l_edge,
				    *r_edge = _r_edge;

				/* for each edge attribute */
				for (int i = 0; i < POLYPOINT::NUM_EDGE_ATTRIBUTES; i++) {
					for (int j = 0; j < num_points; j++) {

						POLYPOINT p1 = points[j],
						          p2 = points[j + 1];

						/* request attribute values to interpolate */
						int p1_attr = p1.edge_attr(i),
						    p2_attr = p2.edge_attr(i);

						/* horizontal edge */
						if (p1.y() == p2.y());

						/* right edge */
						else if (p1.y() < p2.y())
							interpolate(p1_attr, p2_attr, r_edge + p1.y(), p2.y() - p1.y());

						/* left edge */
						else
							interpolate(p2_attr, p1_attr, l_edge + p2.y(), p1.y() - p2.y());
					}

					/* use next edge-buffer portion for next attribute */
					l_edge += h();
					r_edge += h();
				}
			}

			inline void _memclear(void *ptr, unsigned long num_bytes)
			{
				unsigned long num_longs = num_bytes/sizeof(long);
				unsigned long *dst = (unsigned long *)ptr;
				for (; num_longs--;)
					*dst++ = 0;
			}

		public:

			/**
			 * Constructor
			 */
			Chunky_canvas(PT *addr, unsigned char *alpha,
			              long capacity, Area size,
			              int *l_edge = 0, int *r_edge = 0)
			:	Canvas(capacity, size), _addr(addr), _alpha(alpha),
				_l_edge(l_edge), _r_edge(r_edge) { }


			/**********************
			 ** Canvas interface **
			 **********************/

			void *addr() { return _addr; }
			void  addr(void *addr) { _addr = static_cast<PT *>(addr); }

			Texture *alloc_texture(Allocator *alloc, Area size);
			void free_texture(Texture *texture);
			void set_rgba_texture(Texture *dst, unsigned char *rgba, int len, int y);


			/************************
			 ** Drawing primitives **
			 ************************/

			void clear()
			{
				_memclear(_addr, w()*h()*sizeof(PT));
				_memclear(_alpha, w()*h());
			}

			void draw_texture(Texture *src, Point point);

			void draw_dot(Point point, Color color)
			{
				if (!_clip.contains(point)) return;

				/* calculate target pixel address */
				PT            *dst       = _addr  + point.y()*w() + point.x();
				unsigned char *dst_alpha = _alpha + point.y()*w() + point.x();

				/* apply color */
				*dst = PT::mix(*dst, PT(color.r, color.g, color.b), color.a);
				*dst_alpha = 255;
			}

			void draw_line(Point p1, Point p2, Color color)
			{
				/* assure y2 >= y1 by swapping points if needed */
				if (p2.y() < p1.y()) { Point tmp = p1; p1 = p2; p2 = tmp; }

				/* both points must reside within clipping area */
				if (!_clip.contains(p1) || !_clip.contains(p2))
					return;

				int dx = p2.x() - p1.x(),
				    dy = p2.y() - p1.y();

				int xi = 1;
				if (dx < 0) {
					dx = -dx;
					xi = -xi;
				}

				int v_dec = dy*2,
				    v_inc = dx*2,
				    v     = dy;

				PT            *dst       = _addr  + w()*p1.y() + p1.x();
				unsigned char *dst_alpha = _alpha + w()*p1.y() + p1.x();

				PT  pixel = PT(color.r, color.g, color.b);
				int alpha = color.a;

				for (int d = dx + dy; d > 0; d--) {
					v -= v_dec;
					while ((v < 0) && d--) {
						dst        += w();
						dst_alpha  += w();
						v          += v_inc;
						*dst        = PT::mix(*dst, pixel, alpha);
						*dst_alpha  = 255;
					}
					dst        += xi;
					dst_alpha  += xi;
					*dst        = PT::mix(*dst, pixel, alpha);
					*dst_alpha  = 255;
				}
			}

			/**
			 * Draw polygon with solid color
			 *
			 * \param points      Array of polygon points
			 * \param num_points  Number of polygon points
			 * \param color       Solid color
			 *
			 * To use this function, l_edge and r_edge must be properly
			 * initialized.
			 */
			void draw_flat_polygon(Polypoint *points, int num_points, Color color)
			{
				/* sanity check for the presence of the required edge buffers */
				if (!_l_edge || !_r_edge) return;

				Polypoint clipped[2*_max_points_clipped(num_points)];
				num_points = _clip_polygon<Polypoint>
				                          (points, num_points, clipped);

				int y_min = 0, y_max = 0;
				_calc_y_range(clipped, num_points, &y_min, &y_max);

				_fill_edge_buffers(clipped, num_points);

				/* render scanlines */
				PT  pixel = PT(color.r, color.g, color.b);
				int alpha = color.a;
				PT *dst   = _addr + w()*y_min;  /* begin of destination scanline */
				for (int y = y_min; y < y_max; y++, dst += w()) {

					int num_scanline_pixels = _r_edge[y] - _l_edge[y];
					for (PT *d = dst + _l_edge[y] ; num_scanline_pixels-- > 0; d++)
						*d = PT::mix(*d, pixel, alpha);
				}
			}

			/**
			 * Draw polygon with linearly interpolated color
			 *
			 * \param points      Array of polygon points
			 * \param num_points  Number of polygon points
			 *
			 * To use this function, l_edge and r_edge must be properly
			 * initialized.
			 */
			void draw_shaded_polygon(Colored_polypoint *points, int num_points)
			{
				/* sanity check for the presence of the required edge buffers */
				if (!_l_edge || !_r_edge) return;

				Colored_polypoint clipped[2*_max_points_clipped(num_points)];
				num_points = _clip_polygon<Colored_polypoint>
				                          (points, num_points, clipped);

				int y_min = 0, y_max = 0;
				_calc_y_range(clipped, num_points, &y_min, &y_max);

				_fill_edge_buffers(clipped, num_points);

				/* calculate addresses of edge buffers holding the texture attributes */
				int *red_l_edge   = _l_edge      + h(),
				    *red_r_edge   = _r_edge      + h(),
				    *green_l_edge = red_l_edge   + h(),
				    *green_r_edge = red_r_edge   + h(),
				    *blue_l_edge  = green_l_edge + h(),
				    *blue_r_edge  = green_r_edge + h(),
				    *alpha_l_edge = blue_l_edge  + h(),
				    *alpha_r_edge = blue_r_edge  + h();

				/* begin of first destination scanline */
				PT            *dst       = _addr  + w()*y_min;
				unsigned char *dst_alpha = _alpha + w()*y_min;

				for (int y = y_min; y < y_max; y++, dst += w(), dst_alpha += w()) {

					/* read left and right color values from corresponding edge buffers */
					Color l_color = Color(red_l_edge[y], green_l_edge[y], blue_l_edge[y], alpha_l_edge[y]);
					Color r_color = Color(red_r_edge[y], green_r_edge[y], blue_r_edge[y], alpha_r_edge[y]);

					interpolate_colors(l_color, r_color, dst + _l_edge[y],
					                   dst_alpha + _l_edge[y],
					                   _r_edge[y] - _l_edge[y], _l_edge[y], y);
				}
			}

			/**
			 * Draw textured polygon
			 *
			 * \param points      Array of polygon points
			 * \param num_points  Number of polygon points
			 * \param texture     Texture
			 *
			 * To use this function, l_edge and r_edge must be properly
			 * initialized.
			 */
			void draw_textured_polygon(Textured_polypoint *points,
			                           int num_points, Texture *texture);
	};
}

#endif /* _INCLUDE__NANO3D__CANVAS_H_ */
