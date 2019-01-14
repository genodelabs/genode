/*
 * \brief  Dodecahedron 3D object
 * \author Norman Feske
 * \date   2015-06-19
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__NANO3D__DODECAHEDRON_SHAPE_H_
#define _INCLUDE__NANO3D__DODECAHEDRON_SHAPE_H_

#include <nano3d/vertex_array.h>

namespace Nano3d { class Dodecahedron_shape; }


class Nano3d::Dodecahedron_shape
{
	private:

		enum { NUM_VERTICES = 20, NUM_EDGES = 30, NUM_FACES = 12 };

		struct Edge
		{
			unsigned left_face, right_face;

			unsigned vertex[2];

			Edge() : left_face(0), right_face(0), vertex { 0, 0 } { }

			Edge(unsigned vertex_0, unsigned vertex_1,
			     unsigned left_face, unsigned right_face)
			:
				left_face(left_face), right_face(right_face),
				vertex { vertex_0, vertex_1 }
			{ }
		};

		class Face
		{
			public:

				enum { NUM_EDGES = 5 };

			private:

				int _edges[NUM_EDGES];

			public:

				Face() : _edges{} { }

				template <typename... EDGE_INDICES>
				Face(EDGE_INDICES... edge_indices)
				:
					_edges { edge_indices... }
				{ }

				static constexpr unsigned num_edges() { return NUM_EDGES; }

				int edge(unsigned i) const { return _edges[i]; }
		};

		typedef Nano3d::Vertex_array<NUM_VERTICES> Vertex_array;

		Vertex_array _vertices { };

		Edge _edges[NUM_EDGES];
		Face _faces[NUM_FACES];

		/* ratio of edge length to radius, as 16.16 fixpoint number */
		enum { A_TO_R = 46769 };

		/* angle between two edges, scaled to 0..1024 range */
		enum { DIHEDRAL_ANGLE = 332 };

	public:

		/**
		 * \param r  radius of the surrounding sphere
		 */
		Dodecahedron_shape(int r)
		{
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
				_vertices[j++] = Vertex((r0*sin_frac16(a)) >> 16, y0,
				                        (r0*cos_frac16(a)) >> 16);
			}

			/* level 2 */
			for (int i = 0; i < 5; i++) {
				int const a = i*ANGLE_STEP;
				_vertices[j++] = Vertex((r1*sin_frac16(a)) >> 16, y1,
				                        (r1*cos_frac16(a)) >> 16);
			}

			/* level 3 */
			for (int i = 0; i < 5; i++) {
				int const a = i*ANGLE_STEP + ANGLE_HALF_STEP;
				_vertices[j++] = Vertex((r1*sin_frac16(a)) >> 16, -y1,
				                        (r1*cos_frac16(a)) >> 16);
			}

			/* level 4 */
			for (int i = 0; i < 5; i++) {
				int const a = i*ANGLE_STEP + ANGLE_HALF_STEP;
				_vertices[j++] = Vertex((r0*sin_frac16(a)) >> 16, -y0,
				                        (r0*cos_frac16(a)) >> 16);
			}

			/*
			 * Edges
			 */

			j = 0; /* index into '_edges' array */

			/* level 1 */
			for (int i = 0; i < 5; i++)
				_edges[j++] = Edge(i, (i+1)%5, i + 1, 0);

			/* level 1 to level 2 */
			for (int i = 0; i < 5; i++)
				_edges[j++] = Edge(i, i + 5, 1 + (i + 4)%5, 1 + i);

			/* level 2 to level 3 */
			for (int i = 0; i < 5; i++)
				_edges[j++] = Edge(i+5, i + 10, 1 + 5 + (i + 4)%5, 1 + i);

			/* level 3 to level 2 */
			for (int i = 0; i < 5; i++)
				_edges[j++] = Edge(i + 10, (i + 1)%5 + 5, 1 + 5 + i, 1 + i);

			/* level 3 to level 4 */
			for (int i = 0; i < 5; i++)
				_edges[j++] = Edge(i + 10, i + 15, 1 + 5 + (i + 4)%5, 1 + 5 + i);

			/* level 4 */
			for (int i = 0; i < 5; i++)
				_edges[j++] = Edge(i + 15, (i + 1)%5 + 15, 11, 1 + 5 + i);

			/*
			 * Faces
			 */

			j = 0; /* index into '_faces' array */
			_faces[j++] = Face(0, 1, 2, 3, 4);

			for (int i = 0; i < 5; i++)
				_faces[j++] = Face(i, i + 5, i + 10, i + 15, 5 + (1 + i)%5);

			for (int i = 0; i < 5; i++)
				_faces[j++] = Face(i+20, i + 25, (i + 1)%5 + 20, 10 + (i + 1)%5, 15 + i);

			_faces[j++] = Face(29, 28, 27, 26, 25);
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
			for (unsigned i = 0; i < NUM_FACES; i++) {

				Face const face = _faces[i];

				/*
				 * Asssemble array of vertex indices for the current face.
				 */
				unsigned vertex_indices[Face::num_edges()];

				bool skip_face = false;

				for (unsigned j = 0; j < Face::num_edges(); j++) {

					Edge const edge = _edges[face.edge(j)];

					int vertex_idx = -1;

					if (edge.left_face == i)
						vertex_idx = edge.vertex[1];

					if (edge.right_face == i)
						vertex_idx = edge.vertex[0];

					if (vertex_idx == -1)
						skip_face = true;

					vertex_indices[j] = vertex_idx;
				}

				/* call functor with the information about the face vertices */
				if (!skip_face)
					fn(vertex_indices, Face::num_edges());
			}
		}
};

#endif /* _INCLUDE__NANO3D__DODECAHEDRON_SHAPE_H_ */
