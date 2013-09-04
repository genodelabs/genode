/*
 * \brief  Animated banner
 * \author Norman Feske
 * \date   2010-09-27
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _BANNER_H_
#define _BANNER_H_

/* Genode includes */
#include <nitpicker_session/connection.h>
#include <nitpicker_view/client.h>
#include <framebuffer_session/client.h>
#include <base/printf.h>

/* Nano3D includes */
#include <nano3d/canvas_rgb565.h>

/* local includes */
#include "texture.h"
#include <util/lazy_value.h>


enum { TILE_SIZE = 32U };


struct Vertex
{
	int x, y, z;

	/**
	 * Constructor
	 */
	Vertex(int init_x, int init_y, int init_z)
	: x(init_x), y(init_y), z(init_z) { }
};


enum { MAX_VERTICES_PER_FACE = 4 };
class Face
{
	private:

		unsigned _num_vertices;
		int _vertex_indices[MAX_VERTICES_PER_FACE];

	public:

		/**
		 * Constructor
		 */
		Face(int idx1, int idx2, int idx3, int idx4)
		: _num_vertices(4)
		{
			_vertex_indices[0] = idx1;
			_vertex_indices[1] = idx2;
			_vertex_indices[2] = idx3;
			_vertex_indices[3] = idx4;
		}

		/**
		 * Default constructor creates invalid face
		 */
		Face() : _num_vertices(0) { }

		/**
		 * Return number of face vertices
		 */
		unsigned num_vertices() { return _num_vertices; }

		/**
		 * Return vertex index of face point
		 *
		 * \param i  Index of face point. If 'i' is out of
		 *           range, the first face point is returned.
		 */
		int vertex_index(unsigned i) {
			return (i < _num_vertices) ? _vertex_indices[i]
			                           : _vertex_indices[0]; }
};

namespace Cube
{
	enum {
		SIZE         = (TILE_SIZE << 5) + 15,
		NUM_VERTICES = 8,
		NUM_FACES    = 6,
	};

	enum Side { FRONT, RIGHT, BACK, LEFT, TOP, BOTTOM };

	static Vertex vertices[] =
	{
		Vertex(-SIZE, -SIZE, -SIZE),
		Vertex(-SIZE, -SIZE,  SIZE),
		Vertex(-SIZE,  SIZE,  SIZE),
		Vertex(-SIZE,  SIZE, -SIZE),
		Vertex( SIZE, -SIZE, -SIZE),
		Vertex( SIZE, -SIZE,  SIZE),
		Vertex( SIZE,  SIZE,  SIZE),
		Vertex( SIZE,  SIZE, -SIZE),
	};

	static Nano3d::Color colors[] =
	{
		Nano3d::Color(  0,   0,   0,   0),
		Nano3d::Color(  0,   0, 255),
		Nano3d::Color(  0, 255,   0),
		Nano3d::Color(255,   0,   0),
		Nano3d::Color(255, 255, 255),
		Nano3d::Color(0  ,   0,   0),
	};

	static Face faces[] =
	{
		Face(3, 7, 4, 0), /* front  */
		Face(5, 4, 7, 6), /* right  */
		Face(6, 2, 1, 5), /* back   */
		Face(1, 2, 3, 0), /* left   */
		Face(5, 1, 0, 4), /* top    */
		Face(6, 7, 3, 2), /* bottom */
	};
}


template <unsigned MAX_VERTICES>
class Vertex_array
{
	protected:

		int      _x_buf[MAX_VERTICES];
		int      _y_buf[MAX_VERTICES];
		int      _z_buf[MAX_VERTICES];
		unsigned _num_vertices;

		void _rotate(int *x_buf, int *y_buf, int angle)
		{
			using namespace Nano3d;

			int sina = sin(angle);
			int cosa = cos(angle);
			int x, y;

			for (unsigned i = 0; i < _num_vertices; i++, x_buf++, y_buf++) {
				x = (*x_buf * cosa + *y_buf * sina) >> 16;
				y = (*x_buf * sina - *y_buf * cosa) >> 16;
				*x_buf = x;
				*y_buf = y;
			}
		}

	public:

		/**
		 * Constructor
		 */
		Vertex_array(Vertex *vertices, unsigned num_vertices)
		{
			/* import vertices */
			_num_vertices = Nano3d::min(num_vertices, MAX_VERTICES);
			for (unsigned i = 0; i < _num_vertices; i++, vertices++) {
				_x_buf[i] = vertices->x;
				_y_buf[i] = vertices->y;
				_z_buf[i] = vertices->z;
			}
		}

		/**
		 * Rotate vertices around x, y, and z axis
		 */
		void rotate_x(int angle) { _rotate(_y_buf, _z_buf, angle); }
		void rotate_y(int angle) { _rotate(_x_buf, _z_buf, angle); }
		void rotate_z(int angle) { _rotate(_x_buf, _y_buf, angle); }

		/**
		 * Apply central projection to vertices
		 *
		 * FIXME: Add proper documentation of the parameters.
		 *
		 * \param z_shift   Recommended value is 1600
		 * \param distance  Recommended value is screen height
		 */
		void projection(int z_shift, int distance)
		{
			int *x_buf = _x_buf, *y_buf = _y_buf, *z_buf = _z_buf;
			for (unsigned i = 0; i < _num_vertices; i++, x_buf++, y_buf++, z_buf++) {
				int z = (*z_buf >> 5) + z_shift - 1;

				/* avoid division by zero */
				if (z == 0) z += 1;

				*x_buf = ((*x_buf >> 5) * distance) / z;
				*y_buf = ((*y_buf >> 5) * distance) / z;
			}
		}

		/**
		 * Translate vertices
		 */
		void translate(int dx, int dy, int dz)
		{
			int *x_buf = _x_buf, *y_buf = _y_buf, *z_buf = _z_buf;
			for (unsigned i = 0; i < _num_vertices; i++, x_buf++, y_buf++, z_buf++) {
				*x_buf += dx;
				*y_buf += dy;
				*z_buf += dz;
			}
		}
};


template <unsigned MAX_FACES>
class Face_topology
{
	protected:

		Face     _faces[MAX_FACES];
		unsigned _num_faces;

	public:

		/**
		 * Constructor
		 */
		Face_topology(Face *faces, unsigned num_faces)
		{
			/* import face topology */
			_num_faces = Nano3d::min(num_faces, MAX_FACES);
			Face *src = faces, *dst = _faces;
			for (unsigned i = 0; i < num_faces; i++)
				*dst++ = *src++;
		}
};


template <typename PT, int MAX_VERTICES, int MAX_FACES>
class Textured_object : public Vertex_array<MAX_VERTICES>,
                        public Face_topology<MAX_FACES>
{
	private:

		Nano3d::Canvas::Texture *_texture[MAX_FACES];

	public:

		/**
		 * Constructor
		 */
		Textured_object(Vertex *vertices, unsigned num_vertices,
		                Face   *faces,    unsigned num_faces)
		:
			Vertex_array<MAX_VERTICES>(vertices, num_vertices),
			Face_topology<MAX_FACES>(faces, num_faces)
		{
			for (unsigned i = 0; i < MAX_FACES; i++)
				_texture[i] = 0;
		}

		void assign_texture(unsigned face, Nano3d::Canvas::Texture *texture)
		{
			if (face < MAX_FACES)
				_texture[face] = texture;
		}

		void draw(Nano3d::Chunky_canvas<PT> *dst)
		{
			using namespace Nano3d;

			for (unsigned i = 0; i < Face_topology<MAX_FACES>::_num_faces; i++) {
				Textured_polypoint polygon[MAX_VERTICES_PER_FACE];

				Face *f = &Face_topology<MAX_FACES>::_faces[i];
				for (unsigned j = 0; j < f->num_vertices(); j++) {
					int v = f->vertex_index(j);
					Point texpos;
					switch (j) {
					default:
					case 0: texpos = Point(0, 0); break;
					case 1: texpos = Point(TILE_SIZE, 0); break;
					case 2: texpos = Point(TILE_SIZE, TILE_SIZE); break;
					case 3: texpos = Point(0, TILE_SIZE); break;
					}
					polygon[j] = Textured_polypoint(Vertex_array<MAX_VERTICES>::_x_buf[v],
					                                Vertex_array<MAX_VERTICES>::_y_buf[v],
					                                texpos);
				}
				if (_texture[i])
					dst->draw_textured_polygon(polygon, f->num_vertices(), _texture[i]);
			}
		}
};


class View_angle
{
	enum { ALPHA = 0, BETA = 1, GAMMA = 2 };
	Lazy_value<int> _angle[3];
	int _lazyness;

	public:

		/**
		 * Constructor
		 */
		View_angle(int alpha, int beta, int gamma)
		:
			_lazyness(10 << 4)
		{
			_angle[ALPHA] = alpha << 4;
			_angle[BETA]  = beta  << 4;
			_angle[GAMMA] = gamma << 4;
		}

		int alpha() const { return _angle[ALPHA] >> 4; }
		int beta()  const { return _angle[BETA]  >> 4; }
		int gamma() const { return _angle[GAMMA] >> 4; }

		void lazyness(int lazyness) { _lazyness = lazyness; }

		void dst(int dst_alpha, int dst_beta, int dst_gamma)
		{
			int new_dst_angle[3] = { dst_alpha << 4,
			                         dst_beta  << 4,
			                         dst_gamma << 4 };

			enum { R360 = 1024 << 4 };

			for (unsigned i = 0; i < 3; i++)
				while (new_dst_angle[i] >= R360
				 && _angle[i] >= R360) {
				 	new_dst_angle[i] -= R360;
				 	_angle[i].assign(_angle[i] - R360);
				}

			for (unsigned i = 0; i < 3; i++)
				_angle[i].dst(new_dst_angle[i], _lazyness);
		}

		void animate() {
			for (unsigned i = 0; i < 3; i++)
				_angle[i].animate();
		}
};


template <typename PT>
class Cube_sprite : private Textured_object<PT, 8, 6>
{
	private:

		int _tile_x, _tile_y;
		View_angle _view_angle;

		enum { MAX_DISTANCE = 18000 };
		Lazy_value<int> _distance;
		int _distance_lazyness;

		Lazy_value<int> _scale;
		int _scale_lazyness;

		void _direct_view_angle(Cube::Side side)
		{
			enum { R90 = 256, R180 = 512, R270 = R90 + R180 };
			switch (side) {
			default:
			case Cube::FRONT:  _view_angle.dst(0,    0,    0); break;
			case Cube::RIGHT:  _view_angle.dst(0, R270, R180); break;
			case Cube::BACK:   _view_angle.dst(R180, 0, R180); break;
			case Cube::LEFT:   _view_angle.dst(0,  R90,   R90); break;
			case Cube::TOP:    _view_angle.dst(R270, 0, R180); break;
			case Cube::BOTTOM: _view_angle.dst(R90,  0, R270); break;
			}
		}

	public:

		using Textured_object<PT, 8, 6>::draw;
		using Textured_object<PT, 8, 6>::assign_texture;

		/**
		 * Constructor
		 */
		Cube_sprite(unsigned tile_x, unsigned tile_y)
		:
			Textured_object<PT, 8, 6>(Cube::vertices, Cube::NUM_VERTICES,
			                          Cube::faces,    Cube::NUM_FACES),
			_tile_x(tile_x), _tile_y(tile_y),
			_view_angle(256, 768, 512),
			_distance(0),
			_distance_lazyness(0),
			_scale(0),
			_scale_lazyness(0)
		{ }

		void lazyness(int lazyness) {
			_distance_lazyness = lazyness;
			_scale_lazyness = lazyness;
			_view_angle.lazyness(lazyness);
		}

		void calculate()
		{
			/* reset sprite coordinates */
			*(static_cast<Vertex_array<8> *>(this)) =
				Vertex_array<8>(Cube::vertices, Cube::NUM_VERTICES);

			/* apply vertex transformations */
			Vertex_array<8>::rotate_x(_view_angle.alpha());
			Vertex_array<8>::rotate_y(_view_angle.beta());
			Vertex_array<8>::rotate_z(_view_angle.gamma());

			int distance = _distance;
			if (distance > MAX_DISTANCE)
				distance = MAX_DISTANCE*2 - distance;

			Vertex_array<8>::translate(0, 0, distance);

			Vertex_array<8>::projection(800 + TILE_SIZE - 1, _scale);

			Vertex_array<8>::translate(_tile_x*TILE_SIZE + TILE_SIZE,
			                           _tile_y*TILE_SIZE + TILE_SIZE, 0);
		}

		void animate()
		{
			_view_angle.animate();
			_distance.animate();
			_scale.animate();
		}

		void hide()
		{
			_scale.dst(0, _scale_lazyness);
			_view_angle.dst(_view_angle.alpha() + 64,
			                _view_angle.beta() + 128,
			                _view_angle.gamma() + 92);
		}

		bool visible() { return _scale > 0; }

		void show(Cube::Side side)
		{
			_direct_view_angle(side);
			_scale.dst(400, _scale_lazyness);

			if (_distance.dst() == 0)
				_distance.dst(MAX_DISTANCE*2, _distance_lazyness);
			else
				_distance.dst(0, _distance_lazyness);
		}
};


class Heap_allocator : public Nano3d::Allocator
{
	public:
		void *alloc(unsigned long size) { return Genode::env()->heap()->alloc(size); }
		void  free(void *ptr, unsigned long size) { Genode::env()->heap()->free(ptr, size); }
};


void *operator new (unsigned int size) { return Genode::env()->heap()->alloc(size); }
void operator delete (void *ptr, unsigned int size) { Genode::env()->heap()->free(ptr, size); }


typedef Nano3d::Pixel_rgb565 PT;


/**
 * Extract tile from large texture
 *
 * \param src   large source texture
 * \param dst   tile-sized destination texture
 * \param x, y  tile position, specified as horizontal and
 *              vertical tile indices
 */
void extract_tile_texture(Nano3d::Canvas_rgb565::Texture *src,
                          Nano3d::Canvas_rgb565::Texture *dst, int x, int y)
{
	unsigned long offset = y*TILE_SIZE*src->w() + x*TILE_SIZE;

	Nano3d::Pixel_rgb565 *src_pixel = src->pixel() + offset;
	unsigned char        *src_alpha = src->alpha() + offset;
	Nano3d::Pixel_rgb565 *dst_pixel = dst->pixel();
	unsigned char        *dst_alpha = dst->alpha();

	unsigned x_max = Nano3d::min((unsigned)TILE_SIZE, src->w() - (unsigned)x*TILE_SIZE);
	unsigned y_max = Nano3d::min((unsigned)TILE_SIZE, src->h() - (unsigned)y*TILE_SIZE);

	/* clear dst */
	for (unsigned y = 0; y < dst->w(); y++) {
		for (unsigned x = 0; x < dst->h(); x++) {
			dst_pixel[y*dst->w() + x] = Nano3d::Pixel_rgb565(255,0,0);
			dst_alpha[y*dst->w() + x] = 255;
		}
	}

	/*
	 * The dst texture is one pixel wider/higher than the tile
	 * size to account for fixpoint rounding artifacts. To fill
	 * the last row/column of the dst texture with useful pixel
	 * data, we assign each src pixel not only to a single dst
	 * pixel but to its 2x2 neighbors as well.
	 */

	/* copy tile */
	for (unsigned y = 0; y < y_max; y++) {
		for (unsigned x = 0; x < x_max; x++) {
			dst_pixel[x]                = src_pixel[x];
			dst_pixel[x + 1]            = src_pixel[x];
			dst_pixel[x + dst->w()]     = src_pixel[x];
			dst_pixel[x + dst->w() + 1] = src_pixel[x];
			dst_alpha[x]                = src_alpha[x];
			dst_alpha[x + 1]            = src_alpha[x];
			dst_alpha[x + dst->w()]     = src_alpha[x];
			dst_alpha[x + dst->w() + 1] = src_alpha[x];
		}
		dst_pixel += dst->w();
		dst_alpha += dst->w();
		src_pixel += src->w();
		src_alpha += src->w();
	}
}


class Banner
{
	public:

		class Unsupported_color_depth { };

		enum { INITIAL = Cube::NUM_FACES - 1, NO_BANNER = Cube::NUM_FACES + 1 };

	private:

		/**
		 * Screen position of the banner
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

		unsigned _num_h_sprites() { return _width/TILE_SIZE; }
		unsigned _num_v_sprites() { return _height/TILE_SIZE; }

		enum { MAX_V_SPRITES = 100, MAX_H_SPRITES = 100 };
		Cube_sprite<PT> *_sprites[MAX_V_SPRITES][MAX_H_SPRITES];

		Nano3d::Canvas_rgb565::Texture *_full;

		int _curr_side;

	public:

		Banner(long xpos, long ypos, long width, long height)
		:
			_xpos(xpos), _ypos(ypos), _width(width), _height(height),
			_nitpicker(true),
			_mode(_width + TILE_SIZE, 2*(_height + TILE_SIZE), Framebuffer::Mode::RGB565),
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
			_full(_canvas_1.alloc_texture(&_alloc, Nano3d::Area(_width, _height))),
			_curr_side(Cube::FRONT)
		{
			Nano3d::init_sincos_tab();

			_view.viewport(_xpos, _ypos, _fb_width, _fb_height/2, 0, 0, false);
			_view.stack(Nitpicker::View_capability(), true, true);
			_canvas_1.clip(_clip);
			_canvas_2.clip(_clip);

			for (unsigned y = 0; y < _num_v_sprites(); y++)
				for (unsigned x = 0; x < _num_h_sprites(); x++) {
					_sprites[y][x] = new Cube_sprite<PT>(x, y);
					_sprites[y][x]->lazyness(25 + (TILE_SIZE*TILE_SIZE*(x*x+y*y))/(7*1024));
					_sprites[y][x]->show((Cube::Side)_curr_side);
				}

			if (_num_v_sprites() > 8 && _num_h_sprites() > 11) {
				_sprites[5][7]->lazyness(70);
				_sprites[7][10]->lazyness(85);
			}
		}

		void assign_png_to_cube_face(void *png_image_data, unsigned face)
		{
			/* read png data into texture */
			Png_image png(png_image_data);
			png.convert_to_rgb565((Genode::uint16_t *)_full->pixel(),
			                      _full->alpha(), _full->w(), _full->h());

			/*
			 * XXX
			 *
			 * Free textures formerly assigned to the cube faces.
			 * However, this is only important if the code is used in a
			 * dynamic fashion, which is not the case for now.
			 */

			/* cut png texture into tiles and assign them as cube textures */
			for (unsigned y = 0; y < _num_v_sprites(); y++) {
				for (unsigned x = 0; x < _num_h_sprites(); x++) {

					/* allocate tile-sized texture */
					Nano3d::Canvas_rgb565::Texture *tile;
					tile = _canvas_1.alloc_texture(&_alloc, Nano3d::Area(TILE_SIZE + 1, TILE_SIZE + 1));

					extract_tile_texture(_full, tile, x, y);
					_sprites[y][x]->assign_texture(face, tile);
				}
			}
		}

		void render()
		{
			_canvas_back->clear();

			for (unsigned y = _num_v_sprites(); y > 0; y--) {
				for (unsigned x = _num_h_sprites(); x > 0; x--) {
					_sprites[y - 1][x - 1]->calculate();
					_sprites[y - 1][x - 1]->draw(_canvas_back);
				}
			}

			/* check if the nitpicker view must be visibile */
			bool any_cube_visible = false;
			for (unsigned y = 0; y < _num_v_sprites(); y++)
				for (unsigned x = 0; x < _num_h_sprites(); x++)
					any_cube_visible |= _sprites[y][x]->visible();

			int buf_y = 0;
			if (_canvas_back->addr() != _fb_base)
				buf_y -= _fb_height/2;
			_view.viewport(any_cube_visible ? _xpos : -20000, _ypos, _fb_width, _fb_height/2,
			               0, buf_y, true);

			/* swap back and front canvas */
			Nano3d::Canvas_rgb565 *tmp = _canvas_back;
			_canvas_back = _canvas_front;
			_canvas_front = tmp;
		}

		void animate(unsigned passed_frames)
		{
			for (unsigned y = 0; y < _num_v_sprites(); y++)
				for (unsigned x = 0; x < _num_h_sprites(); x++)
					for (unsigned i = 0; i < passed_frames; i++)
						_sprites[y][x]->animate();
		}

		void show(unsigned face)
		{
			for (unsigned y = 0; y < _num_v_sprites(); y++)
				for (unsigned x = 0; x < _num_h_sprites(); x++)
					if (face < Cube::NUM_FACES)
						_sprites[y][x]->show((Cube::Side)face);
					else
						_sprites[y][x]->hide();
		}
};

#endif /* _BANNER_H_ */
