/*
 * \brief  Affine transform of 2-D coordinates
 * \author Christian Helmuth
 * \date   2024-01-28
 *
 * Affine transformation of coordinate (vector) x to y is the combination of
 * two operations.
 *
 * - linear map - matrix multiplication (A)
 * - translation - vector addition (b)
 *
 * y = A*x + b
 *
 * Both operations can be combined by using an augmented matrix that includes
 * the translation vector. A sequence of transformations can be combined into a
 * single transformation matrix by multiplication of respective matrices.
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _EVENT_FILTER__AFFINE_TRANSFORM_H_
#define _EVENT_FILTER__AFFINE_TRANSFORM_H_


namespace Transform {
	struct Matrix;
	struct Point;

	enum Angle { ANGLE_0, ANGLE_90, ANGLE_180, ANGLE_270 };

	static Angle angle_from_degrees(unsigned degrees)
	{
		switch (degrees) {
		case  90: return ANGLE_90;
		case 180: return ANGLE_180;
		case 270: return ANGLE_270;
		default:  return ANGLE_0;
		}
	}
}

/*
 * Augmented transformation matrix
 *
 *  | v11 v12 v13 |
 *  | v21 v22 v23 |
 *  |   0   0   1 |
 */
struct Transform::Matrix
{
	float v11, v12, v13,
	      v21, v22, v23;

	/*
	 * Identity transform (initial matrix)
	 */
	static Matrix identity()
	{
		return { .v11 = 1, .v12 = 0, .v13 = 0,
		         .v21 = 0, .v22 = 1, .v23 = 0 };
	}

	Matrix mul(Matrix const &t) const
	{
		return {
			.v11 = t.v11 * v11 + t.v12 * v21 + t.v13 * 0,
			.v12 = t.v11 * v12 + t.v12 * v22 + t.v13 * 0,
			.v13 = t.v11 * v13 + t.v12 * v23 + t.v13 * 1,
			.v21 = t.v21 * v11 + t.v22 * v21 + t.v23 * 0,
			.v22 = t.v21 * v12 + t.v22 * v22 + t.v23 * 0,
			.v23 = t.v21 * v13 + t.v22 * v23 + t.v23 * 1
		};
	}

	/*
	 * Translation by (x,y)
	 */
	Matrix translate(float x, float y) const
	{
		return mul(Matrix { 1, 0, x,
		                    0, 1, y});
	}

	/*
	 * Scaling by (x,y)
	 */
	Matrix scale(float x, float y) const
	{
		return mul(Matrix { x, 0, 0,
		                    0, y, 0});
	}

	/*
	 * Rotation clock-wise by 90, 180, 270 degrees
	 */
	Matrix rotate(Angle angle) const
	{
		/*
		 *      90°  180°  270°
		 * cos   0    -1     0
		 * sin   1     0    -1
		 */
		switch (angle) {
		case ANGLE_90:
			return mul(Matrix {  0, -1,  0,
			                     1,  0,  0 });
		case ANGLE_180:
			return mul(Matrix { -1,  0,  0,
			                     0, -1,  0 });
		case ANGLE_270:
			return mul(Matrix {  0,  1,  0,
			                    -1,  0,  0 });
		default:
			return *this;
		}
	}

	/*
	 * Reflection on (vertical) y-axis
	 */
	Matrix reflect_vertical_axis() const
	{
		return mul(Matrix { -1,  0,  0,
		                     0,  1,  0});
	}

	/*
	 * Reflection on (horizontal) x-axis
	 */
	Matrix reflect_horizontal_axis() const
	{
		return mul(Matrix {  1,  0,  0,
		                     0, -1,  0});
	}

	/*
	 * Rotate and adjust origin
	 */
	Matrix reorient(Angle angle, float width, float height) const
	{
		switch (angle) {
		case ANGLE_90:  return rotate(angle).translate(width - 1, 0);
		case ANGLE_180: return rotate(angle).translate(width - 1, height - 1);
		case ANGLE_270: return rotate(angle).translate(0, height - 1);
		default:        return *this;
		}
	}

	/*
	 * Flip (in vertical direction) and adjust origin
	 */
	Matrix vflip(float height) const
	{
		return reflect_horizontal_axis().translate(0, height - 1);
	}

	/*
	 * Flip (in horizontal direction) and adjust origin
	 */
	Matrix hflip(float width) const
	{
		return reflect_vertical_axis().translate(width - 1, 0);
	}
};


/*
 * Point as augmented vector
 *
 *  | x |
 *  | y |
 *  | 1 |
 */
struct Transform::Point
{
	float x, y;

	Point transform(Matrix const &t) const
	{
		return {
			.x = t.v11 * x + t.v12 * y + t.v13,
			.y = t.v21 * x + t.v22 * y + t.v23,
		};
	}

	int int_x() const
	{
		return x >= 0 ? int(x + 0.5) : -int(-x + 0.5);
	}

	int int_y() const
	{
		return y >= 0 ? int(y + 0.5) : -int(-y + 0.5);
	}
};

#endif /* _EVENT_FILTER__AFFINE_TRANSFORM_H_ */
