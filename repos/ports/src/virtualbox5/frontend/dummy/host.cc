#include "VirtualBoxBase.h"

#include <VBox/usbfilter.h>

#include "dummy/macros.h"

#include "HostImpl.h"

static bool debug = false;


DEFINE_EMPTY_CTOR_DTOR(Host)

HRESULT Host::i_findHostDriveByName(DeviceType_T mediumType,
                                  const Utf8Str &strLocationFull,
                                  bool fRefresh,
                                  ComObjPtr<Medium> &pMedium)
	DUMMY(E_FAIL)

HRESULT Host::i_findHostDriveById(DeviceType_T, com::Guid const&, bool,
                                  ComObjPtr<Medium>&)
	TRACE(VBOX_E_OBJECT_NOT_FOUND)
HRESULT Host::i_saveSettings(settings::Host&)
	TRACE(S_OK)
HRESULT Host::i_loadSettings(const settings::Host &)
	TRACE(S_OK)
HRESULT Host::FinalConstruct()
	TRACE(S_OK)


HRESULT Host::init(VirtualBox *aParent)
{
	/* Enclose the state transition NotReady->InInit->Ready */
	AutoInitSpan autoInitSpan(this);
	AssertReturn(autoInitSpan.isOk(), E_FAIL);

	/* Confirm a successful initialization */
	autoInitSpan.setSucceeded();

	return S_OK;
}

void Host::uninit()
	DUMMY()

void Host::i_generateMACAddress(Utf8Str &mac)
{
	static unsigned counter = 1;

	mac = Utf8StrFmt("080027%06X", counter++);

	TRACE();
}

HRESULT Host::generateMACAddress(com::Utf8Str &aAddress)
{
    i_generateMACAddress(aAddress);
    return S_OK;
}

HRESULT Host::getProcessorFeature(ProcessorFeature_T feature, BOOL *supported)
{
	CheckComArgOutPointerValid(supported);

	switch (feature)
	{
		case ProcessorFeature_HWVirtEx:
			*supported = true;
			break;
		case ProcessorFeature_PAE:
			*supported = true;
			break;
		case ProcessorFeature_LongMode:
			*supported = (sizeof(void *) > 4);
			break;
		case ProcessorFeature_NestedPaging:
			*supported = true;
			break;
		default:
			return setError(E_INVALIDARG, tr("The feature value is out of range."));
	}
	return S_OK;
}

#ifdef VBOX_WITH_USB
USBProxyService* Host::i_usbProxyService()
{
	TRACE(nullptr)
}

VirtualBox* Host::i_parent()
	DUMMY(nullptr)

void Host::i_getUSBFilters(Host::USBDeviceFilterList *aGlobalFilters)
	DUMMY()

HRESULT Host::i_checkUSBProxyService()
	TRACE(S_OK)

#include "HostUSBDeviceImpl.h"
bool HostUSBDevice::i_isMatch(const USBDeviceFilter::BackupableUSBDeviceFilterData &)
	DUMMY(false)
#endif


HRESULT Host::createHostOnlyNetworkInterface(ComPtr<IHostNetworkInterface> &,
                                             ComPtr<IProgress> &)
	DUMMY(E_FAIL)
HRESULT Host::removeHostOnlyNetworkInterface(const com::Guid &,
                                             ComPtr<IProgress> &)
	DUMMY(E_FAIL)


HRESULT Host::createUSBDeviceFilter(const com::Utf8Str &,
                                    ComPtr<IHostUSBDeviceFilter> &)
	DUMMY(E_FAIL)
HRESULT Host::insertUSBDeviceFilter(ULONG, const ComPtr<IHostUSBDeviceFilter> &)
	DUMMY(E_FAIL)
HRESULT Host::removeUSBDeviceFilter(ULONG aPosition)
	DUMMY(E_FAIL)


HRESULT Host::getAcceleration3DAvailable(BOOL *)
	DUMMY(E_FAIL)
HRESULT Host::getDomainName(com::Utf8Str &)
	DUMMY(E_FAIL)
HRESULT Host::getDVDDrives(std::vector<ComPtr<IMedium> > &)
	DUMMY(E_FAIL)
HRESULT Host::getFloppyDrives(std::vector<ComPtr<IMedium> > &)
	DUMMY(E_FAIL)
HRESULT Host::getMemorySize(ULONG *)
	DUMMY(E_FAIL)
HRESULT Host::getMemoryAvailable(ULONG *)
	DUMMY(E_FAIL)
HRESULT Host::getNameServers(std::vector<com::Utf8Str> &)
	DUMMY(E_FAIL)
HRESULT Host::getNetworkInterfaces(std::vector<ComPtr<IHostNetworkInterface> > &)
	DUMMY(E_FAIL)
HRESULT Host::getOperatingSystem(com::Utf8Str &)
	DUMMY(E_FAIL)
HRESULT Host::getOSVersion(com::Utf8Str &)
	DUMMY(E_FAIL)
HRESULT Host::getProcessorCount(ULONG *)
	DUMMY(E_FAIL)
HRESULT Host::getProcessorCoreCount(ULONG *)
	DUMMY(E_FAIL)
HRESULT Host::getProcessorDescription(ULONG, com::Utf8Str &)
	DUMMY(E_FAIL)
HRESULT Host::getProcessorOnlineCount(ULONG *)
	DUMMY(E_FAIL)
HRESULT Host::getProcessorOnlineCoreCount(ULONG *)
	DUMMY(E_FAIL)
HRESULT Host::getProcessorSpeed(ULONG, ULONG *)
	DUMMY(E_FAIL)
HRESULT Host::getProcessorCPUIDLeaf(ULONG, ULONG, ULONG, ULONG *, ULONG *,
                                    ULONG *, ULONG *)
	DUMMY(E_FAIL)
HRESULT Host::getSearchStrings(std::vector<com::Utf8Str> &)
	DUMMY(E_FAIL)
HRESULT Host::getUTCTime(LONG64 *)
	DUMMY(E_FAIL)
HRESULT Host::getUSBDevices(std::vector<ComPtr<IHostUSBDevice> > &)
	DUMMY(E_FAIL)
HRESULT Host::getUSBDeviceFilters(std::vector<ComPtr<IHostUSBDeviceFilter> > &)
	DUMMY(E_FAIL)
HRESULT Host::getVideoInputDevices(std::vector<ComPtr<IHostVideoInputDevice> > &)
	DUMMY(E_FAIL)


HRESULT Host::findHostDVDDrive(const com::Utf8Str &, ComPtr<IMedium> &)
	DUMMY(E_FAIL)
HRESULT Host::findHostFloppyDrive(const com::Utf8Str &aName, ComPtr<IMedium> &)
	DUMMY(E_FAIL)
HRESULT Host::findHostNetworkInterfaceByName(const com::Utf8Str &,
                                             ComPtr<IHostNetworkInterface> &)
	DUMMY(E_FAIL)
HRESULT Host::findHostNetworkInterfaceById(const com::Guid &,
                                           ComPtr<IHostNetworkInterface> &)
	DUMMY(E_FAIL)
HRESULT Host::findHostNetworkInterfacesOfType(HostNetworkInterfaceType_T,
                                              std::vector<ComPtr<IHostNetworkInterface> > &)
	DUMMY(E_FAIL)
HRESULT Host::findUSBDeviceByAddress(const com::Utf8Str &,
                                     ComPtr<IHostUSBDevice> &)
	DUMMY(E_FAIL)
HRESULT Host::findUSBDeviceById(const com::Guid &, ComPtr<IHostUSBDevice> &)
	DUMMY(E_FAIL)

HRESULT Host::addUSBDeviceSource(const com::Utf8Str &, const com::Utf8Str &,
                                 const com::Utf8Str &,
                                 const std::vector<com::Utf8Str> &,
                                 const std::vector<com::Utf8Str> &)
	DUMMY(E_FAIL)
HRESULT Host::removeUSBDeviceSource(const com::Utf8Str &aId)
	DUMMY(E_FAIL)
