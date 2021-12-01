/*
 * \brief  Soft float support
 * \author Sebastian Sumpf
 * \date   2021-06-29
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

extern "C"  {
	int __softfloat_float_rounding_mode;
	int __softfloat_float_exception_mask;
	int __softfloat_float_exception_flags;
}
