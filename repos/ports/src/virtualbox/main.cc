/*
 * \brief  Port of VirtualBox to Genode
 * \author Norman Feske
 * \date   2013-08-20
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <base/printf.h>

#include <os/config.h>
#include <base/snprintf.h>

/* libc includes */
#include <stdio.h>
#include <string.h>

/* libc memory allocator */
#include <libc_mem_alloc.h>

/* Virtualbox includes of VBoxBFE */
#include <iprt/initterm.h>
#include <iprt/err.h>

void *operator new (Genode::size_t size)
{
	static Libc::Mem_alloc_impl heap(Genode::env()->rm_session());
	return heap.alloc(size, 0x10);
}

void *operator new [] (Genode::size_t size) {
	return Genode::env()->heap()->alloc(size); }

void operator delete(void * p) {
	if (Genode::env()->heap()->need_size_for_free()) {
		PERR("leaking memory - delete operator is missing size information");
		return;
	}
	Genode::env()->heap()->free(p, 0);
}


namespace {
	template <int MAX_ARGS>
	class Args
	{
		private:

			int   _argc           = 0;
			char *_argv[MAX_ARGS] = { };

		public:

			class Too_many_arguments { };

			void add(char const *arg)
			{
				/* argv[MAX_ARGS - 1] must be unused and set to 0 */
				if (_argc >= MAX_ARGS - 1)
					throw Too_many_arguments();

				_argv[_argc++] = strdup(arg);
			}

			char *** argvp() {
				static char ** argv = _argv; 
				return &argv;
			}

			int argc() { return _argc; }
	};
} /* unnamed namespace  */


extern "C" {

/* make output of Virtualbox visible */
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	if (!stream || !ptr ||
	    !(fileno(stdout) == fileno(stream) || fileno(stderr) == fileno(stream)))
		return EOF;

	char const * cptr = reinterpret_cast<const char *>(ptr);
	for (size_t j = 0; j < nmemb; j++)
		for (size_t i = 0; i < size; i++)
			Genode::printf("%c", cptr[j * size + i]);

	return nmemb;
}

int fprintf(FILE *stream, const char *format, ...) /* called by RTAssertMsg1 */
{
	if (!stream ||
	    !(fileno(stdout) == fileno(stream) || fileno(stderr) == fileno(stream)))
		return EOF;

	va_list list;
	va_start(list, format);

	Genode::vprintf(format, list);

	va_end(list);

	return 0;
}

int fputs(const char *s, FILE *stream) /* called by RTAssertMsg2Weak */
{
	if (!stream ||
	    !(fileno(stdout) == fileno(stream) || fileno(stderr) == fileno(stream)))
		return EOF;

	fwrite(s, Genode::strlen(s), 1, stream);
}

/* our libc provides a _nanosleep function */
int _nanosleep(const struct timespec *req, struct timespec *rem);
int nanosleep(const struct timespec *req, struct timespec *rem) {
	return _nanosleep(req, rem); }


/*
 * Genode way of using a configuration. Wrap VBox main until we throught it
 * out eventually.
 */

/* main function of VBox is in Frontends/VBoxBFE/VBoxBFE.cpp */
extern "C" DECLEXPORT(int) TrustedMain (int argc, char **argv, char **envp);

int main()
{
	static char c_mem[16];
	static char c_vram[16];
	static char c_type[4];
	static char c_file[128];
	static bool bOverlay = false;

	static Args<64> args;

	/* request max available memory */
	size_t vm_size = Genode::env()->ram_session()->avail();

	enum {
	       VMM_MEMORY  = 88 * 1024 * 1024, /* let a bit memory for the VMM */
	       VRAM_MEMORY =  8 * 1024 * 1024, /* video memory */
	};

	if (vm_size < VMM_MEMORY + VRAM_MEMORY) {
		PERR("not enough memory available - need %u, available only %zu "
		     "- exit", VMM_MEMORY + VRAM_MEMORY, vm_size);
		return 1;
	}
	vm_size -= VMM_MEMORY + VRAM_MEMORY;

	try {
		using namespace Genode;

		Xml_node node = config()->xml_node().sub_node("image");
		Xml_node::Attribute type = node.attribute("type");
		Xml_node::Attribute file = node.attribute("file");
		try {
			Xml_node::Attribute overlay = node.attribute("overlay");
			overlay.value(c_type, sizeof(c_type));
			if (!Genode::strcmp(c_type, "yes"))
				bOverlay = true;
		} catch (...) { }
		type.value(c_type, sizeof(c_type));
		file.value(c_file, sizeof(c_file));
	} catch (...) {
		PERR("C++ exception during xml parsing");
		return 2;
	}

	args.add("virtualbox");

	Genode::snprintf(c_mem, sizeof(c_mem), "%u", vm_size / 1024 / 1024);
	args.add("-m"); args.add(c_mem);

	Genode::snprintf(c_vram, sizeof(c_vram), "%u", VRAM_MEMORY / 1024 / 1024);
	args.add("-vram"); args.add(c_vram);

	args.add("-boot");
	if (!Genode::strcmp(c_type, "iso")) {
		args.add("d");
		args.add("-cdrom");
	} else 
	if (!Genode::strcmp(c_type, "vdi")) {
		args.add("c");
		args.add("-hda");
	} else {
		PERR("invalid configuration - abort");
		return 3;
	}

	args.add(c_file);

	if (bOverlay)
		args.add("-overlay");

	/* disable acpi support if requested */
	try {
		Genode::Xml_node node = Genode::config()->xml_node().sub_node("noacpi");
		args.add("-noacpi");
	} catch (...) { }

	/* ioapic support */
	try {
		Genode::Xml_node node = Genode::config()->xml_node().sub_node("ioapic");
		args.add("-ioapic");
	} catch (...) { }

	/* shared folder setup */
	unsigned shares = 0;
	try {
		using namespace Genode;
		for (Xml_node node = config()->xml_node().sub_node("share");
		     true; node = node.next("share")) {

			Xml_node::Attribute share_dir_host  = node.attribute("host");
			Xml_node::Attribute share_dir_guest = node.attribute("guest");

			char * dir_host  = new char[share_dir_host.value_size() + 1];
			char * dir_guest = new char[share_dir_guest.value_size() + 1];

			share_dir_host.value(dir_host, share_dir_host.value_size() + 1);
			share_dir_guest.value(dir_guest, share_dir_guest.value_size() + 1);

			args.add("-share"); args.add(dir_host), args.add(dir_guest);
			shares ++;
		}
	} catch(Genode::Xml_node::Nonexistent_sub_node) { }

	/* network setup */
	unsigned net = 0;
	try {
		using namespace Genode;
		for (Xml_node node = config()->xml_node().sub_node("net");
		     true; node = node.next("net")) {

			net ++;

			char buf [10];
			Genode::snprintf(buf, sizeof(buf), "-hifdev%d", net);
			args.add(buf);

			/* read out network model, if not set use e1000 */
			try {
				Xml_node::Attribute model  = node.attribute("model");
				char * c_model = new char[model.value_size() + 1];
				model.value(c_model, model.value_size() + 1);
				args.add(c_model);
			} catch(Genode::Xml_node::Nonexistent_attribute) {
				args.add("e1000");
			}
		}
	} catch(Genode::Xml_node::Nonexistent_sub_node) { }

	PINF("start %s image '%s' with %zu MB guest memory=%zu, %u shared folders,"
	     " %u network connections",
	     c_type, c_file, vm_size / 1024 / 1024,
	     Genode::env()->ram_session()->avail(), shares, net);

	if (RT_FAILURE(RTR3InitExe(args.argc(), args.argvp(), 0))) {
		PERR("Intialization of VBox Runtime failed.");
		return 5;
	}

	return TrustedMain(args.argc(), *args.argvp(), NULL);
}

} /* extern "C" */
