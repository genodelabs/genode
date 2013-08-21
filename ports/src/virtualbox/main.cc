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

/* Virtualbox includes of VBoxBFE */
#include <iprt/initterm.h>
#include <iprt/err.h>

void *operator new (Genode::size_t size) {
	return Genode::env()->heap()->alloc(size); }

void operator delete(void * p) {
	if (Genode::env()->heap()->need_size_for_free()) {
		PERR("leaking memory - delete operator is missing size information");
		return;
	}
	Genode::env()->heap()->free(p, 0);
}

extern "C" {

/* string conversion function currently does not convert ... */
int RTStrCurrentCPToUtf8Tag(char **ppszString, char *pszString,
                            const char *pszTag)
{
	/* dangerous */
	*ppszString = pszString;
	return 0;
}

/* don't use 'Runtime/r3/posix/utf8-posix.cpp' because it depends on libiconv */
int RTStrUtf8ToCurrentCPTag(char **ppszString, char *pszString,
                            const char *pszTag)
{
	/* dangerous */
	*ppszString = pszString;
	return 0;
}


char * RTPathRealDup(const char *pszPath)
{
	/* dangerous */
	return (char *)pszPath;
}


bool RTPathExists(const char *pszPath)
{
	return true;
}


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

int main(int, char **)
{
	static char c_mem[16];
	static char c_type[4];
	static char c_file[128];
	static bool bOverlay = false;

	int argc = 9;
	char * argv[argc + 1];

	/* request max available memory */
	size_t vm_size = Genode::env()->ram_session()->avail();

	enum { VMM_MEMORY = 64 * 1024 * 1024 /* let a bit memory for the VMM */ };
	if (vm_size < VMM_MEMORY) {
		PERR("not enough memory available - need %u, available only %zu "
		     "- exit", VMM_MEMORY, vm_size);
		return 1;
	}
	vm_size -= VMM_MEMORY;

	try {
		using namespace Genode;

		Xml_node node = config()->xml_node().sub_node("image");
		Xml_node::Attribute type = node.attribute("type");
		Xml_node::Attribute file = node.attribute("file");
		try {
			Xml_node::Attribute overlay = node.attribute("overlay");
			overlay.value(c_type, sizeof(c_type));
			if (!strcmp(c_type, "yes"))
				bOverlay = true;
		} catch (...) { }
		type.value(c_type, sizeof(c_type));
		file.value(c_file, sizeof(c_file));
	} catch (...) {
		PERR("C++ exception during xml parsing");
		return 2;
	}

	argv[0] = (char *)"virtualbox";
	argv[1] = (char *)"-m";

	Genode::snprintf(c_mem, sizeof(c_mem), "%u", vm_size / 1024 / 1024);
	argv[2] = c_mem;
	argv[3] = (char *)"-boot";

	if (!Genode::strcmp(c_type, "iso")) {
		argv[4] = (char *)"d";
		argv[5] = (char *)"-cdrom";
	} else 
	if (!Genode::strcmp(c_type, "vdi")) {
		argv[4] = (char *)"c";
		argv[5] = (char *)"-hda";
	} else {
		PERR("invalid configuration - abort");
		return 3;
	}

	argv[6] = c_file;
	argv[7] = (char *)"-ioapic";

	if (9 > argc + 1) {
		PERR("argc argv misconfiguration - abort");
		return 4;
	}

	if (bOverlay)
		argv[8] = (char *)"-overlay";
	else {
		argc   -= 1;
		argv[8] = 0;
	}
	argv[9] = 0;

	PINF("start %s image '%s' with %zu MB Guest memory=%zu",
	     c_type, c_file, vm_size / 1024 / 1024,
	     Genode::env()->ram_session()->avail());

	if (RT_FAILURE(RTR3InitExe(argc, (char ***)&argv, 0))) {
        PERR("Intialization of VBox Runtime failed.");
		return 5;
	}

    return TrustedMain(argc, argv, NULL);
}

} /* EXTERN "C" */
