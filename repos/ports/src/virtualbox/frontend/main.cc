/*
 * \brief  Port of VirtualBox to Genode
 * \author Norman Feske
 * \author Alexander Boettcher
 * \date   2013-08-20
 */

/*
 * Copyright (C) 2013-2014 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */


/* Genode includes */
#include <base/printf.h>
#include <os/config.h>

/* Virtualbox includes */
#include <iprt/initterm.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <VBox/vmm/vmapi.h>

/* Virtualbox includes of generic Main frontend */
#include "ConsoleImpl.h"
#include "MachineImpl.h"
#include "MouseImpl.h"

/* Genode port specific includes */
#include "console.h"
#include "fb.h"

static char c_vbox_file[128];
static char c_vbox_vmname[128];


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
Framebuffer::Framebuffer() { }
Framebuffer::~Framebuffer() { }

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


HRESULT setupmachine()
{
	HRESULT rc;

	static com::Utf8Str vm_config(c_vbox_file);
	static com::Utf8Str vm_name(c_vbox_vmname);
	settings::MachineConfigFile * machine_config = new settings::MachineConfigFile(&vm_config);

	/* Machine object */
	ComObjPtr<Machine> machine;
	rc = machine.createObject();
	if (FAILED(rc))
		return rc;

	/* Virtualbox object */
	ComObjPtr<VirtualBox> virtualbox;
	rc = virtualbox.createObject();
	if (FAILED(rc))
		return rc;

	rc = machine->init(virtualbox, vm_name, *machine_config);
	if (FAILED(rc))
		return rc;

	rc = virtualbox->RegisterMachine(machine);
	if (FAILED(rc))
		return rc;

	// open a session
	ComObjPtr<ISession> session;
	rc = session.createObject();
	if (FAILED(rc))
		return rc;

	rc = machine->LockMachine(session, LockType_VM);
	if (FAILED(rc))
		return rc;

	/* Console object */
	GenodeConsole * gConsole = new GenodeConsole();
	/* Derived from Session::AssignMachine method in Main/src-client/SessionImpl.cpp */
	static IInternalMachineControl control;
	rc = control.init(machine);
	if (FAILED(rc))
		return rc;

	rc = gConsole->init(machine, &control, LockType_VM);
	if (FAILED(rc))
		return rc;

	/* Validate configured memory of vbox file and Genode config */
	ULONG memory_vbox;
	rc = machine->COMGETTER(MemorySize)(&memory_vbox);
	if (FAILED(rc))
		return rc;

	/* request max available memory */
	size_t memory_genode = Genode::env()->ram_session()->avail() >> 20;
	size_t memory_vmm    = 28;

	if (memory_vbox + memory_vmm > memory_genode) {
		PERR("Configured memory %u MB (vbox file) is insufficient.",
		     memory_vbox);
		PERR("%zu MB (1) - %zu MB (2) = %zu MB (3)",
			 memory_genode, memory_vmm, memory_genode - memory_vmm);
		PERR("(1) available memory based defined by Genode config");
		PERR("(2) minimum memory required for VBox VMM");
		PERR("(3) maximal available memory to VM");
		return E_FAIL;
	}

	/* Display object */
	ComPtr<Display> display;
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
		Genodefb *fb = new Genodefb();
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
	if (RT_FAILURE(rc))
		return rc;
	Assert (&*gMouse);

	/* request keyboard object */
	ComPtr<IKeyboard> gKeyboard;
	rc = gConsole->COMGETTER(Keyboard)(gKeyboard.asOutParam());
	if (FAILED(rc))
		return rc;
	Assert (&*gKeyboard);

	/* handle input of Genode and forward it to VMM layer */
	while (true) {
		gConsole->eventWait(gKeyboard, gMouse);
	}

	Assert(!"return not expected");
	return E_FAIL;
}


int main(int argc, char **argv)
{
	try {
		using namespace Genode;

		Xml_node node = config()->xml_node();
		Xml_node::Attribute vbox_file = node.attribute("vbox_file");
		vbox_file.value(c_vbox_file, sizeof(c_vbox_file));
		Xml_node::Attribute vm_name = node.attribute("vm_name");
		vm_name.value(c_vbox_vmname, sizeof(c_vbox_vmname));
	} catch (...) {
		PERR("Missing attributes in configuration, minimum requirements: ");
		PERR("  <config vbox_file=\"...\" vm_name=\"...\">" );
		throw;
	}

	int rc = RTR3InitExe(argc, &argv, 0);
	if (RT_FAILURE(rc))
		return -1;

	HRESULT hrc = setupmachine();
	if (FAILED(hrc)) {
		PERR("Start-up of VMM failed - reason %d - exiting ...", hrc);
		return -2;
	}

	PERR("VMM exiting ...");

	return 0;
}
