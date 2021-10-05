/*
 * \brief  VM mouse shape support
 * \author Christian Prochaska
 * \author Sebastian Sumpf
 * \date   2021-10-05
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _MOUSE_SHAPE_H_
#define _MOUSE_SHAPE_H_

#include <report_session/connection.h>
#include <pointer/shape_report.h>


class Mouse_shape
{
	private:

		Genode::Env &_env;

		Report::Connection _report_connection { _env, "shape",
			sizeof(Pointer::Shape_report) };

		Genode::Attached_dataspace _report_ds { _env.rm(),
			_report_connection.dataspace() };

		Pointer::Shape_report *_shape_report {
			_report_ds.local_addr<Pointer::Shape_report>() };

	public:

		Mouse_shape(Genode::Env &env)
		  : _env(env)
		{ }

		void update(BOOL visible,
		            BOOL alpha,
		            ULONG xHot,
		            ULONG yHot,
		            ULONG width,
		            ULONG height,
		            ComSafeArrayIn(BYTE,inShape))
		{
			com::SafeArray <BYTE> aShape(ComSafeArrayInArg(inShape));

			if (visible && ((width == 0) || (height == 0)))
				return;

			_shape_report->visible = visible;
			_shape_report->x_hot   = xHot;
			_shape_report->y_hot   = yHot;
			_shape_report->width   = width;
			_shape_report->height  = height;

			unsigned int and_mask_size = (_shape_report->width + 7) / 8 *
			                              _shape_report->height;

			const unsigned char *and_mask = aShape.raw();

			const unsigned char *shape = and_mask + ((and_mask_size + 3) & ~3);

			size_t shape_size = aShape.size() - (shape - and_mask);

			if (shape_size > Pointer::MAX_SHAPE_SIZE) {
				Genode::error("mouse shape: data buffer too small for ",
				              shape_size, " bytes");
				return;
			}

			/* convert the shape data from BGRA encoding to RGBA encoding */

			unsigned char const *bgra_shape = shape;
			unsigned char       *rgba_shape = _shape_report->shape;

			for (unsigned int y = 0; y < _shape_report->height; y++) {

				unsigned char const *bgra_line = &bgra_shape[y * _shape_report->width * 4];
				unsigned char *rgba_line       = &rgba_shape[y * _shape_report->width * 4];

				for (unsigned int i = 0; i < _shape_report->width * 4; i += 4) {
					rgba_line[i + 0] = bgra_line[i + 2];
					rgba_line[i + 1] = bgra_line[i + 1];
					rgba_line[i + 2] = bgra_line[i + 0];
					rgba_line[i + 3] = bgra_line[i + 3];
				}
			}

			if (visible && !alpha) {

				for (unsigned int i = 0; i < width * height; i++) {

					unsigned int *color =
						&((unsigned int*)_shape_report->shape)[i];

					/* heuristic from VBoxSDL.cpp */

					if (and_mask[i / 8] & (1 << (7 - (i % 8)))) {

						if (*color & 0x00ffffff)
							*color = 0xff000000;
						else
							*color = 0x00000000;

					} else
						*color |= 0xff000000;
				}
			}

			_report_connection.submit(sizeof(Pointer::Shape_report));
		}
};

#endif /* _MOUSE_SHAPE_H_ */
