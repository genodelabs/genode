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

class Tresor_check::Main : private Vfs::Env::User, private Tresor::Module_composition, public  Tresor::Module, public Module_channel
{
	private:

		enum State { INIT, REQ_GENERATED, CHECK_SBS_SUCCEEDED };

		Env  &_env;
		Heap  _heap { _env.ram(), _env.rm() };
		Attached_rom_dataspace _config_rom { _env, "config" };
		Vfs::Simple_env _vfs_env { _env, _heap, _config_rom.xml().sub_node("vfs"), *this };
		Signal_handler<Main> _sigh { _env.ep(), *this, &Main::_handle_signal };
		Trust_anchor _trust_anchor { _vfs_env, _config_rom.xml().sub_node("trust-anchor") };
		Crypto _crypto { _vfs_env, _config_rom.xml().sub_node("crypto") };
		Block_io _block_io { _vfs_env, _config_rom.xml().sub_node("block-io") };
		Vbd_check _vbd_check { };
		Ft_check _ft_check { };
		Sb_check _sb_check { };
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
			add_module(VBD_CHECK, _vbd_check);
			add_module(FT_CHECK, _ft_check);
			add_module(SB_CHECK, _sb_check);
			add_channel(*this);
			_handle_signal();
		}

		void execute(bool &progress) override
		{
			switch(_state) {
			case INIT:

				generate_req<Sb_check_request>(CHECK_SBS_SUCCEEDED, progress, _generated_req_success);
				_state = REQ_GENERATED;
				break;

			case CHECK_SBS_SUCCEEDED: _env.parent().exit(0); break;
			default: break;
			}
		}
};

void Component::construct(Genode::Env &env) { static Tresor_check::Main main { env }; }

namespace Libc {

	struct Env;
	struct Component { void construct(Libc::Env &) { } };
}
