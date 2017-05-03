/*
 * \brief  Port of VirtualBox to Genode
 * \author Norman Feske
 * \author Alexander Boettcher
 * \date   2013-08-20
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */


/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <base/log.h>
#include <libc/component.h>

/* Virtualbox includes */
#include <iprt/initterm.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <VBox/com/com.h>
#include <VBox/vmm/vmapi.h>

/* Virtualbox includes of generic Main frontend */
#include "ConsoleImpl.h"
#include "MachineImpl.h"
#include "MouseImpl.h"
#include "SessionImpl.h"
#include "VirtualBoxImpl.h"

/* Genode port specific includes */
#include "console.h"
#include "fb.h"
#include "../sup.h"

static char c_vbox_file[128];
static char c_vbox_vmname[128];

extern "C" void init_libc_vbox_logger(void);


/**
 * xpcom style memory allocation
 */
void * nsMemory::Alloc(size_t size)
{
	return new char[size];
}
void  nsMemory::Free(void* ptr)
{
	Assert(ptr);
	delete [] reinterpret_cast<char *>(ptr);
}
void *nsMemory::Realloc(void* ptr, size_t size)
{
	Assert(!"not implemented");
	return nullptr;
}
void * nsMemory::Clone(const void*, size_t)
{
	Assert(!"not implemented");
	return nullptr;
}

/**
 * Other stuff
 */

int com::GetVBoxUserHomeDirectory(char *aDir, size_t aDirLen, bool fCreateDir)
{
    AssertReturn(aDir, VERR_INVALID_POINTER);
    AssertReturn(aDirLen > 1, VERR_BUFFER_OVERFLOW);

	memcpy(aDir, "/", 1);
	aDir[1] = 0;
	return VINF_SUCCESS;
}

extern "C"
RTDECL(int) RTPathUserHome(char *pszPath, size_t cchPath)
{
	return com::GetVBoxUserHomeDirectory(pszPath, cchPath);
}


HRESULT setupmachine(Genode::Env &env)
{
	HRESULT rc;

	static com::Utf8Str vm_config(c_vbox_file);
	static com::Utf8Str vm_name(c_vbox_vmname);

	/* Machine object */
	static ComObjPtr<Machine> machine;
	rc = machine.createObject();
	if (FAILED(rc))
		return rc;

	/* Virtualbox object */
	static ComObjPtr<VirtualBox> virtualbox;
	rc = virtualbox.createObject();
	if (FAILED(rc))
		return rc;

	rc = machine->initFromSettings(virtualbox, vm_config, nullptr);
	if (FAILED(rc))
		return rc;

	rc = genode_setup_machine(machine);
	if (FAILED(rc))
		return rc;

	rc = virtualbox->RegisterMachine(machine);
	if (FAILED(rc))
		return rc;

	// open a session
	static ComObjPtr<Session> session;
	rc = session.createObject();
	if (FAILED(rc))
		return rc;

	rc = machine->LockMachine(session, LockType_VM);
	if (FAILED(rc))
		return rc;

	/* Console object */
	static ComPtr<IConsole> gConsole;
	rc = session->COMGETTER(Console)(gConsole.asOutParam());

	/* handle input of Genode and forward it to VMM layer */
	static ComPtr<GenodeConsole> genodeConsole = gConsole;

	genodeConsole->init_clipboard();

	/* Display object */
	static ComPtr<IDisplay> display;
	rc = gConsole->COMGETTER(Display)(display.asOutParam());
	if (FAILED(rc))
		return rc;

	ULONG cMonitors = 1;
	rc = machine->COMGETTER(MonitorCount)(&cMonitors);
	if (FAILED(rc))
		return rc;

	unsigned uScreenId;
	for (uScreenId = 0; uScreenId < cMonitors; uScreenId++)
	{
		Genodefb *fb = new Genodefb(env);
		display->SetFramebuffer(uScreenId, fb);
	}

	/* Power up the VMM */
	ComPtr <IProgress> progress;
	rc = gConsole->PowerUp(progress.asOutParam());
	if (FAILED(rc))
		return rc;

	/* wait until VM is up */
	MachineState_T machineState = MachineState_Null;
	do {
		if (machineState != MachineState_Null)
			RTThreadSleep(1000);

		rc = machine->COMGETTER(State)(&machineState);
	} while (machineState == MachineState_Starting);
	if (rc != S_OK || (machineState != MachineState_Running))
		return E_FAIL;

	/* request mouse object */
	static ComPtr<IMouse> gMouse;
	rc = gConsole->COMGETTER(Mouse)(gMouse.asOutParam());
	if (FAILED(rc))
		return rc;
	Assert (&*gMouse);

	/* request keyboard object */
	static ComPtr<IKeyboard> gKeyboard;
	rc = gConsole->COMGETTER(Keyboard)(gKeyboard.asOutParam());
	if (FAILED(rc))
		return rc;
	Assert (&*gKeyboard);

	genodeConsole->init_backends(gKeyboard, gMouse);

	return rc;
}


static Genode::Env *genode_env_ptr = nullptr;


Genode::Env &genode_env()
{
	struct Genode_env_ptr_uninitialized : Genode::Exception { };
	if (!genode_env_ptr)
		throw Genode_env_ptr_uninitialized();

	return *genode_env_ptr;
}


Genode::Allocator &vmm_heap()
{
	static Genode::Heap heap (genode_env().ram(), genode_env().rm());
	return heap;
}


void Libc::Component::construct(Libc::Env &env)
{
	/* make Genode environment accessible via the global 'genode_env()' */
	genode_env_ptr = &env;

	try {
		using namespace Genode;

		Attached_rom_dataspace config(env, "config");
		Xml_node::Attribute vbox_file = config.xml().attribute("vbox_file");
		vbox_file.value(c_vbox_file, sizeof(c_vbox_file));
		Xml_node::Attribute vm_name = config.xml().attribute("vm_name");
		vm_name.value(c_vbox_vmname, sizeof(c_vbox_vmname));
	} catch (...) {
		Genode::error("missing attributes in configuration, minimum requirements: ");
		Genode::error("  <config vbox_file=\"...\" vm_name=\"...\">" );
		throw;
	}

	/* enable stdout/stderr for VBox Log infrastructure */
	init_libc_vbox_logger();

	Libc::with_libc([&] () {
		static char  argv0[] = { '_', 'm', 'a', 'i', 'n', 0};
		static char *argv[1] = { argv0 };
		char **dummy_argv = argv;

		int rc = RTR3InitExe(1, &dummy_argv, 0);
		if (RT_FAILURE(rc))
			throw -1;

		HRESULT hrc = setupmachine(env);
		if (FAILED(hrc)) {
			Genode::error("startup of VMM failed - reason ", hrc, " - exiting ...");
			throw -2;
		}
	});
}
