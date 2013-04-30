/*
 * \brief  Frame-buffer driver for the i.MX53
 * \author Nikolay Golikov <nik@ksyslabs.org>
 * \date   2012-06-21
 */

/* Genode includes */
#include <imx_framebuffer_session/imx_framebuffer_session.h>
#include <cap_session/connection.h>
#include <dataspace/client.h>
#include <base/printf.h>
#include <base/sleep.h>
#include <os/static_root.h>

/* local includes */
#include <driver.h>

namespace Framebuffer {
	using namespace Genode;
	class Session_component;
};


class Framebuffer::Session_component :
	public Genode::Rpc_object<Framebuffer::Imx_session>
{
	private:

		size_t               _size;
		Dataspace_capability _ds;
		addr_t               _phys_base;
		Mode                 _mode;

		Ipu &_ipu;

	public:

		Session_component(Driver &driver)
		: _size(driver.size()),
		  _ds(env()->ram_session()->alloc(_size, false)),
		  _phys_base(Dataspace_client(_ds).phys_addr()),
		  _mode(driver.mode()),
		  _ipu(driver.ipu())
		{
			if (!driver.init(_phys_base)) {
				PERR("Could not initialize display");
				struct Could_not_initialize_display : Exception { };
				throw Could_not_initialize_display();
			}
		}


		/**************************************
		 **  Framebuffer::session interface  **
		 **************************************/

		Dataspace_capability dataspace()                  { return _ds;   }
		void release()                                    { }
		Mode mode() const                                 { return _mode; }
		void mode_sigh(Genode::Signal_context_capability) { }
		void refresh(int, int, int, int)                  { }

		void overlay(Genode::addr_t phys_base, int x, int y, int alpha) {
			_ipu.overlay(phys_base, x, y, alpha); }
};

int main(int, char **)
{
	Genode::printf("Starting i.MX53 framebuffer driver\n");

	using namespace Framebuffer;

	static Driver driver;

	enum { STACK_SIZE = 4096 };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "fb_ep");

	static Session_component                 fb_session(driver);
	static Static_root<Framebuffer::Session> fb_root(ep.manage(&fb_session));

	env()->parent()->announce(ep.manage(&fb_root));

	sleep_forever();
	return 0;
}
