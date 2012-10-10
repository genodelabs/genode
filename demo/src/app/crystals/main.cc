/*
 * \brief  Live backdrop showing the Genode process tree
 * \author Norman Feske
 * \date   2012-10-09
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <timer_session/connection.h>
#include <nitpicker_session/connection.h>
#include <nitpicker_view/client.h>
#include <framebuffer_session/client.h>

/* local includes */
#include <vertex_array.h>


typedef Nano3d::Pixel_rgb565 PT;


struct Heap_allocator : Nano3d::Allocator
{
	void *alloc(unsigned long size)
	{
		return Genode::env()->heap()->alloc(size);
	}

	void free(void *ptr, unsigned long size)
	{
		Genode::env()->heap()->free(ptr, size);
	}
};


struct Edge
{
	int left_face, right_face;

	int vertex[2];

	Edge()
	: left_face(0), right_face(0)
	{
		vertex[0] = 0; vertex[1] = 0;
	}

	Edge(int vertex_0, int vertex_1, int left_face, int right_face)
	: left_face(left_face), right_face(right_face)
	{
		vertex[0] = vertex_0; vertex[1] = vertex_1;
	}
};


struct Face
{
	int edge[5];

	Face()
	{
		for (int i = 0; i < 5; i++)
			edge[i] = 0;
	}

	Face(int e0, int e1, int e2, int e3, int e4)
	{
		edge[0] = e0, edge[1] = e1, edge[2] = e2, edge[3] = e3, edge[4] = e4;
	}
};



template <typename PT>
class Dodecahedron
{
	private:

		enum { NUM_VERTICES = 20 };
		Vertex_array<NUM_VERTICES> _vertices;

		enum { NUM_EDGES = 30 };
		Edge _edges[NUM_EDGES];

		enum { NUM_FACES = 12 };
		Face _faces[NUM_FACES];

		/* ratio of edge length to radius, as 16.16 fixpoint number */
		enum { A_TO_R = 46769 };

		/* angle between two edges, scaled to 0..1024 range */
		enum { DIHEDRAL_ANGLE = 332 };

	public:


		/**
		 * \param r  radius of the surrounding sphere
		 */
		Dodecahedron(int r)
		{
			using namespace Nano3d;
			/*
			 * Vertices
			 */

			/*
			 * There are four level, each with 5 vertices.
			 *
			 * y0 and y1 are the y positions of the first and second level.
			 * r0 and r1 are the radius of first and second levels.
			 * The third and fourth levels are symetric to the first levels.
			 */
			int const y0 = -(r * 52078) >> 16; /* r*0.7947 */
			int const y1 = -(r * 11030) >> 16;
			int const r0 =  (r * 39780) >> 16; /* r*0.607  */
			int const r1 =  (r * 63910) >> 16;

			enum { ANGLE_STEP = 1024 / 5 };
			enum { ANGLE_HALF_STEP = 1024 / 10 };

			int j = 0; /* index into '_vertices' array */

			/* level 1 */
			for (int i = 0; i < 5; i++) {
				int const a = i*ANGLE_STEP;
				_vertices[j++] = Vertex((r0*sin(a)) >> 16, y0, (r0*cos(a)) >> 16);
			}

			/* level 2 */
			for (int i = 0; i < 5; i++) {
				int const a = i*ANGLE_STEP;
				_vertices[j++] = Vertex((r1*sin(a)) >> 16, y1, (r1*cos(a)) >> 16);
			}

			/* level 3 */
			for (int i = 0; i < 5; i++) {
				int const a = i*ANGLE_STEP + ANGLE_HALF_STEP;
				_vertices[j++] = Vertex((r1*sin(a)) >> 16, -y1, (r1*cos(a)) >> 16);
			}

			/* level 4 */
			for (int i = 0; i < 5; i++) {
				int const a = i*ANGLE_STEP + ANGLE_HALF_STEP;
				_vertices[j++] = Vertex((r0*sin(a)) >> 16, -y0, (r0*cos(a)) >> 16);
			}

			/*
			 * Edges
			 */

			j = 0; /* index into '_edges' array */

			/* level 1 */
			for (int i = 0; i < 5; i++)
				_edges[j++] = Edge(i, (i + 1) % 5,   i + 1, 0);

			/* level 1 to level 2 */
			for (int i = 0; i < 5; i++)
				_edges[j++] = Edge(i, i + 5,   1 + (i + 4) % 5, 1 + i);

			/* level 2 to level 3 */
			for (int i = 0; i < 5; i++)
				_edges[j++] = Edge(i + 5, i + 10,   1 + 5 + (i + 4) % 5,  1 + i);

			/* level 3 to level 2 */
			for (int i = 0; i < 5; i++)
				_edges[j++] = Edge(i + 10, (i + 1) % 5 + 5,   1 + 5 + i, 1 + i);

			/* level 3 to level 4 */
			for (int i = 0; i < 5; i++)
				_edges[j++] = Edge(i + 10, i + 15,   1 + 5 + (i + 4) % 5,  1 + 5 + i);

			/* level 4 */
			for (int i = 0; i < 5; i++)
				_edges[j++] = Edge(i + 15, (i + 1) % 5 + 15,    11, 1 + 5 + i);

			/*
			 * Faces
			 */

			j = 0; /* index into '_faces' array */
			_faces[j++] = Face(0, 1, 2, 3, 4);

			for (int i = 0; i < 5; i++)
				_faces[j++] = Face(i, i + 5, i + 10, i + 15, 5 + (1 + i) % 5);

			for (int i = 0; i < 5; i++)
				_faces[j++] = Face(i + 20, i + 25,  (i + 1) % 5 + 20,   10 + (i + 1) % 5,    15 + i);

			_faces[j++] = Face(29, 28, 27, 26, 25);
		}

		void draw(Nano3d::Chunky_canvas<PT> *dst, int frame,  bool backward_facing = false)
		{
			using namespace Nano3d;

			Vertex_array<NUM_VERTICES> vertices = _vertices;

			vertices.rotate_x(frame*1);
			vertices.rotate_y(frame*2);
			vertices.rotate_z(frame*3);
			vertices.project(1600, 800);
			vertices.translate(400, 350, 0);

			enum { SHOW_EDGE = -1 };

			if (0) {
				for (int i = 0; i < NUM_EDGES; i++) {
					Vertex v1 = vertices[_edges[i].vertex[0]];
					Vertex v2 = vertices[_edges[i].vertex[1]];

					Point p1 = Point(v1.x(), v1.y());
					Point p2 = Point(v2.x(), v2.y());

					Color color(40 + 7*i,  128, 255 - 7*i, 20);

					if (i == SHOW_EDGE)
						color = Color(255, 255, 255);

					dst->draw_line(p1, p2, color);
				}
			}

			Colored_polypoint p[5];
			for (int i = 0; i < NUM_FACES; i++) {

				for (int j = 0; j < 5; j++) {

					Face face = _faces[i];

					int edge_idx = face.edge[j];

					Edge edge = _edges[edge_idx];

					int vertex_idx = -1;

					if (edge.left_face == i)
						vertex_idx = edge.vertex[1];

					if (edge.right_face == i)
						vertex_idx = edge.vertex[0];

					if (edge_idx == SHOW_EDGE)
						PLOG("edge=%d (left_face=%d, right_face=%d) (%d->%d)",
						     edge_idx, edge.left_face, edge.right_face, edge.vertex[0], edge.vertex[1]);

					if (vertex_idx == -1) {
						PERR("face %d: invalid edge=%d (left_face=%d, right_face=%d) (%d->%d)",
						     i, edge_idx, edge.left_face, edge.right_face, edge.vertex[0], edge.vertex[1]);
						break;
					}

					Vertex v = vertices[vertex_idx];

					if (backward_facing) {
						p[4 - j] = Colored_polypoint(v.x(), v.y(), Color(j*10, j*10, j*10, 230 - j*18));
					} else {
						p[j] = Colored_polypoint(v.x(), v.y(), Color(240, 10*j, 0, 50 + j*30));
					}
				}

				dst->draw_shaded_polygon(p, 5);
			}
		}
};


class Process_tree
{
	public:

		class Unsupported_color_depth { };

	private:

		/**
		 * Screen position
		 */
		long _xpos, _ypos, _width, _height;

		Nitpicker::Connection       _nitpicker;
		Framebuffer::Mode           _mode;
		Framebuffer::Session_client _framebuffer;

		Framebuffer::Session_capability _init_framebuffer()
		{
			_nitpicker.buffer(_mode, true);
			return _nitpicker.framebuffer_session();
		}

		int            _fb_width, _fb_height;
		void          *_fb_base;
		unsigned char *_fb_alpha;

		/**
		 * Helper for construction
		 */
		void *_map_framebuffer()
		{
			Framebuffer::Mode mode = _framebuffer.mode();
			_fb_width = mode.width();
			_fb_height = mode.height();

			if (mode.format() != Framebuffer::Mode::RGB565) {
				PERR("framebuffer mode %d is not supported\n", mode.format());
				throw Unsupported_color_depth();
			}

			return Genode::env()->rm_session()->attach(_framebuffer.dataspace());
		}

		Nitpicker::View_capability _view_cap;
		Nitpicker::View_client     _view;

		/*
		 * Dimension edge buffers such that they can hold the
		 * interpolated edge values of 5 different attributes.
		 */
		enum { MAX_FB_HEIGHT = 2000 };
		int _l_edge[MAX_FB_HEIGHT*5];
		int _r_edge[MAX_FB_HEIGHT*5];

		/**
		 * Allocator used for texture allocation
		 */
		Heap_allocator _alloc;

		Nano3d::Rect _clip;

		unsigned long _backbuffer_offset() { return _fb_width*_fb_height/2; }

		Nano3d::Chunky_canvas<PT> _canvas_1, _canvas_2;

		Nano3d::Canvas_rgb565 *_canvas_front;
		Nano3d::Canvas_rgb565 *_canvas_back;

		Dodecahedron<PT> _outer_dodecahedron;
		Dodecahedron<PT> _inner_dodecahedron;

		int _frame;

	public:

		Process_tree(long xpos, long ypos, long width, long height)
		:
			_xpos(xpos), _ypos(ypos), _width(width), _height(height),
			_nitpicker(true),
			_mode(_width, 2*_height, Framebuffer::Mode::RGB565),
			_framebuffer(_init_framebuffer()),
			_fb_base(_map_framebuffer()),
			_fb_alpha((unsigned char *)_fb_base + _fb_width*_fb_height*sizeof(PT)),
			_view_cap(_nitpicker.create_view()),
			_view(_view_cap),
			_clip(Nano3d::Point(0, 0), Nano3d::Point(_fb_width - 1, _fb_height/2 - 1)),
			_canvas_1((PT *)_fb_base, _fb_alpha, _fb_width*_fb_height/2,
			          Nano3d::Area(_fb_width, _fb_height/2), _l_edge, _r_edge),
			_canvas_2((PT *)_fb_base + _backbuffer_offset(),
			          _fb_alpha + _backbuffer_offset(), _fb_width*_fb_height/2,
			          Nano3d::Area(_fb_width, _fb_height/2), _l_edge, _r_edge),
			_canvas_front(&_canvas_1),
			_canvas_back(&_canvas_2),
			_outer_dodecahedron(10000),
			_inner_dodecahedron(3000),
			_frame(0)
		{
			Nano3d::init_sincos_tab();

			_view.viewport(_xpos, _ypos, _fb_width, _fb_height/2, 0, 0, false);
			_view.stack(Nitpicker::View_capability(), true, true);
			_canvas_1.clip(_clip);
			_canvas_2.clip(_clip);
		}

		void render()
		{
			_canvas_back->clear();

			_outer_dodecahedron.draw(_canvas_back, _frame, true);
			_inner_dodecahedron.draw(_canvas_back, _frame, true);
			_inner_dodecahedron.draw(_canvas_back, _frame, false);
			_outer_dodecahedron.draw(_canvas_back, _frame, false);

			_frame++;
			if (_frame >= 1024)
				_frame = 0;

			int buf_y = 0;
			if (_canvas_back->addr() != _fb_base)
				buf_y -= _fb_height/2;
			_view.viewport(_xpos, _ypos, _fb_width, _fb_height/2, 0, buf_y, true);

			/* swap back and front canvas */
			Nano3d::Canvas_rgb565 *tmp = _canvas_back;
			_canvas_back = _canvas_front;
			_canvas_front = tmp;
		}
};


int main(int argc, char **argv)
{
	Nano3d::init_sincos_tab();
	static Timer::Connection timer;

	static Process_tree process_tree(10, 10, 1000, 720);

	while (1) {

		timer.msleep(10);

		process_tree.render();

	}
	return 0;
}
