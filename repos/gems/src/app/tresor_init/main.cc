/*
 * \brief  Integration of the Tresor block encryption
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
#include <tresor/ft_initializer.h>
#include <tresor/sb_initializer.h>
#include <tresor/vbd_initializer.h>

/* tresor init includes */
#include <tresor_init/configuration.h>

using namespace Genode;
using namespace Tresor;

namespace Tresor_init { class Main; }

class Tresor_init::Main : private Vfs::Env::User, private Tresor::Module_composition, public  Tresor::Module, public Module_channel
{
	private:

		enum State { INIT, REQ_GENERATED, INIT_SBS_SUCCEEDED };

		Env  &_env;
		Heap  _heap { _env.ram(), _env.rm() };
		Attached_rom_dataspace _config_rom { _env, "config" };
		Vfs::Simple_env _vfs_env { _env, _heap, _config_rom.xml().sub_node("vfs"), *this };
		Signal_handler<Main> _sigh { _env.ep(), *this, &Main::_handle_signal };
		Constructible<Configuration> _cfg { };
		Trust_anchor _trust_anchor { _vfs_env, _config_rom.xml().sub_node("trust-anchor") };
		Crypto _crypto { _vfs_env, _config_rom.xml().sub_node("crypto") };
		Block_io _block_io { _vfs_env, _config_rom.xml().sub_node("block-io") };
		Pba_allocator _pba_alloc { NR_OF_SUPERBLOCK_SLOTS };
		Vbd_initializer _vbd_initializer { };
		Ft_initializer _ft_initializer { };
		Sb_initializer _sb_initializer { };
		bool _generated_req_success { };
		State _state { INIT };

		NONCOPYABLE(Main);

		void _generated_req_completed(State_uint state_uint) override
		{
			if (!_generated_req_success) {
				error("command pool: request failed because generated request failed)");
				_env.parent().exit(-1);
				return;
			}
			_state = (State)state_uint;
		}

		void wakeup_vfs_user() override { _sigh.local_submit(); }

		void _wakeup_back_end_services() { _vfs_env.io().commit(); }

		void _handle_signal()
		{
			execute_modules();
			_wakeup_back_end_services();
		}

	public:

		Main(Env &env) : Module_channel { COMMAND_POOL, 0 }, _env { env }
		{
			add_module(COMMAND_POOL, *this);
			add_module(CRYPTO, _crypto);
			add_module(TRUST_ANCHOR, _trust_anchor);
			add_module(BLOCK_IO, _block_io);
			add_module(VBD_INITIALIZER, _vbd_initializer);
			add_module(FT_INITIALIZER, _ft_initializer);
			add_module(SB_INITIALIZER, _sb_initializer);
			add_channel(*this);
			_cfg.construct(_config_rom.xml());
			_handle_signal();
		}

		void execute(bool &progress) override
		{
			switch(_state) {
			case INIT:

				generate_req<Sb_initializer_request>(
					INIT_SBS_SUCCEEDED, progress, (Tree_level_index)_cfg->vbd_nr_of_lvls() - 1,
					(Tree_degree)_cfg->vbd_nr_of_children(), _cfg->vbd_nr_of_leafs(),
					(Tree_level_index)_cfg->ft_nr_of_lvls() - 1,
					(Tree_degree)_cfg->ft_nr_of_children(), _cfg->ft_nr_of_leafs(),
					(Tree_level_index)_cfg->ft_nr_of_lvls() - 1,
					(Tree_degree)_cfg->ft_nr_of_children(), _cfg->ft_nr_of_leafs(), _pba_alloc,
					_generated_req_success);
				_state = REQ_GENERATED;
				break;

			case INIT_SBS_SUCCEEDED: _env.parent().exit(0); break;
			default: break;
			}
		}
};

void Component::construct(Genode::Env &env) { static Tresor_init::Main main { env }; }

namespace Libc {

	struct Env;
	struct Component { void construct(Libc::Env &) { } };
}
