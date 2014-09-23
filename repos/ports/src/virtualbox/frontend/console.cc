#include <base/printf.h>

#include <VBox/settings.h>

#include "ConsoleImpl.h"
#include "MachineImpl.h"

static const bool debug = false;

#define DUMMY(X) \
	{ \
		PERR("%s called (%s), not implemented, eip=%p", __func__, __FILE__, \
		     __builtin_return_address(0)); \
		while (1) \
			asm volatile ("ud2a"); \
		\
		return X; \
	}

#define TRACE(X) \
	{ \
		if (debug) \
			PDBG(" called (%s) - eip=%p", __FILE__, \
			     __builtin_return_address(0)); \
		return X; \
	}

void    Console::uninit()                                                       DUMMY()
HRESULT Console::resume(Reason_T aReason)                                       DUMMY(E_FAIL)
HRESULT Console::pause(Reason_T aReason)                                        DUMMY(E_FAIL)
void    Console::enableVMMStatistics(BOOL aEnable)                              DUMMY()
void    Console::changeClipboardMode(ClipboardMode_T aClipboardMode)            DUMMY()
HRESULT Console::updateMachineState(MachineState_T aMachineState)               DUMMY(E_FAIL)

HRESULT Console::attachToTapInterface(INetworkAdapter *networkAdapter)
{
	HRESULT rc = S_OK;

	ULONG slot = 0;
	rc = networkAdapter->COMGETTER(Slot)(&slot);
	AssertComRC(rc);

	maTapFD[slot] = (RTFILE)1;

	TRACE(S_OK)
}

HRESULT Console::teleporterTrg(UVM*, Machine*, com::Utf8Str*, bool, Progress*,
                               bool*)                                           DUMMY(E_FAIL)
HRESULT Console::detachFromTapInterface(INetworkAdapter *networkAdapter)        DUMMY(E_FAIL)
HRESULT Console::saveState(Reason_T aReason, IProgress **aProgress)             DUMMY(E_FAIL)
HRESULT Console::get_Debugger(MachineDebugger**)                                DUMMY(E_FAIL)
HRESULT Console::get_USBDevices(unsigned int*, IUSBDevice***)                   DUMMY(E_FAIL)
HRESULT Console::get_RemoteUSBDevices(unsigned int*, IHostUSBDevice***)         DUMMY(E_FAIL)
HRESULT Console::get_VRDEServerInfo(VRDEServerInfo**)                           DUMMY(E_FAIL)
HRESULT Console::get_SharedFolders(unsigned int*, SharedFolder***)              DUMMY(E_FAIL)
HRESULT Console::get_AttachedPCIDevices(unsigned int*, PCIDeviceAttachment***)  DUMMY(E_FAIL)
HRESULT Console::get_UseHostClipboard(bool*)                                    DUMMY(E_FAIL)
HRESULT Console::set_UseHostClipboard(bool)                                     DUMMY(E_FAIL)
HRESULT Console::get_EmulatedUSB(EmulatedUSB**)                                 DUMMY(E_FAIL)
HRESULT Console::Reset()                                                        DUMMY(E_FAIL)
HRESULT Console::Pause()                                                        DUMMY(E_FAIL)
HRESULT Console::Resume()                                                       DUMMY(E_FAIL)
HRESULT Console::PowerButton()                                                  DUMMY(E_FAIL)
HRESULT Console::SleepButton()                                                  DUMMY(E_FAIL)
HRESULT Console::GetPowerButtonHandled(bool*)                                   DUMMY(E_FAIL)
HRESULT Console::GetGuestEnteredACPIMode(bool*)                                 DUMMY(E_FAIL)
HRESULT Console::SaveState(Progress**)                                          DUMMY(E_FAIL)
HRESULT Console::AdoptSavedState(unsigned short*)                               DUMMY(E_FAIL)
HRESULT Console::DiscardSavedState(bool)                                        DUMMY(E_FAIL)
HRESULT Console::GetDeviceActivity(DeviceType_T, DeviceActivity_T*)             DUMMY(E_FAIL)
HRESULT Console::AttachUSBDevice(unsigned short*)                               DUMMY(E_FAIL)
HRESULT Console::DetachUSBDevice(unsigned short*, IUSBDevice**)                 DUMMY(E_FAIL)
HRESULT Console::FindUSBDeviceByAddress(unsigned short*, IUSBDevice**)          DUMMY(E_FAIL)
HRESULT Console::FindUSBDeviceById(unsigned short*, IUSBDevice**)               DUMMY(E_FAIL)
HRESULT Console::CreateSharedFolder(unsigned short*, unsigned short*, bool,
                                    bool)                                       DUMMY(E_FAIL)
HRESULT Console::RemoveSharedFolder(unsigned short*) DUMMY(E_FAIL)
HRESULT Console::TakeSnapshot(unsigned short*, unsigned short*, Progress**)     DUMMY(E_FAIL)
HRESULT Console::DeleteSnapshot(unsigned short*, Progress**)                    DUMMY(E_FAIL)
HRESULT Console::DeleteSnapshotAndAllChildren(unsigned short*, Progress**)      DUMMY(E_FAIL)
HRESULT Console::DeleteSnapshotRange(unsigned short*, unsigned short*,
                                     Progress**)                                DUMMY(E_FAIL)
HRESULT Console::RestoreSnapshot(Snapshot*, Progress**)                         DUMMY(E_FAIL)
HRESULT Console::Teleport(unsigned short*, unsigned int, unsigned short*,
                          unsigned int, Progress**)                             DUMMY(E_FAIL)
HRESULT Console::setDiskEncryptionKeys(const Utf8Str &strCfg)                   DUMMY(E_FAIL)

DisplayMouseInterface *Console::getDisplayMouseInterface()                      DUMMY(nullptr)

void    Console::onMouseCapabilityChange(BOOL, BOOL, BOOL, BOOL)                TRACE()
void    Console::onAdditionsStateChange()                                       TRACE()
void    Console::onAdditionsOutdated()                                          DUMMY()
void    Console::onMousePointerShapeChange(bool, bool, uint32_t, uint32_t,
                                        uint32_t, uint32_t,
                                        ComSafeArrayIn(uint8_t, aShape))        DUMMY()
void    Console::onKeyboardLedsChange(bool, bool, bool)                         TRACE()
HRESULT Console::onVideoCaptureChange()                                         DUMMY(E_FAIL)
HRESULT Console::onSharedFolderChange(BOOL aGlobal)                             DUMMY(E_FAIL)
HRESULT Console::onUSBControllerChange()                                        DUMMY(E_FAIL)
HRESULT Console::onCPUChange(ULONG aCPU, BOOL aRemove)                          DUMMY(E_FAIL)
HRESULT Console::onClipboardModeChange(ClipboardMode_T aClipboardMode)          DUMMY(E_FAIL)
HRESULT Console::onDragAndDropModeChange(DragAndDropMode_T aDragAndDropMode)    DUMMY(E_FAIL)
HRESULT Console::onCPUExecutionCapChange(ULONG aExecutionCap)                   DUMMY(E_FAIL)
HRESULT Console::onStorageControllerChange()                                    DUMMY(E_FAIL)
HRESULT Console::onMediumChange(IMediumAttachment *aMediumAttachment, BOOL)     DUMMY(E_FAIL)
HRESULT Console::onVRDEServerChange(BOOL aRestart)                              DUMMY(E_FAIL)
HRESULT Console::onUSBDeviceAttach(IUSBDevice *, IVirtualBoxErrorInfo *, ULONG) DUMMY(E_FAIL)
HRESULT Console::onUSBDeviceDetach(IN_BSTR aId, IVirtualBoxErrorInfo *aError)   DUMMY(E_FAIL)
HRESULT Console::onShowWindow(BOOL aCheck, BOOL *aCanShow, LONG64 *aWinId)      DUMMY(E_FAIL)
HRESULT Console::onNetworkAdapterChange(INetworkAdapter *, BOOL changeAdapter)  DUMMY(E_FAIL)
HRESULT Console::onStorageDeviceChange(IMediumAttachment *, BOOL, BOOL)         DUMMY(E_FAIL)
HRESULT Console::onBandwidthGroupChange(IBandwidthGroup *aBandwidthGroup)       DUMMY(E_FAIL)
HRESULT Console::onSerialPortChange(ISerialPort *aSerialPort)                   DUMMY(E_FAIL)
HRESULT Console::onParallelPortChange(IParallelPort *aParallelPort)             DUMMY(E_FAIL)
HRESULT Console::onlineMergeMedium(IMediumAttachment *aMediumAttachment,
                                   ULONG aSourceIdx, ULONG aTargetIdx,
                                   IProgress *aProgress)                        DUMMY(E_FAIL)
