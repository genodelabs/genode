/*
 * \brief  Test for dynamically starting chrooted subsystems via the loader
 * \author Norman Feske
 * \date   2012-06-06
 *
 * This test creates two subsystems, each residing in a dedicated chroot
 * environment, by combining the loader service with the chroot mechanism.
 * One subsystem runs infinitely. The other subsystem will be repeatedly
 * started and killed.
 */

/* Genode includes */
#include <base/snprintf.h>
#include <loader_session/connection.h>
#include <os/config.h>
#include <timer_session/connection.h>


/*******************************************************
 ** Helpers for obtaining test parameters from config **
 *******************************************************/

static char const *chroot_path_from_config(char const *node_name,
                                           char *dst, Genode::size_t dst_len)
{
	Genode::config()->xml_node().sub_node(node_name)
		.attribute("chroot_path").value(dst, dst_len);
	return dst;
}


static char const *chroot_path_of_static_test()
{
	static char buf[1024];
	return chroot_path_from_config("static_test", buf, sizeof(buf));
}


static char const *chroot_path_of_dynamic_test()
{
	static char buf[1024];
	return chroot_path_from_config("dynamic_test", buf, sizeof(buf));
}


/**********
 ** Test **
 **********/

/**
 * Return subsystem configuration.
 */
static char const *subsystem_config()
{
	return "<config verbose=\"yes\">\n"
	       "  <parent-provides>\n"
	       "    <service name=\"ROM\"/>\n"
	       "    <service name=\"LOG\"/>\n"
	       "    <service name=\"CAP\"/>\n"
	       "    <service name=\"RAM\"/>\n"
	       "    <service name=\"CPU\"/>\n"
	       "    <service name=\"RM\"/>\n"
	       "    <service name=\"PD\"/>\n"
	       "    <service name=\"SIGNAL\"/>\n"
	       "    <service name=\"Timer\"/>\n"
	       "  </parent-provides>\n"
	       "  <default-route>\n"
	       "    <any-service> <parent/> </any-service>\n"
	       "  </default-route>\n"
	       "  <start name=\"test-timer\">\n"
	       "    <resource name=\"RAM\" quantum=\"1G\"/>\n"
	       "  </start>\n"
	       "</config>\n";
}


/**
 * Chroot subsystem corresponding to a loader session
 */
class Chroot_subsystem
{
	private:

		Loader::Connection _loader;

		char _label[32];

		/**
		 * Import data as ROM module into the subsystem-specific ROM service
		 */
		void _import_rom_module(char const *name, void const *ptr, Genode::size_t size)
		{
			using namespace Genode;

			Dataspace_capability ds = _loader.alloc_rom_module(name, size);

			/* fill ROM module with data */
			char *local_addr = env()->rm_session()->attach(ds);
			memcpy(local_addr, ptr, size);
			env()->rm_session()->detach(local_addr);

			_loader.commit_rom_module(name);
		}

	public:

		Chroot_subsystem(char const *chroot_path, Genode::size_t ram_quota)
		:
			_loader(ram_quota)
		{
			using namespace Genode;

			/*
			 * Import subsystem's configuration into the subsystem's loader
			 * session as a ROM module named "config".
			 */
			_import_rom_module("config", subsystem_config(),
			                   strlen(subsystem_config()) + 1);

			/*
			 * Name of the Genode binary is start as the root of the new
			 * subsystem.
			 */
			char const *binary_name = "init";

			/*
			 * Generate unique label name using a counter
			 *
			 * The label appears in the LOG output of the loaded subsystem.
			 * Technically, it does need to be unique. It is solely used
			 * for validating the test in the run script.
			 */
			static int cnt = 0;
			snprintf(_label, sizeof(_label), "%s-%d", binary_name, ++cnt);

			/* start execution of new subsystem */
			_loader.start(binary_name,
			              Loader::Session::Name(_label),
			              Native_pd_args(chroot_path, 0, 0));
		}
};


int main(int, char **)
{
	Genode::printf("--- chroot-loader test started ---\n");

	static Chroot_subsystem static_subsystem(chroot_path_of_static_test(),
	                                         2*1024*1024);

	static Timer::Connection timer;

	for (unsigned i = 0; i < 5; i++) {

		PLOG("dynamic test iteration %d", i);

		Chroot_subsystem subsystem(chroot_path_of_dynamic_test(),
		                           2*1024*1024);

		/* grant the subsystem one second of life */
		timer.msleep(1000);

		/*
		 * The local 'dynamic_subsystem' instance will be destructed at the of
		 * the loop body.
		 */
	}

	Genode::printf("--- chroot-loader test finished ---\n");
	return 0;
}
