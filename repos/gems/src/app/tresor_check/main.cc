/*
 * \brief  Verify the dimensions and hashes of a tresor container
 * \author Martin Stein
 * \author Josef Soentgen
 * \date   2020-11-10
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <vfs/simple_env.h>

/* tresor includes */
#include <tresor/block_io.h>
#include <tresor/crypto.h>
#include <tresor/trust_anchor.h>
#include <tresor/ft_check.h>
#include <tresor/sb_check.h>
#include <tresor/vbd_check.h>

using namespace Genode;
using namespace Tresor;

namespace Tresor_check { class Main; }

class Tresor_check::Main : private Vfs::Env::User
{
	private:

		enum State { INIT, CHECK_SB, CHECK_SB_SUCCEEDED };

		Env  &_env;
		Heap  _heap { _env.ram(), _env.rm() };
		Attached_rom_dataspace _config_rom { _env, "config" };
		Vfs::Simple_env _vfs_env = _config_rom.xml().with_sub_node("vfs",
			[&] (Xml_node const &config) -> Vfs::Simple_env {
				return { _env, _heap, config, *this }; },
			[&] () -> Vfs::Simple_env {
				error("VFS not configured");
				return { _env, _heap, Xml_node("<empty/>") }; });
		Signal_handler<Main> _sigh { _env.ep(), *this, &Main::_handle_signal };

		Tresor::Path _path_from_config(auto const &node_name) const
		{
			return _config_rom.xml().with_sub_node(node_name,
				[&] (Xml_node const &node) { return node.attribute_value("path", Tresor::Path()); },
				[&]                        { return Tresor::Path(); });
		}

		Tresor::Path const _block_io_path     = _path_from_config("block-io");
		Tresor::Path const _trust_anchor_path = _path_from_config("trust-anchor");
		Vfs::Vfs_handle &_block_io_file { open_file(_vfs_env, _block_io_path, Vfs::Directory_service::OPEN_MODE_RDWR) };
		Vfs::Vfs_handle &_ta_decrypt_file { open_file(_vfs_env, { _trust_anchor_path, "/decrypt" }, Vfs::Directory_service::OPEN_MODE_RDWR) };
		Vfs::Vfs_handle &_ta_encrypt_file { open_file(_vfs_env, { _trust_anchor_path, "/encrypt" }, Vfs::Directory_service::OPEN_MODE_RDWR) };
		Vfs::Vfs_handle &_ta_generate_key_file { open_file(_vfs_env, { _trust_anchor_path, "/generate_key" }, Vfs::Directory_service::OPEN_MODE_RDWR) };
		Vfs::Vfs_handle &_ta_initialize_file { open_file(_vfs_env, { _trust_anchor_path, "/initialize" }, Vfs::Directory_service::OPEN_MODE_RDWR) };
		Vfs::Vfs_handle &_ta_hash_file { open_file(_vfs_env, { _trust_anchor_path, "/hash" }, Vfs::Directory_service::OPEN_MODE_RDWR) };
		Block_io _block_io { _block_io_file };
		Vbd_check _vbd_check { };
		Ft_check _ft_check { };
		Sb_check _sb_check { };
		Trust_anchor _trust_anchor { { _ta_decrypt_file, _ta_encrypt_file, _ta_generate_key_file, _ta_initialize_file, _ta_hash_file } };
		Sb_check::Check _check_superblocks { };

		void _wakeup_back_end_services() { _vfs_env.io().commit(); }

		void _handle_signal()
		{
			while(_sb_check.execute(_check_superblocks, _vbd_check, _ft_check, _block_io, _trust_anchor));
			if (_check_superblocks.complete())
				_env.parent().exit(_check_superblocks.success() ? 0 : -1);
			_wakeup_back_end_services();
		}

		/********************
		 ** Vfs::Env::User **
		 ********************/

		void wakeup_vfs_user() override { _sigh.local_submit(); }

	public:

		Main(Env &env) : _env(env) { _handle_signal(); }
};

void Component::construct(Genode::Env &env) { static Tresor_check::Main main { env }; }

namespace Libc {

	struct Env;
	struct Component { void construct(Libc::Env &) { } };
}
