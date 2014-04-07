/*
 * \brief  Device Random filesystem
 * \author Josef Soentgen
 * \date   2012-07-31
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__RANDOM_FILE_SYSTEM_H_
#define _NOUX__RANDOM_FILE_SYSTEM_H_

/* Genode includes */
#include <base/printf.h>
#include <base/stdint.h>
#include <util/string.h>

/* Noux includes */
#include <noux_session/sysio.h>
#include "file_system.h"

/*-
 * Copyright (c) 2010, 2012
 *      Thorsten Glaser <tg@mirbsd.org>
 * Copyright (c) 2012
 *      Josef Soentgen <cnuke@mirbsd.org>
 *
 * Provided that these terms and disclaimer and all copyright notices
 * are retained or reproduced in an accompanying document, permission
 * is granted to deal in this work without restriction, including un-
 * limited rights to use, publicly perform, distribute, sell, modify,
 * merge, give away, or sublicence.
 *
 * This work is provided "AS IS" and WITHOUT WARRANTY of any kind, to
 * the utmost extent permitted by applicable law, neither express nor
 * implied; without malicious intent or gross negligence. In no event
 * may a licensor, author or contributor be held liable for indirect,
 * direct, other damage, loss, or other issues arising in any way out
 * of dealing in the work, even if advised of the possibility of such
 * damage or existence of a defect, except proven that it results out
 * of said person's immediate fault when using the work as intended.
 *-
 * arc4random for use as NOUX random device.
 *
 * From:
 * MirOS: src/kern/c/arcfour_base.c,v 1.1 2010/09/12 17:10:49 tg Exp
 * MirOS: src/kern/c/arcfour_ksa.c,v 1.1 2010/09/12 17:10:50 tg Exp
 * MirOS: src/lib/libc/crypt/arc4random_base.c,v 1.4 2011/07/06 22:22:09 tg Exp
 * MirOS: src/lib/libc/crypt/arc4random_buf.c,v 1.1 2010/09/12 17:10:53 tg Exp
 */


namespace Noux {

	using namespace Genode;

	/**
	 * Arcfour cipher re-implementation from (alledged) spec description.
	 */

	class Arc4random
	{
		private:

			uint8_t S[256];
			uint8_t i;
			uint8_t j;
			uint16_t num;
			uint8_t initialised;

			/*
			 * Base cipher operation: initialise state
			 */
			void init(void)
			{
				register uint8_t n = 0;

				do {
					S[n] = n;
				} while (++n);

				i = j = 0;
			}

			/*
			 * Base cipher operation: get byte of keystream.
			 */
			uint8_t byte(void)
			{
				register uint8_t si, sj;

				si = S[++i];
				j += si;
				sj = S[j];
				S[i] = sj;
				S[j] = si;
				return (S[(uint8_t)(si + sj)]);
			}

			/*
			 * Normal key scheduling algorithm.
			 */
			void ksa(const uint8_t *key, size_t keylen)
			{
				register uint8_t si, n = 0;

				--i;
				do {
					++i;
					si = S[i];
					j = (uint8_t)(j + si + key[n++ % keylen]);
					S[i] = S[j];
					S[j] = si;
				} while (n);
				j = ++i;
			}

			/*
			 * arc4random implementation
			 */
			void stir_unlocked(void)
			{
				register unsigned int m;
				uint8_t n;
				struct {
					//	struct timeval tv;
					//	pid_t mypid;
					uint32_t mypid;
					const void *stkptr, *bssptr, *txtptr;
					uint16_t num;
					uint8_t initialised;
					// FIXME sizeof (sb) should be as close to 256 as possible
				} sb;

				/* save some state; while not a secret, helps through variety */
				//sb.mypid = getpid();
				sb.mypid = 42;
				sb.stkptr = &sb;
				sb.bssptr = &i;
				//sb.txtptr = &byte;
				sb.txtptr = (const void *)0xDEADBEEF;;
				sb.num = num;
				sb.initialised = initialised;

				/* initialise i, j and the S-box if not done yet */
				if (!initialised) {
					init();
					initialised = 1;
				}

				// FIXME initialize more sb member

				/* dance around by some bytes for added protection */
				n = byte();
				/* and carry some over to below */
				m = byte();
				while (n--)
					(void)byte();
				m += byte();

				/* while time is not a secret, it helps through variety */
				//gettimeofday(&sb.tv, NULL);

				/* actually add the hopefully random-containing seed */
				ksa((const uint8_t *)&sb, sizeof(sb));

				/* throw away the first part of the arcfour keystream */
				/* with some bytes varied for added protection */
				m += 256 * 4 + (byte() & 0x1F);
				while (m--)
					(void)byte();
				/* state is now good for so many bytes: (not so much in NOUX) */
				num = 2000;
			}

			void buf(void *buf_, size_t len)
			{
				size_t chunk;
				uint8_t *buf = (uint8_t *)buf_;
				uint8_t m, n;

				/* operate in chunks of at most 256 bytes */
				while ((chunk = len > 256 ? 256 : len) > 0) {
					/* adjust len */
					len -= chunk;

					/* is the state good for this? (or even initialised, yet?) */
					if (num < chunk)
						stir_unlocked();

					/* dance around a few bytes for added protection */
					m = byte() & 3;
					/* and carry some down below */
					n = byte() & 3;
					while (m--)
						(void)byte();

					/* actually read out the keystream into buf */
					while (chunk--) {
						*buf++ = byte();
					}

					/* dance around the bytes read from above, for protection */
					while (n--)
						(void)byte();
				}
			}

		public:

			Arc4random(void* bytes, size_t nbytes)
			:
				i(0),
				j(0),
				num(0),
				initialised(0)
			{
				memset(S, 0, 256);
			}

			void get(void *_buf, size_t len)
			{
				buf(_buf, len);
			}
	};


	class Random_file_system : public File_system
	{
		private:

			Arc4random *_arc4random;

			const char *_filename() { return "urandom"; }

			bool _is_root(const char *path)
			{
				return (strcmp(path, "") == 0) || (strcmp(path, "/") == 0);
			}

			bool _is_device_random_file(const char *path)
			{
				return (strlen(path) == (strlen(_filename()) + 1)) &&
					   (strcmp(&path[1], _filename()) == 0);
			}

		public:

			Random_file_system(Xml_node node)
			{
				void   *bytes = 0;
				size_t nbytes = 0;
				_arc4random = new (env()->heap()) Arc4random(bytes, nbytes);
			}

			~Random_file_system()
			{
				destroy(env()->heap(), _arc4random);
			}


			/*********************************
			 ** Directory-service interface **
			 *********************************/

			Dataspace_capability dataspace(char const *path)
			{
				/* not supported */
				return Dataspace_capability();
			}

			void release(char const *path, Dataspace_capability ds_cap)
			{
				/* not supported */
			}

			bool stat(Sysio *sysio, char const *path)
			{
				Sysio::Stat st = { 0, 0, 0, 0, 0, 0 };

				if (_is_root(path))
					st.mode = Sysio::STAT_MODE_DIRECTORY;
				else if (_is_device_random_file(path)) {
					st.mode = Sysio::STAT_MODE_CHARDEV;
				} else {
					sysio->error.stat = Sysio::STAT_ERR_NO_ENTRY;
					return false;
				}

				sysio->stat_out.st = st;
				return true;
			}

			bool dirent(Sysio *sysio, char const *path, off_t index)
			{
				if (_is_root(path)) {
					if (index == 0) {
						sysio->dirent_out.entry.type = Sysio::DIRENT_TYPE_CHARDEV;
						strncpy(sysio->dirent_out.entry.name,
						        _filename(),
						        sizeof(sysio->dirent_out.entry.name));
					} else {
						sysio->dirent_out.entry.type = Sysio::DIRENT_TYPE_END;
					}

					return true;
				}

				return false;
			}

			size_t num_dirent(char const *path)
			{
				if (_is_root(path))
					return 1;
				else
					return 0;
			}

			bool is_directory(char const *path)
			{
				if (_is_root(path))
					return true;

				return false;
			}

			char const *leaf_path(char const *path)
			{
				return path;
			}

			Vfs_handle *open(Sysio *sysio, char const *path)
			{
				if (!_is_device_random_file(path)) {
					sysio->error.open = Sysio::OPEN_ERR_UNACCESSIBLE;
					return 0;
				}

				return new (env()->heap()) Vfs_handle(this, this, 0);
			}

			bool unlink(Sysio *sysio, char const *path)
			{
				/* not supported */
				return false;
			}

			bool readlink(Sysio *sysio, char const *path)
			{
				/* not supported */
				return false;
			}

			bool rename(Sysio *sysio, char const *from_path, char const *to_path)
			{
				/* not supported */
				return false;
			}

			bool mkdir(Sysio *sysio, char const *path)
			{
				/* not supported */
				return false;
			}

			bool symlink(Sysio *sysio, char const *path)
			{
				/* not supported */
				return false;
			}

			/***************************
			 ** File_system interface **
			 ***************************/

			static char const *name() { return "random"; }


			/********************************
			 ** File I/O service interface **
			 ********************************/

			bool write(Sysio *sysio, Vfs_handle *handle)
			{
				sysio->write_out.count = sysio->write_in.count;

				return true;
			}

			bool read(Sysio *sysio, Vfs_handle *vfs_handle)
			{
				size_t nbytes = sysio->read_in.count;

				_arc4random->get(sysio->read_out.chunk, nbytes);
				sysio->read_out.count = nbytes;

				return true;
			}

			bool ftruncate(Sysio *sysio, Vfs_handle *vfs_handle) { return true; }
	};
}

#endif /* _NOUX__RANDOM_H_ */
