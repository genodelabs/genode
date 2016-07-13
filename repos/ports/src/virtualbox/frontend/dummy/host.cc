#include "VirtualBoxBase.h"

#include <VBox/usbfilter.h>

#include "dummy/macros.h"

#include "HostImpl.h"

static bool debug = false;


STDMETHODIMP Host::COMGETTER(DVDDrives)(ComSafeArrayOut(IMedium *, drives)) DUMMY(E_FAIL)
STDMETHODIMP Host::COMGETTER(FloppyDrives)(ComSafeArrayOut(IMedium *, drives)) DUMMY(E_FAIL)
STDMETHODIMP Host::COMGETTER(USBDevices)(ComSafeArrayOut(IHostUSBDevice *, aUSBDevices)) DUMMY(E_FAIL)
STDMETHODIMP Host::COMGETTER(USBDeviceFilters)(ComSafeArrayOut(IHostUSBDeviceFilter *, aUSBDeviceFilters)) DUMMY(E_FAIL)
STDMETHODIMP Host::COMGETTER(NetworkInterfaces)(ComSafeArrayOut(IHostNetworkInterface *, aNetworkInterfaces)) DUMMY(E_FAIL)
STDMETHODIMP Host::COMGETTER(NameServers)(ComSafeArrayOut(BSTR, aNameServers)) DUMMY(E_FAIL)
STDMETHODIMP Host::COMGETTER(DomainName)(BSTR *aDomainName) DUMMY(E_FAIL)
STDMETHODIMP Host::COMGETTER(SearchStrings)(ComSafeArrayOut(BSTR, aSearchStrings)) DUMMY(E_FAIL)
STDMETHODIMP Host::COMGETTER(ProcessorCount)(ULONG *count) DUMMY(E_FAIL)
STDMETHODIMP Host::COMGETTER(ProcessorOnlineCount)(ULONG *count) DUMMY(E_FAIL)
STDMETHODIMP Host::COMGETTER(ProcessorCoreCount)(ULONG *count) DUMMY(E_FAIL)
STDMETHODIMP Host::COMGETTER(ProcessorOnlineCoreCount)(ULONG *count) DUMMY(E_FAIL)
STDMETHODIMP Host::GetProcessorSpeed(ULONG cpuId, ULONG *speed) DUMMY(E_FAIL)
STDMETHODIMP Host::GetProcessorDescription(ULONG cpuId, BSTR *description) DUMMY(E_FAIL)
STDMETHODIMP Host::GetProcessorCPUIDLeaf(ULONG aCpuId, ULONG aLeaf, ULONG aSubLeaf, ULONG *aValEAX, ULONG *aValEBX, ULONG *aValECX, ULONG *aValEDX) DUMMY(E_FAIL)
STDMETHODIMP Host::COMGETTER(MemorySize)(ULONG *size) DUMMY(E_FAIL)
STDMETHODIMP Host::COMGETTER(MemoryAvailable)(ULONG *available) DUMMY(E_FAIL)
STDMETHODIMP Host::COMGETTER(OperatingSystem)(BSTR *os) DUMMY(E_FAIL)
STDMETHODIMP Host::COMGETTER(OSVersion)(BSTR *version) DUMMY(E_FAIL)
STDMETHODIMP Host::COMGETTER(UTCTime)(LONG64 *aUTCTime) DUMMY(E_FAIL)
STDMETHODIMP Host::COMGETTER(Acceleration3DAvailable)(BOOL *aSupported) DUMMY(E_FAIL)
STDMETHODIMP Host::COMGETTER(VideoInputDevices)(ComSafeArrayOut(IHostVideoInputDevice*, aVideoInputDevices)) DUMMY(E_FAIL)

    // IHost methods
STDMETHODIMP Host::CreateHostOnlyNetworkInterface(IHostNetworkInterface **aHostNetworkInterface,
                                              IProgress **aProgress) DUMMY(E_FAIL)
STDMETHODIMP Host::RemoveHostOnlyNetworkInterface(IN_BSTR aId, IProgress **aProgress) DUMMY(E_FAIL)
STDMETHODIMP Host::CreateUSBDeviceFilter(IN_BSTR aName, IHostUSBDeviceFilter **aFilter) DUMMY(E_FAIL)
STDMETHODIMP Host::InsertUSBDeviceFilter(ULONG aPosition, IHostUSBDeviceFilter *aFilter) DUMMY(E_FAIL)
STDMETHODIMP Host::RemoveUSBDeviceFilter(ULONG aPosition) DUMMY(E_FAIL)

STDMETHODIMP Host::FindHostDVDDrive(IN_BSTR aName, IMedium **aDrive) DUMMY(E_FAIL)
STDMETHODIMP Host::FindHostFloppyDrive(IN_BSTR aName, IMedium **aDrive) DUMMY(E_FAIL)
STDMETHODIMP Host::FindHostNetworkInterfaceByName(IN_BSTR aName, IHostNetworkInterface **networkInterface) DUMMY(E_FAIL)
STDMETHODIMP Host::FindHostNetworkInterfaceById(IN_BSTR id, IHostNetworkInterface **networkInterface) DUMMY(E_FAIL)
STDMETHODIMP Host::FindHostNetworkInterfacesOfType(HostNetworkInterfaceType_T type, ComSafeArrayOut(IHostNetworkInterface *, aNetworkInterfaces)) DUMMY(E_FAIL)
STDMETHODIMP Host::FindUSBDeviceByAddress(IN_BSTR aAddress, IHostUSBDevice **aDevice) DUMMY(E_FAIL)
STDMETHODIMP Host::FindUSBDeviceById(IN_BSTR aId, IHostUSBDevice **aDevice) DUMMY(E_FAIL)
STDMETHODIMP Host::GenerateMACAddress(BSTR *aAddress) DUMMY(E_FAIL)

HRESULT Host::findHostDriveByName(DeviceType_T mediumType,
                                  const Utf8Str &strLocationFull,
                                  bool fRefresh,
                                  ComObjPtr<Medium> &pMedium)                   DUMMY(E_FAIL)

HRESULT Host::findHostDriveById(DeviceType_T, com::Guid const&, bool,
                                ComObjPtr<Medium>&)                             TRACE(VBOX_E_OBJECT_NOT_FOUND)

HRESULT Host::saveSettings(settings::Host&)                                     TRACE(S_OK)

HRESULT Host::init(VirtualBox *aParent)                                         TRACE(S_OK)
HRESULT Host::loadSettings(const settings::Host &)                              TRACE(S_OK)
HRESULT Host::FinalConstruct()                                                  TRACE(S_OK)
void    Host::FinalRelease()                                                    DUMMY()
void    Host::uninit()                                                          DUMMY()

void Host::generateMACAddress(Utf8Str &mac)
{
	static unsigned counter = 1;

	mac = Utf8StrFmt("080027%06X", counter++);

	TRACE();
}

HRESULT Host::GetProcessorFeature(ProcessorFeature_T feature, BOOL *supported)
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

HRESULT Host::getDrives(DeviceType_T, bool, MediaList*&, util::AutoWriteLock&)  DUMMY(E_FAIL)
HRESULT Host::findHostDriveByNameOrId(DeviceType_T, const com::Utf8Str&,
                                      ComObjPtr<Medium>&)                       DUMMY(E_FAIL)
HRESULT Host::buildDVDDrivesList(MediaList &list) DUMMY(E_FAIL)
HRESULT Host::buildFloppyDrivesList(MediaList &list) DUMMY(E_FAIL)

#ifdef VBOX_WITH_USB
USBProxyService* Host::usbProxyService()
{
	TRACE(nullptr)
}

HRESULT Host::addChild(HostUSBDeviceFilter *pChild)                             DUMMY(E_FAIL)
HRESULT Host::removeChild(HostUSBDeviceFilter *pChild)                          DUMMY(E_FAIL)
VirtualBox* Host::parent()                                                      DUMMY(nullptr)

HRESULT Host::onUSBDeviceFilterChange(HostUSBDeviceFilter *, BOOL)              DUMMY(E_FAIL)

void Host::getUSBFilters(Host::USBDeviceFilterList *aGlobalFilters)             DUMMY()

HRESULT Host::checkUSBProxyService()                                            TRACE(S_OK)

#include "HostUSBDeviceImpl.h"
#include "USBDeviceFilterImpl.h"

bool HostUSBDevice::isMatch(const USBDeviceFilter::Data &aData)                 DUMMY(false)

#endif
