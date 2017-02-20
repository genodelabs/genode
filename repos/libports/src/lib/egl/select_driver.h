/*
 * \brief  Probe GPU device to select the proper Gallium3D driver
 * \author Norman Feske
 * \date   2010-09-23
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SELECT_DRIVER_H_
#define _SELECT_DRIVER_H_

/**
 * Probe GPU and determine the shared object name of the Gallium3D driver
 *
 * \return  ASCII string with the name of the Gallium3D driver, or
 *          0 if detection failed
 */
const char *probe_gpu_and_select_driver();

#endif /* _SELECT_DRIVER_H_ */
