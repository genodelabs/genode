/*
 * \brief  Advertise OpenGL support
 * \author Norman Feske
 * \date   2010-07-08
 *
 * The supported APIs are determined at runtime by querying symbols called
 * 'st_api_<API>' via 'dlopen'. The OpenGL API is supported by the Mesa state
 * tracker. To use it, we only need to make the corresponding symbol appear
 * in our Gallium EGL driver.
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

const int st_api_OpenGL = 1;

