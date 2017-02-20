/**
 * \brief  RUMP cgd
 * \author Josef Soentgen
 * \date   2014-04-11
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <os/config.h>

/* repo includes */
#include <rump_cgd/cgd.h>

/* local includes */
#include "cgd.h"

/* rump includes */
extern "C" {
#include <sys/cdefs.h>
#include <sys/errno.h>
#include <fcntl.h>
#include <sys/dirent.h>
#include <sys/disklabel.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/reboot.h>
#include <dev/cgdvar.h>
#include <rump/rump.h>
#include <rump/rump_syscalls.h>
}


/**
 * Miscellaneous methods used for converting the key string
 */
namespace {

	/**
	 * Decode base64 encoding
	 */
	class Base64
	{
		private:

			/* initialize the lookup table */
			static constexpr const unsigned char _ascii_to_value[256]
			{
				64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
				64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
				64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
				52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
				64,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
				15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
				64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
				41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64,
				64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
				64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
				64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
				64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
				64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
				64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
				64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
				64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
			};


		public:

			static size_t decode(char *dst, char const *src, size_t src_len)
			{
				unsigned char const *usrc = reinterpret_cast<unsigned char const*>(src);

				size_t i;
				for (i = 0; i < src_len; i += 4) {
					*(dst++) = _ascii_to_value[usrc[i]]     << 2 | _ascii_to_value[usrc[i + 1]] >> 4;
					*(dst++) = _ascii_to_value[usrc[i + 1]] << 4 | _ascii_to_value[usrc[i + 2]] >> 2;
					*(dst++) = _ascii_to_value[usrc[i + 2]] << 6 | _ascii_to_value[usrc[i + 3]];
				}

				return src_len * 3/4;
			}
	};


	/* lookup table instance */
	constexpr unsigned char Base64::_ascii_to_value[256];


	/**
	 * Convert unsigned big endian value to host endianess
	 */
	unsigned read_big_endian(unsigned val)
	{
		unsigned char *p = reinterpret_cast<unsigned char*>(&val);

		return (p[3]<<0) | (p[2]<<8) | (p[1]<<16) | (p[0]<<24);
	}
}


/**
 * cgd(4) wrapper
 */
namespace Cgd {

	struct Params
	{
		enum {
			ALGORITHM_LEN  = 16,
			IVMETHOD_LEN   = 16,
			KEY_LEN        = 64,
			PASSPHRASE_LEN = 64,
			SALT_LEN       = 29,
		};

		char           algorithm[ALGORITHM_LEN];
		char           ivmethod[IVMETHOD_LEN];
		char           key[KEY_LEN];

		Genode::size_t keylen    = 0;
		Genode::size_t blocksize = 0;

		Params()
		{
			Genode::memset(algorithm, 0, sizeof(*algorithm));
			Genode::memset(ivmethod,  0, sizeof(*ivmethod));
			Genode::memset(key,       0, sizeof(*key));
		}

		static Params *generate()
		{
			return 0;
		}
	};


	/**
	 * Configuration of the cgd device
	 *
	 * cgdconfig(8) saves the key as base 64 encoded string. The first 4 bytes
	 * of this string are the length of the key in bits stored as big endian.
	 */
	class Config
	{
		public:

			/* same as in cgdconfig(8) */
			typedef enum { ACTION_INVALID, ACTION_CONFIGURE, ACTION_GENERATE } Action;


		private:

			enum { ACTION_VALUE_LEN      = 16,  /* action attribute value length */
			       ENCODED_KEY_LEN       = 64,  /* encoded key string buffer */
			       DECODED_KEY_LEN       = 36,  /* decoded key string buffer */
			       VALID_ENCODED_KEY_LEN = 48,  /* length of 256Bit key */
			};

			Genode::Config &_cfg;
			Action          _action;
			Params         *_params;

			/**
			 * Get action from config attribute
			 *
			 * \return action
			 */
			Action _get_action()
			{
				char action_val[ACTION_VALUE_LEN];

				if (_cfg.xml_node().has_attribute("action"))
					_cfg.xml_node().attribute("action").value(action_val, sizeof (action_val));
				else
					return ACTION_INVALID;

				if (Genode::strcmp(action_val, "configure") == 0)
					return ACTION_CONFIGURE;
				if (Genode::strcmp(action_val, "generate") == 0)
					return ACTION_GENERATE;

				return ACTION_INVALID;
			}

			/**
			 * Decode key string in cgdconfig(8) format
			 *
			 * \param dst pointer to buffer to store decoded key
			 * \param src pointer to buffer with encoded key string
			 * \param src_len length of encoded key string buffer
			 *
			 * \return true if decoding was successful and false otherwise
			 */
			bool _decode_key_string(char *dst, char const *src, size_t src_len)
			{
				char dec_key[DECODED_KEY_LEN];

				/* check that buffer is large enough */
				if (sizeof (dec_key) < (src_len*3/4))
					return false;

				(void) Base64::decode(dec_key, src, src_len);

				unsigned *p    = reinterpret_cast<unsigned *>(dec_key);
				unsigned  bits = read_big_endian(*p);
				Genode::memcpy(dst, p + 1, bits / 8);

				return true;
			}

			/**
			 * Parse given configuration
			 *
			 * This method throws a Genode::Exception if an error in the given
			 * configuration is found.
			 */
			void _parse_config()
			{
				if (_cfg.xml_node().has_sub_node("params")) {
					Genode::Xml_node pnode = _cfg.xml_node().sub_node("params");

					char method_val[4];
					pnode.sub_node("method").value(method_val, sizeof (method_val));
					bool use_key = Genode::strcmp(method_val, "key") == 0 ? true : false;

					if (!use_key) {
						Genode::error("no valid method specified.");
						throw Genode::Exception();
					}

					_params = new (Genode::env()->heap()) Params();

					Genode::strncpy(_params->algorithm, CGD_ALGORITHM,
					                Params::ALGORITHM_LEN);
					Genode::strncpy(_params->ivmethod, CGD_IVMETHOD,
					                Params::IVMETHOD_LEN);

					char enc_key[ENCODED_KEY_LEN];
					pnode.sub_node("key").value(enc_key, sizeof (enc_key));
					size_t enc_key_len = Genode::strlen(enc_key);

					if (enc_key_len != VALID_ENCODED_KEY_LEN) {
						Genode::error("incorrect encoded key found.");
						throw Genode::Exception();
					}

					if (!_decode_key_string(_params->key, enc_key, enc_key_len)) {
						Genode::error("could not decode key string.");
						throw Genode::Exception();
					}

					_params->keylen = CGD_KEYLEN;

					/* let cgd(4) figure out the right blocksize */
					_params->blocksize = -1;
				} else {
					Genode::error("no <params> node found.");
					throw Genode::Exception();
				}
			}


		public:

			Config()
			:
				_cfg(*Genode::config()),
				_action(_get_action()),
				_params(0)
			{
				_parse_config();
			}

			~Config() { destroy(Genode::env()->heap(), _params); }

			Action  action() { return _action; }
			Params* params() { return _params; }
	};
}


/**
 * Constructor
 *
 * \param fd used by the rumpkernel to execute syscalls on
 */
Cgd::Device::Device(int fd)
:
	_fd(fd),
	_blk_sz(0), _blk_cnt(0)
{
	/**
	 * We query the disklabel(5) on CGD_RAW_DEVICE to get the
	 * actual block size and the number of blocks in the
	 * first partition. Though there may be more partitions
	 * we always use the first one.
	 */
	disklabel dl;
	Genode::memset(&dl, 0, sizeof (dl));
	int err = rump_sys_ioctl(_fd, DIOCGDINFO, &dl);
	if (err) {
		cgd_ioctl ci;
		rump_sys_ioctl(_fd, CGDIOCCLR, &ci);
		rump_sys_close(_fd);

		Genode::error("could not read geometry of '", CGD_RAW_DEVICE, "'");
		throw Genode::Exception();
	}

	_blk_sz  = dl.d_secsize;
	_blk_cnt = dl.d_partitions[0].p_size;
}


/**
 * Destructor
 */
Cgd::Device::~Device()
{
	/* unconfigure cgd(4) device to explicitly clean up buffers */
	cgd_ioctl ci;
	rump_sys_ioctl(_fd, CGDIOCCLR, &ci);
	rump_sys_close(_fd);
}


/**
 * Get name of the actual cgd device
 *
 * \return name
 */
char const *Cgd::Device::name() const { return CGD_RAW_DEVICE; }


/**
 * Read from the cgd device
 *
 * \param dst pointer to buffer
 * \param len size of the buffer
 * \param seek_offset offset from where to start reading
 *
 * \return bytes read
 */
Genode::size_t Cgd::Device::read(char *dst, Genode::size_t len, seek_off_t seek_offset)
{
	ssize_t ret = rump_sys_pread(_fd, dst, len, seek_offset);

	return ret == -1 ? 0 : ret;
}


/**
 * Write to cgd device
 *
 * \param src pointer to buffer
 * \param len size of the buffer
 * \param seek_offset offset from where to start writing
 *
 * \return bytes written
 */
Genode::size_t Cgd::Device::write(char const *src, Genode::size_t len, seek_off_t seek_offset)
{
	/* should we append? */
	if (seek_offset == ~0ULL) {
		off_t off = rump_sys_lseek(_fd, 0, SEEK_END);
		if (off == -1)
			return 0;
		seek_offset = off;
	}

	ssize_t ret = rump_sys_pwrite(_fd, src, len, seek_offset);
	return ret == -1 ? 0 : ret;
}


/**
 * Configure the actual cgd device
 *
 * \param alloc memory allocator for metadata
 * \param p pointer to parameters
 * \param dev name of the block device used as backing device for cgd
 *
 * \return configured cgd device
 */
Cgd::Device *Cgd::Device::configure(Genode::Allocator *alloc, Cgd::Params const *p,
                                    char const *dev)
{
	int fd = rump_sys_open(CGD_RAW_DEVICE, O_RDWR);
	if (fd == -1) {
		Genode::error("could not open '", CGD_RAW_DEVICE, "'");
		throw Genode::Exception();
	}

	/* perform configuration of cgd device */
	cgd_ioctl ci;
	Genode::memset(&ci, 0, sizeof (ci));

	ci.ci_disk      = dev;
	ci.ci_alg       = p->algorithm;
	ci.ci_ivmethod  = p->ivmethod;
	ci.ci_key       = p->key;
	ci.ci_keylen    = p->keylen;
	ci.ci_blocksize = p->blocksize;

	int err = rump_sys_ioctl(fd, CGDIOCSET, &ci);
	if (err == -1) {
		rump_sys_close(fd);

		Genode::error("could not configure '", CGD_RAW_DEVICE, "'");
		throw Genode::Exception();
	}

	cgd_user cu;
	Genode::memset(&cu, 0, sizeof (cu));

	err = rump_sys_ioctl(fd, CGDIOCGET, &cu);
	if (err == -1) { /* unlikely */

		/**
		 * Reuse former cgd_ioctl struct because it is not used by this
		 * ioctl() anyway.
		 */
		rump_sys_ioctl(fd, CGDIOCCLR, &ci);
		rump_sys_close(fd);

		Genode::error("could not get cgd information.");
		throw Genode::Exception();
	}

	Cgd::Device *cgd = new (alloc) Cgd::Device(fd);

	return cgd;
}


/**
 * Initialize a new Cgd::Device
 */
Cgd::Device *Cgd::init(Genode::Allocator *alloc, Server::Entrypoint &ep)
{
	/* start rumpkernel */
	rump_init();

	/* register block device */
	if (rump_pub_etfs_register(GENODE_DEVICE, GENODE_BLOCK_SESSION,
	                           RUMP_ETFS_BLK)) {
		Genode::error("could not register '", GENODE_DEVICE, "' within rumpkernel");
		throw Genode::Exception();
	}

	Cgd::Config cfg;

	Cgd::Config::Action action = cfg.action();

	Cgd::Device *cgd_dev = 0;

	switch (action) {
	case Cgd::Config::ACTION_CONFIGURE:
		{
			Cgd::Params *p = cfg.params();
			if (!p)
				throw Genode::Exception();

			cgd_dev = Cgd::Device::configure(alloc, p, GENODE_DEVICE);

			break;
		}
	case Cgd::Config::ACTION_GENERATE:
		{
			Cgd::Params *params = Cgd::Params::generate();
			(void)params;

			break;
		}
	case Cgd::Config::ACTION_INVALID:
		Genode::error("invalid action declared");
		throw Genode::Exception();
		break;
	}

	Genode::log("exporting '", cgd_dev->name(), "' as block session");

	return cgd_dev;
}


/**
 * Deinitialize a Cgd::Device
 */
void Cgd::deinit(Genode::Allocator *alloc, Cgd::Device *dev)
{
	destroy(alloc, dev);

	/* halt rumpkernel */
	rump_sys_reboot(RB_HALT, 0);
}
