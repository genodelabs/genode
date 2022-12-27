/*
 * \brief  Public interface for libdrm
 * \author Sebastian Sumpf
 * \date   2022-01-14
 *
 * This is a interim solution becaues libdrm currently needs access to the
 * GPU::Connection and Genode::Env. Once all the necessary features are provided
 * by this plugin this interface is removed.
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VFS_GPU_H_
#define _VFS_GPU_H_

namespace Gpu { class Connection; }

Gpu::Connection *vfs_gpu_connection(unsigned long);

namespace Genode { class Env; }

Genode::Env *vfs_gpu_env();

#endif /* _VFS_GPU_H_ */
