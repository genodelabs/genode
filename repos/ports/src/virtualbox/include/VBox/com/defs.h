#ifndef ___VBox_com_defs_h
#define ___VBox_com_defs_h

#include <iprt/types.h>

typedef       short unsigned int * BSTR;
typedef       BSTR                 IN_BSTR;
typedef const short unsigned int * CBSTR;
typedef       short unsigned int   OLECHAR;
typedef       unsigned int         PRUint32;
typedef       OLECHAR              PRUnichar;
typedef       bool                 BOOL;
typedef       unsigned char        BYTE;

#define       FALSE                false
#define       TRUE                 true

#define SUCCEEDED(X) ((X) == VINF_SUCCESS)
#define FAILED(X) ((X) != VINF_SUCCESS)

#define ComSafeArrayInArg(aArg)             aArg##Size, aArg
#define ComSafeArrayOutArg(aArg)            aArg##Size, aArg
#define ComSafeArrayOut(aType, aArg)        PRUint32 *aArg##Size, aType **aArg
#define ComSafeArrayIn(aType, aArg)         unsigned aArg##Size, aType *aArg
#define ComSafeGUIDArrayIn(aArg)            PRUint32 aArg##Size, const nsID **aArg
#define ComSafeGUIDArrayInArg(aArg)         ComSafeArrayInArg(aArg)
#define ComSafeArrayOutIsNull(aArg)         ((aArg) == NULL)
#define ComSafeArrayInIsNull(aArg)          ((aArg) == NULL)

#define COM_STRUCT_OR_CLASS(I) class I

#define FAILED_DEAD_INTERFACE(rc)  (   (rc) != VINF_SUCCESS )

enum HRESULT {
	S_OK,
	E_ACCESSDENIED,
	E_OUTOFMEMORY,
	E_INVALIDARG,
	E_FAIL,
	E_POINTER,
	E_NOTIMPL,
	E_UNEXPECTED,
	E_NOINTERFACE,
	E_ABORT,
	VBOX_E_VM_ERROR,
	VBOX_E_INVALID_VM_STATE,
	VBOX_E_INVALID_OBJECT_STATE,
	VBOX_E_INVALID_SESSION_STATE,
	VBOX_E_OBJECT_NOT_FOUND,
	VBOX_E_FILE_ERROR,
	VBOX_E_OBJECT_IN_USE,
	VBOX_E_NOT_SUPPORTED,
	VBOX_E_IPRT_ERROR,
	VBOX_E_PDM_ERROR,
	VBOX_E_HOST_ERROR,
	VBOX_E_XML_ERROR,
};

typedef struct { char x [sizeof(RTUUID)]; } GUID;

inline GUID& stuffstuff() {
	static GUID stuff;
	return stuff;
}
#define COM_IIDOF(X) stuffstuff()
#define getStaticClassIID() stuffstuff()

#define IN_GUID GUID
#define OUT_GUID GUID *

extern "C"
{
	BSTR SysAllocString(const OLECHAR* sz);
	BSTR SysAllocStringByteLen(char *psz, unsigned int len);
	BSTR SysAllocStringLen(const OLECHAR *pch, unsigned int cch);
	void SysFreeString(BSTR bstr);
	unsigned int SysStringByteLen(BSTR bstr);
	unsigned int SysStringLen(BSTR bstr);
}

typedef signed   int   LONG;
typedef unsigned int   ULONG;
typedef unsigned short USHORT;
typedef          short SHORT;
typedef signed   long long LONG64;
typedef unsigned long long ULONG64;

/* capiidl.xsl */
typedef enum VARTYPE
{
    VT_I2 = 2,
    VT_I4 = 3,
    VT_BSTR = 8,
    VT_DISPATCH = 9,
    VT_BOOL = 11,
    VT_UNKNOWN = 13,
    VT_I1 = 16,
    VT_UI1 = 17,
    VT_UI2 = 18,
    VT_UI4 = 19,
    VT_I8 = 20,
    VT_UI8 = 21,
    VT_HRESULT = 25
} VARTYPE;

typedef struct SAFEARRAY
{
    void *pv;
    ULONG c;
} SAFEARRAY;

enum AccessMode_T
{
	AccessMode_ReadOnly,
	AccessMode_ReadWrite,
};

enum AdditionsFacilityClass_T
{
	AdditionsFacilityClass_None,
	AdditionsFacilityClass_Driver,
	AdditionsFacilityClass_Feature,
	AdditionsFacilityClass_Program,
	AdditionsFacilityClass_Service,
};

enum AdditionsFacilityStatus_T
{
	AdditionsFacilityStatus_Unknown,
	AdditionsFacilityStatus_Active,
};

enum AdditionsFacilityType_T
{
	AdditionsFacilityType_None,
	AdditionsFacilityType_AutoLogon,
	AdditionsFacilityType_Graphics,
	AdditionsFacilityType_Seamless,
	AdditionsFacilityType_VBoxService,
	AdditionsFacilityType_VBoxGuestDriver,
	AdditionsFacilityType_VBoxTrayClient,
};

enum CopyFileFlag_T { };
enum DeviceActivity_T  { };
enum FsObjType_T { };
enum FileStatus_T { };
enum FileSeekType_T { };
enum DragAndDropAction_T { };
enum GuestSessionStatus_T { };
enum GuestSessionWaitForFlag_T { };
enum GuestSessionWaitResult_T { };
enum DirectoryCreateFlag_T { };
enum DirectoryOpenFlag_T { };
enum DirectoryRemoveRecFlag_T { };
enum PathRenameFlag_T { };
enum SymlinkType_T { };
enum SymlinkReadFlag_T { };
enum AdditionsUpdateFlag_T { };
enum AdditionsRunLevelType_T
{
	AdditionsRunLevelType_None,
	AdditionsRunLevelType_System,
	AdditionsRunLevelType_Desktop,
	AdditionsRunLevelType_Userland,
};

enum GuestUserState_T {
};

enum MouseButtonState
{
	MouseButtonState_LeftButton = 0x01,
	MouseButtonState_RightButton = 0x02,
	MouseButtonState_MiddleButton = 0x04,
	MouseButtonState_WheelUp = 0x08,
	MouseButtonState_WheelDown = 0x10,
	MouseButtonState_XButton1 = 0x20,
	MouseButtonState_XButton2 = 0x40,
	MouseButtonState_MouseStateMask = 0x7F
};

enum GuestMouseEventMode_T
{
	GuestMouseEventMode_Absolute,
	GuestMouseEventMode_Relative,
};

enum GUEST_FILE_SEEKTYPE { };
enum ProcessPriority_T
{
	ProcessPriority_Default,
};

enum FramebufferPixelFormat
{
	FramebufferPixelFormat_Opaque = 0,
	FramebufferPixelFormat_FOURCC_RGB = 0x32424752,
};

enum GuestMonitorChangedEventType
{
	GuestMonitorChangedEventType_Enabled,
	GuestMonitorChangedEventType_Disabled,
	GuestMonitorChangedEventType_NewOrigin,
};

enum VBoxEventType_T
{
	VBoxEventType_Invalid = 0,
	VBoxEventType_Any = 1,
	VBoxEventType_Vetoable = 2,
	VBoxEventType_MachineEvent = 3,
	VBoxEventType_SnapshotEvent = 4,
	VBoxEventType_InputEvent = 5,
	VBoxEventType_LastWildcard = 31,
	VBoxEventType_OnMachineStateChanged = 32,
	VBoxEventType_OnMachineDataChanged = 33,
	VBoxEventType_OnExtraDataChanged = 34,
	VBoxEventType_OnExtraDataCanChange = 35,
	VBoxEventType_OnMediumRegistered = 36,
	VBoxEventType_OnMachineRegistered = 37,
	VBoxEventType_OnSessionStateChanged = 38,
	VBoxEventType_OnSnapshotTaken = 39,
	VBoxEventType_OnSnapshotDeleted = 40,
	VBoxEventType_OnSnapshotChanged = 41,
	VBoxEventType_OnGuestPropertyChanged = 42,
	VBoxEventType_OnMousePointerShapeChanged = 43,
	VBoxEventType_OnMouseCapabilityChanged = 44,
	VBoxEventType_OnKeyboardLedsChanged = 45,
	VBoxEventType_OnStateChanged = 46,
	VBoxEventType_OnAdditionsStateChanged = 47,
	VBoxEventType_OnNetworkAdapterChanged = 48,
	VBoxEventType_OnSerialPortChanged = 49,
	VBoxEventType_OnParallelPortChanged = 50,
	VBoxEventType_OnStorageControllerChanged = 51,
	VBoxEventType_OnMediumChanged = 52,
	VBoxEventType_OnVRDEServerChanged = 53,
	VBoxEventType_OnUSBControllerChanged = 54,
	VBoxEventType_OnUSBDeviceStateChanged = 55,
	VBoxEventType_OnSharedFolderChanged = 56,
	VBoxEventType_OnRuntimeError = 57,
	VBoxEventType_OnCanShowWindow = 58,
	VBoxEventType_OnShowWindow = 59,
	VBoxEventType_OnCPUChanged = 60,
	VBoxEventType_OnVRDEServerInfoChanged = 61,
	VBoxEventType_OnEventSourceChanged = 62,
	VBoxEventType_OnCPUExecutionCapChanged = 63,
	VBoxEventType_OnGuestKeyboard = 64,
	VBoxEventType_OnGuestMouse = 65,
	VBoxEventType_OnNATRedirect = 66,
	VBoxEventType_OnHostPCIDevicePlug = 67,
	VBoxEventType_OnVBoxSVCAvailabilityChanged = 68,
	VBoxEventType_OnBandwidthGroupChanged = 69,
	VBoxEventType_OnGuestMonitorChanged = 70,
	VBoxEventType_OnStorageDeviceChanged = 71,
	VBoxEventType_OnClipboardModeChanged = 72,
	VBoxEventType_OnDragAndDropModeChanged = 73,
	VBoxEventType_OnGuestMultiTouch = 74,
	VBoxEventType_Last = 75
};

enum ProcessStatus_T { };
enum ProcessInputStatus_T { };
enum ProcessInputFlag_T { };
enum ProcessWaitResult_T { };
enum ProcessWaitForFlag_T { };

enum ProcessCreateFlag_T
{
	ProcessCreateFlag_None,
};

enum SessionType_T
{
	SessionType_Null,
	SessionType_WriteLock,
	SessionType_Remote,
	SessionType_Shared,
};

enum MachineState_T {
	MachineState_Null,
	MachineState_Aborted,
	MachineState_Running,
	MachineState_Paused,
	MachineState_Teleporting,
	MachineState_LiveSnapshotting,
	MachineState_Stuck,
	MachineState_Starting,
	MachineState_Stopping,
	MachineState_Saving,
	MachineState_Restoring,
	MachineState_TeleportingPausedVM,
	MachineState_TeleportingIn,
	MachineState_RestoringSnapshot,
	MachineState_DeletingSnapshot,
	MachineState_SettingUp,
	MachineState_FaultTolerantSyncing,
	MachineState_PoweredOff,
	MachineState_Teleported,
	MachineState_Saved,
	MachineState_DeletingSnapshotOnline,
	MachineState_DeletingSnapshotPaused,
};

enum CleanupMode_T {
	CleanupMode_UnregisterOnly,
	CleanupMode_DetachAllReturnHardDisksOnly,
	CleanupMode_Full,
 };

enum CloneMode_T {
	CloneMode_MachineState,
	CloneMode_AllStates,
	CloneMode_MachineAndChildStates,
};

enum CloneOptions_T {
	CloneOptions_Link,
	CloneOptions_KeepAllMACs,
	CloneOptions_KeepNATMACs,
	CloneOptions_KeepDiskNames,
};

enum LockType_T {
	LockType_Shared,
	LockType_Write,
	LockType_VM,
};

enum SessionState_T {
	SessionState_Null,
	SessionState_Locked,
	SessionState_Spawning,
	SessionState_Unlocking,
	SessionState_Unlocked,
};

enum Reason_T
{
	Reason_Unspecified,
	Reason_HostSuspend,
	Reason_HostResume,
	Reason_HostBatteryLow,
};

enum MediumFormatCapabilities_T
{
	MediumFormatCapabilities_Uuid          = 0x01,
	MediumFormatCapabilities_CreateFixed   = 0x02,
	MediumFormatCapabilities_CreateDynamic = 0x04,
	MediumFormatCapabilities_Differencing  = 0x10,
	MediumFormatCapabilities_File          = 0x40
};

enum DataType_T {
	DataType_Int32,
	DataType_Int8,
	DataType_String,
};

enum DataFlags_T {
	DataFlags_Array, 
};

enum MediumVariant_T {
	MediumVariant_Standard,
	MediumVariant_Fixed,
	MediumVariant_Diff,
	MediumVariant_VmdkStreamOptimized,
	MediumVariant_NoCreateDir,
};
enum HostNetworkInterfaceType_T { };

enum NATAliasMode_T
{
	NATAliasMode_AliasLog = 0x1,
	NATAliasMode_AliasProxyOnly = 0x02,
	NATAliasMode_AliasUseSamePorts = 0x04,
};

enum MediumState_T {
    MediumState_NotCreated = 0,
    MediumState_Created = 1,
    MediumState_LockedRead = 2,
    MediumState_LockedWrite = 3,
    MediumState_Inaccessible = 4,
    MediumState_Creating = 5,
    MediumState_Deleting = 6
};

enum AuthType_T {
	AuthType_Null,
	AuthType_Guest,
	AuthType_External,
};

enum BIOSBootMenuMode_T {
	BIOSBootMenuMode_MessageAndMenu,
	BIOSBootMenuMode_Disabled,
	BIOSBootMenuMode_MenuOnly,
};

enum USBControllerType_T {
	USBControllerType_Null,
	USBControllerType_OHCI,
	USBControllerType_EHCI,
	USBControllerType_Last,
};

enum USBDeviceFilterAction_T {
	USBDeviceFilterAction_Null,
	USBDeviceFilterAction_Ignore,
	USBDeviceFilterAction_Hold,
};

enum DeviceType_T {
	DeviceType_Null,
	DeviceType_HardDisk,
	DeviceType_DVD,
	DeviceType_Floppy,
	DeviceType_Network,
	DeviceType_USB,
	DeviceType_SharedFolder,
};

enum MediumType_T {
	MediumType_Normal,
	MediumType_Immutable,
	MediumType_Writethrough,
	MediumType_Shareable,
	MediumType_Readonly,
	MediumType_MultiAttach,
};

enum NATProtocol_T {
	NATProtocol_TCP,
	NATProtocol_UDP,
};

enum NetworkAdapterType_T {
	NetworkAdapterType_Am79C970A,
	NetworkAdapterType_Am79C973,
	NetworkAdapterType_I82540EM,
	NetworkAdapterType_I82543GC,
	NetworkAdapterType_I82545EM,
	NetworkAdapterType_Virtio,
};

enum ProcessorFeature_T
{
	ProcessorFeature_HWVirtEx,
	ProcessorFeature_LongMode,
	ProcessorFeature_NestedPaging,
	ProcessorFeature_PAE,
};

enum CPUPropertyType_T
{
	CPUPropertyType_Null,
	CPUPropertyType_PAE,
	CPUPropertyType_Synthetic,
	CPUPropertyType_LongMode,
	CPUPropertyType_TripleFaultReset,
};

/* End of enum CPUPropertyType Declaration */
enum AudioDriverType_T {
	AudioDriverType_Null,
	AudioDriverType_WinMM,
	AudioDriverType_DirectSound,
	AudioDriverType_SolAudio,
	AudioDriverType_ALSA,
	AudioDriverType_Pulse,
	AudioDriverType_OSS,
	AudioDriverType_CoreAudio,
	AudioDriverType_MMPM,
};

enum PortMode_T {
	PortMode_Disconnected,
	PortMode_HostPipe,
	PortMode_HostDevice,
	PortMode_RawFile,
};

enum BandwidthGroupType_T {
	BandwidthGroupType_Null,
	BandwidthGroupType_Disk,
	BandwidthGroupType_Network,
};

enum ClipboardMode_T {
	ClipboardMode_Disabled,
	ClipboardMode_HostToGuest,
	ClipboardMode_GuestToHost,
	ClipboardMode_Bidirectional,
};

enum FaultToleranceState_T {
	FaultToleranceState_Inactive,
	FaultToleranceState_Master,
	FaultToleranceState_Standby,
};

enum AudioControllerType_T {
	AudioControllerType_AC97,
	AudioControllerType_HDA,
	AudioControllerType_SB16,
};

enum NetworkAttachmentType_T {
	NetworkAttachmentType_Null,
	NetworkAttachmentType_NAT,
	NetworkAttachmentType_Bridged,
	NetworkAttachmentType_Internal,
	NetworkAttachmentType_HostOnly,
	NetworkAttachmentType_Generic,
	NetworkAttachmentType_NATNetwork,
};

enum NetworkAdapterPromiscModePolicy_T {
	NetworkAdapterPromiscModePolicy_Deny,
	NetworkAdapterPromiscModePolicy_AllowNetwork,
	NetworkAdapterPromiscModePolicy_AllowAll,
};

enum StorageBus_T {
	StorageBus_Null,
	StorageBus_IDE,
	StorageBus_SATA,
	StorageBus_SAS,
	StorageBus_SCSI,
	StorageBus_Floppy,
};

enum FirmwareType_T {
	FirmwareType_BIOS,
	FirmwareType_EFI,
	FirmwareType_EFI32,
	FirmwareType_EFI64,
	FirmwareType_EFIDUAL,
};

enum GraphicsControllerType_T {
	GraphicsControllerType_Null,
	GraphicsControllerType_VBoxVGA,
	GraphicsControllerType_VMSVGA,
};

enum AutostopType_T {
	AutostopType_Disabled,
	AutostopType_SaveState,
	AutostopType_PowerOff,
	AutostopType_AcpiShutdown,
};

enum DragAndDropMode_T {
	DragAndDropMode_Disabled,
	DragAndDropMode_HostToGuest,
	DragAndDropMode_GuestToHost,
	DragAndDropMode_Bidirectional,
};

enum StorageControllerType_T {
	StorageControllerType_PIIX3,
	StorageControllerType_IntelAhci,
	StorageControllerType_LsiLogic,
	StorageControllerType_BusLogic,
	StorageControllerType_PIIX4,
	StorageControllerType_ICH6,
	StorageControllerType_I82078,
	StorageControllerType_LsiLogicSas,
};

enum KeyboardHIDType_T {
	KeyboardHIDType_None,
	KeyboardHIDType_PS2Keyboard,
	KeyboardHIDType_USBKeyboard,
	KeyboardHIDType_ComboKeyboard,
};

enum PointingHIDType_T {
	PointingHIDType_None,
	PointingHIDType_PS2Mouse,
	PointingHIDType_USBMouse,
	PointingHIDType_USBTablet,
	PointingHIDType_ComboMouse,
	PointingHIDType_USBMultiTouch,
};

enum ChipsetType_T {
	ChipsetType_Null,
	ChipsetType_ICH9,
	ChipsetType_PIIX3,
};

enum DhcpOpt_T {
	DhcpOpt_SubnetMask,
	DhcpOpt_Router,
};

enum SettingsVersion_T {
	SettingsVersion_Null,
	SettingsVersion_v1_3,
	SettingsVersion_v1_4,
	SettingsVersion_v1_5,
	SettingsVersion_v1_6,
	SettingsVersion_v1_7,
	SettingsVersion_v1_8,
	SettingsVersion_v1_9,
	SettingsVersion_v1_10,
	SettingsVersion_v1_11,
	SettingsVersion_v1_12,
	SettingsVersion_v1_13,
	SettingsVersion_v1_14,
	SettingsVersion_Future,
};

enum HWVirtExPropertyType_T
{
	HWVirtExPropertyType_Enabled,
	HWVirtExPropertyType_Force,
	HWVirtExPropertyType_NestedPaging,
	HWVirtExPropertyType_LargePages,
	HWVirtExPropertyType_VPID,
	HWVirtExPropertyType_UnrestrictedExecution,
};

#endif /* !___VBox_com_defs_h */
