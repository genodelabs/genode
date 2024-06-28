/*
 * \brief  Animated cube
 * \author Norman Feske
 * \date   2015-06-26
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/heap.h>
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <polygon_gfx/shaded_polygon_painter.h>
#include <polygon_gfx/textured_polygon_painter.h>
#include <os/pixel_rgb888.h>
#include <nano3d/dodecahedron_shape.h>
#include <nano3d/cube_shape.h>
#include <nano3d/scene.h>
#include <nano3d/sqrt.h>


template <typename PT>
class Scene : public Nano3d::Scene<PT>
{
	public:

		enum Shape { SHAPE_DODECAHEDRON, SHAPE_CUBE };
		enum Painter { PAINTER_SHADED, PAINTER_TEXTURED };

	private:

		Genode::Env  &_env;
		Genode::Heap  _heap { _env.ram(), _env.rm() };

		Gui::Area const _size;

		struct Radial_texture
		{
			enum { W = 128, H = 128 };

			unsigned char              alpha[H][W];
			PT                         pixel[H][W];
			Genode::Surface_base::Area size { W, H };
			Genode::Texture<PT>        texture { &pixel[0][0], &alpha[0][0], size };

			Radial_texture()
			{
				int const r_max = W/2 + 5;

				for (unsigned y = 0; y < H; y++) {
					for (unsigned x = 0; x < W; x++) {

						int const dx = x - W/2;
						int const dy = y - H/2;

						int const radius = Nano3d::sqrt(dx*dx + dy*dy);

						alpha[y][x] = (unsigned char)(250 - (radius*250)/r_max);

						if ((x&4) ^ (y&4))
							alpha[y][x] = 0;

						int const r = (x*200)/W;
						int const g = (y*200)/H;
						int const b = (x*128)/W + (y*128)/H;

						pixel[y][x] = PT(r, g, b);
					}
				}
			}
		};

		Radial_texture _texture { };

		Shape   _shape   = SHAPE_DODECAHEDRON;
		Painter _painter = PAINTER_TEXTURED;

		Genode::Attached_rom_dataspace _config { _env, "config" };

		void _handle_config()
		{
			_config.update();

			using Value = Genode::String<32>;

			_shape = SHAPE_DODECAHEDRON;
			if (_config.xml().attribute_value("shape", Value()) == "cube")
				_shape = SHAPE_CUBE;

			_painter = PAINTER_TEXTURED;
			if (_config.xml().attribute_value("painter", Value()) == "shaded")
				_painter = PAINTER_SHADED;
		}

		Genode::Signal_handler<Scene> _config_handler;

	public:

		Scene(Genode::Env &env, Genode::uint64_t update_rate_ms,
		      Gui::Point pos, Gui::Area size)
		:
			Nano3d::Scene<PT>(env, update_rate_ms, pos, size),
			_env(env), _size(size),
			_config_handler(env.ep(), *this, &Scene::_handle_config)
		{
			_config.sigh(_config_handler);
			_handle_config();
		}

	private:

		Polygon::Shaded_painter   _shaded_painter   { _heap, _size.h };
		Polygon::Textured_painter _textured_painter { _heap, _size.h };

		Nano3d::Cube_shape         const _cube         { 7000 };
		Nano3d::Dodecahedron_shape const _dodecahedron { 10000 };

		template <typename SHAPE>
		void _render_shape(Genode::Surface<PT>                   &pixel,
		                   Genode::Surface<Genode::Pixel_alpha8> &alpha,
		                   SHAPE const &shape, unsigned frame,
		                   bool backward_facing)
		{
			using Color = Genode::Color;

			auto vertices = shape.vertex_array();

			vertices.rotate_x(frame*1);
			vertices.rotate_y(frame*2);
			vertices.rotate_z(frame*3);
			vertices.project(1600, 800);
			vertices.translate(200, 200, 0);

			if (_painter == PAINTER_TEXTURED) {

				using Textured_point = Polygon::Textured_painter::Point;

				shape.for_each_face([&] (unsigned const vertex_indices[],
				                         unsigned num_vertices) {

					Textured_point points[num_vertices];

					int angle = -frame*4;
					for (unsigned i = 0; i < num_vertices; i++) {

						Nano3d::Vertex const vertex = vertices[vertex_indices[i]];

						Textured_point &point =
							backward_facing ? points[num_vertices - 1 - i]
							                : points[i];

						int const r = _texture.size.w/2;

						int const u = r + (r*Nano3d::cos_frac16(angle) >> 16);
						int const v = r + (r*Nano3d::sin_frac16(angle) >> 16);

						angle += Nano3d::Sincos_frac16::STEPS / num_vertices;

						point = Textured_point(vertex.x(), vertex.y(), u, v);
					}

					_textured_painter.paint(pixel, alpha, points, num_vertices,
					                        _texture.texture);
				});
			}

			if (_painter == PAINTER_SHADED) {

				using Shaded_point = Polygon::Shaded_painter::Point;

				shape.for_each_face([&] (unsigned const vertex_indices[],
				                         unsigned num_vertices) {

					Shaded_point points[num_vertices];

					for (unsigned i = 0; i < num_vertices; i++) {

						Nano3d::Vertex const v = vertices[vertex_indices[i]];

						Shaded_point &point =
							backward_facing ? points[num_vertices - 1 - i]
							                : points[i];

						Color const color =
							backward_facing ? Color::clamped_rgba(i*10, i*10, i*10, 230 - i*18)
							                : Color::clamped_rgba(240, 10*i, 0, 10 + i*35);

						point = Shaded_point(v.x(), v.y(), color);
					}

					_shaded_painter.paint(pixel, alpha,
					                      points, num_vertices);
				});
			}
		}

	public:

		/**
		 * Scene interface
		 */
		void render(Genode::Surface<PT>                   &pixel,
		            Genode::Surface<Genode::Pixel_alpha8> &alpha) override
		{
			unsigned const frame = (this->elapsed_ms()/10) % 1024;

			if (_shape == SHAPE_DODECAHEDRON) {

				_render_shape(pixel, alpha, _dodecahedron, frame, true);
				_render_shape(pixel, alpha, _dodecahedron, frame, false);

			} else if (_shape == SHAPE_CUBE) {

				_render_shape(pixel, alpha, _cube, frame, true);
				_render_shape(pixel, alpha, _cube, frame, false);
			}
		}
};


void Component::construct(Genode::Env &env)
{
	enum { UPDATE_RATE_MS = 20 };

	static Scene<Genode::Pixel_rgb888>
		scene(env, UPDATE_RATE_MS,
		      Gui::Point(-200, -200), Gui::Area(400, 400));
}
