/*
 * \brief  Cube 3D object
 * \author Norman Feske
 * \date   2015-06-19
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__NANO3D__CUBE_SHAPE_H_
#define _INCLUDE__NANO3D__CUBE_SHAPE_H_

#include <nano3d/vertex_array.h>

namespace Nano3d { class Cube_shape; }


class Nano3d::Cube_shape
{
	private:

		enum { NUM_VERTICES = 8, NUM_FACES = 6 };

		typedef Nano3d::Vertex_array<NUM_VERTICES> Vertex_array;

		Vertex_array _vertices;

		enum { VERTICES_PER_FACE = 4 };

		typedef unsigned Face[VERTICES_PER_FACE];

		Face _faces[NUM_FACES] { { 0, 1, 3, 2 },
		                         { 6, 7, 5, 4 },
		                         { 1, 0, 4, 5 },
		                         { 3, 1, 5, 7 },
		                         { 2, 3, 7, 6 },
		                         { 0, 2, 6, 4 } };

	public:

		Cube_shape(int size)
		{
			for (unsigned i = 0; i < NUM_VERTICES; i++)
				_vertices[i] = Nano3d::Vertex((i&1) ? size : -size,
				                              (i&2) ? size : -size,
				                              (i&4) ? size : -size);
		}

		Vertex_array const &vertex_array() const { return _vertices; }

		/**
		 * Call functor 'fn' for each face of the object
		 *
		 * The functor is called with an array of 'unsigned' vertex indices
		 * and the number of indices as arguments.
		 */
		template <typename FN>
		void for_each_face(FN const &fn) const
		{
			for (unsigned i = 0; i < NUM_FACES; i++)
				fn(_faces[i], VERTICES_PER_FACE);
		}
};

#endif /* _INCLUDE__NANO3D__CUBE_SHAPE_H_ */
