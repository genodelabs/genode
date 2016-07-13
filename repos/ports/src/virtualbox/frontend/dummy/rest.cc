#include "dummy/macros.h"

static bool debug = false;


/* ApplianceImplExport.cpp */

#include "MachineImpl.h"

HRESULT Machine::ExportTo(IAppliance *aAppliance, IN_BSTR location,
                          IVirtualSystemDescription **aDescription)             DUMMY(E_FAIL)


/* com.cpp */

#include "VBox/com/Guid.h"

const com::Guid com::Guid::Empty;


/* DisplayImpl.cpp */

#include "DisplayImpl.h"

void fireGuestMonitorChangedEvent(IEventSource* aSource,
                                  GuestMonitorChangedEventType_T a_changeType,
                                  ULONG a_screenId,
                                  ULONG a_originX,
                                  ULONG a_originY,
                                  ULONG a_width,
                                  ULONG a_height)                               TRACE()


/* DisplayPNGUtil.cpp */

#include "DisplayImpl.h"

int DisplayMakePNG(uint8_t *, uint32_t, uint32_t, uint8_t **, uint32_t *,
                   uint32_t *, uint32_t *, uint8_t)                             DUMMY(-1)


/* ErrorInfo.cpp */

#include "VBox/com/ErrorInfo.h"

com::ProgressErrorInfo::ProgressErrorInfo(IProgress*)                           DUMMY()


/* EventImpl.cpp */

#include "EventImpl.h"

HRESULT VBoxEventDesc::init(IEventSource* aSource, VBoxEventType_T aType, ...)  TRACE(S_OK)
HRESULT VBoxEventDesc::reinit(VBoxEventType_T aType, ...)                       TRACE(S_OK)


/* GuestCtrlImpl.cpp */

#include "GuestImpl.h"

STDMETHODIMP Guest::CreateSession(IN_BSTR, IN_BSTR, IN_BSTR, IN_BSTR,
                                  IGuestSession **)                             DUMMY(E_FAIL)
STDMETHODIMP Guest::FindSession(IN_BSTR,
                                ComSafeArrayOut(IGuestSession *, aSessions))    DUMMY(E_FAIL)
STDMETHODIMP Guest::UpdateGuestAdditions(IN_BSTR,
                                         ComSafeArrayIn(IN_BSTR, aArguments),
                                         ComSafeArrayIn(AdditionsUpdateFlag_T, aFlags),
                                         IProgress **aProgress)                 DUMMY(E_FAIL)


/* initterm.cpp */

#include "VBox/com/com.h"

HRESULT com::Initialize(bool fGui)                                              TRACE(S_OK)
HRESULT com::Shutdown() DUMMY(E_FAIL)


/* MachineImpl.cpp */

#include "MachineImpl.h"

void fireHostPCIDevicePlugEvent(IEventSource* aSource,
                                CBSTR a_machineId,
                                BOOL a_plugged,
                                BOOL a_success,
                                IPCIDeviceAttachment* a_attachment,
                                CBSTR a_message)                                TRACE()

/* NATNetworkImpl.cpp */

#include "NetworkServiceRunner.h"
#include "NATNetworkImpl.h"

NATNetwork::NATNetwork() : mVirtualBox(nullptr)                                                  DUMMY()
NATNetwork::~NATNetwork()                                                                        DUMMY()
STDMETHODIMP NATNetwork::COMGETTER(EventSource)(IEventSource **IEventSource)                     DUMMY(E_FAIL)
STDMETHODIMP NATNetwork::COMGETTER(Enabled)(BOOL *aEnabled)                                      DUMMY(E_FAIL)
STDMETHODIMP NATNetwork::COMSETTER(Enabled)(BOOL aEnabled)                                       DUMMY(E_FAIL)
STDMETHODIMP NATNetwork::COMGETTER(NetworkName)(BSTR *aName)                                     DUMMY(E_FAIL)
STDMETHODIMP NATNetwork::COMSETTER(NetworkName)(IN_BSTR aName)                                   DUMMY(E_FAIL)
STDMETHODIMP NATNetwork::COMGETTER(Gateway)(BSTR *aIPGateway)                                    DUMMY(E_FAIL)
STDMETHODIMP NATNetwork::COMGETTER(Network)(BSTR *aIPNetwork)                                    DUMMY(E_FAIL)
STDMETHODIMP NATNetwork::COMSETTER(Network)(IN_BSTR aIPNetwork)                                  DUMMY(E_FAIL)
STDMETHODIMP NATNetwork::COMGETTER(IPv6Enabled)(BOOL *aEnabled)                                  DUMMY(E_FAIL)
STDMETHODIMP NATNetwork::COMSETTER(IPv6Enabled)(BOOL aEnabled)                                   DUMMY(E_FAIL)
STDMETHODIMP NATNetwork::COMGETTER(IPv6Prefix)(BSTR *aName)                                      DUMMY(E_FAIL)
STDMETHODIMP NATNetwork::COMSETTER(IPv6Prefix)(IN_BSTR aName)                                    DUMMY(E_FAIL)
STDMETHODIMP NATNetwork::COMGETTER(AdvertiseDefaultIPv6RouteEnabled)(BOOL *aEnabled)             DUMMY(E_FAIL)
STDMETHODIMP NATNetwork::COMSETTER(AdvertiseDefaultIPv6RouteEnabled)(BOOL aEnabled)              DUMMY(E_FAIL)
STDMETHODIMP NATNetwork::COMGETTER(NeedDhcpServer)(BOOL *aEnabled)                               DUMMY(E_FAIL)
STDMETHODIMP NATNetwork::COMSETTER(NeedDhcpServer)(BOOL aEnabled)                                DUMMY(E_FAIL)
STDMETHODIMP NATNetwork::COMGETTER(LocalMappings)(ComSafeArrayOut(BSTR, aLocalMappings))         DUMMY(E_FAIL)
STDMETHODIMP NATNetwork::AddLocalMapping(IN_BSTR aHostId, LONG aOffset)                          DUMMY(E_FAIL)
STDMETHODIMP NATNetwork::COMGETTER(LoopbackIp6)(LONG *aLoopbackIp6)                              DUMMY(E_FAIL)
STDMETHODIMP NATNetwork::COMSETTER(LoopbackIp6)(LONG aLoopbackIp6)                               DUMMY(E_FAIL)
STDMETHODIMP NATNetwork::COMGETTER(PortForwardRules4)(ComSafeArrayOut(BSTR, aPortForwardRules4)) DUMMY(E_FAIL)
STDMETHODIMP NATNetwork::COMGETTER(PortForwardRules6)(ComSafeArrayOut(BSTR, aPortForwardRules6)) DUMMY(E_FAIL)
STDMETHODIMP NATNetwork::AddPortForwardRule(BOOL aIsIpv6,
                                  IN_BSTR aPortForwardRuleName,
                                  NATProtocol_T aProto,
                                  IN_BSTR aHostIp,
                                  USHORT aHostPort,
                                  IN_BSTR aGuestIp,
                                  USHORT aGuestPort)                                             DUMMY(E_FAIL)
STDMETHODIMP NATNetwork::RemovePortForwardRule(BOOL aIsIpv6, IN_BSTR aPortForwardRuleName)       DUMMY(E_FAIL)
STDMETHODIMP NATNetwork::Start(IN_BSTR aTrunkType)                                               DUMMY(E_FAIL)
STDMETHODIMP NATNetwork::Stop()                                                                  DUMMY(E_FAIL)
HRESULT NATNetwork::init(VirtualBox *aVirtualBox, const settings::NATNetwork &)                  DUMMY(E_FAIL)
HRESULT NATNetwork::FinalConstruct()                                                             DUMMY(E_FAIL)
void    NATNetwork::uninit()                                                                     DUMMY()


/* ProgressProxyImpl.cpp */

#include "ProgressProxyImpl.h"

STDMETHODIMP ProgressProxy::Cancel()                                            DUMMY(E_FAIL)
void ProgressProxy::clearOtherProgressObjectInternal(bool fEarly)               DUMMY()
STDMETHODIMP ProgressProxy::COMGETTER(Cancelable)(BOOL *)                       DUMMY(E_FAIL)
STDMETHODIMP ProgressProxy::COMGETTER(Percent)(ULONG *)                         DUMMY(E_FAIL)
STDMETHODIMP ProgressProxy::COMGETTER(TimeRemaining)(LONG *)                    DUMMY(E_FAIL)
STDMETHODIMP ProgressProxy::COMGETTER(Completed)(BOOL *)                        DUMMY(E_FAIL)
STDMETHODIMP ProgressProxy::COMGETTER(Canceled)(BOOL *)                         DUMMY(E_FAIL)
STDMETHODIMP ProgressProxy::COMGETTER(ResultCode)(LONG *)                       DUMMY(E_FAIL)
STDMETHODIMP ProgressProxy::COMGETTER(ErrorInfo)(IVirtualBoxErrorInfo **)       DUMMY(E_FAIL)
STDMETHODIMP ProgressProxy::COMGETTER(Operation)(ULONG *)                       DUMMY(E_FAIL)
STDMETHODIMP ProgressProxy::COMGETTER(OperationDescription)(BSTR *)             DUMMY(E_FAIL)
STDMETHODIMP ProgressProxy::COMGETTER(OperationPercent)(ULONG *)                DUMMY(E_FAIL)
STDMETHODIMP ProgressProxy::COMSETTER(Timeout)(ULONG)                           DUMMY(E_FAIL)
STDMETHODIMP ProgressProxy::COMGETTER(Timeout)(ULONG *)                         DUMMY(E_FAIL)
void ProgressProxy::copyProgressInfo(IProgress *pOtherProgress, bool fEarly)    DUMMY()
HRESULT ProgressProxy::FinalConstruct()                                         DUMMY(E_FAIL)
void ProgressProxy::FinalRelease()                                              DUMMY()
HRESULT ProgressProxy::init(VirtualBox*, IUnknown*, unsigned short const*,
        bool)                                                                   DUMMY(E_FAIL)
HRESULT ProgressProxy::init(VirtualBox*, void*, unsigned short const*, bool,
                            unsigned int, unsigned short const*, unsigned int,
                            unsigned int)                                       DUMMY(E_FAIL)
HRESULT ProgressProxy::notifyComplete(HRESULT)                                  DUMMY(E_FAIL)
HRESULT ProgressProxy::notifyComplete(HRESULT, GUID const&, char const*,
                                      char const*, ...)                         DUMMY(E_FAIL)
STDMETHODIMP ProgressProxy::SetCurrentOperationProgress(ULONG aPercent)         DUMMY(E_FAIL)
STDMETHODIMP ProgressProxy::SetNextOperation(IN_BSTR, ULONG)                    DUMMY(E_FAIL)
bool    ProgressProxy::setOtherProgressObject(IProgress*)                       DUMMY(false)
void ProgressProxy::uninit()                                                    DUMMY()
STDMETHODIMP ProgressProxy::WaitForCompletion(LONG aTimeout)                    DUMMY(E_FAIL)
STDMETHODIMP ProgressProxy::WaitForOperationCompletion(ULONG, LONG)             DUMMY(E_FAIL)


/* SharedFolderImpl.cpp */

#include "SharedFolderImpl.h"

HRESULT SharedFolder::init(Console*, com::Utf8Str const&, com::Utf8Str const&,
                           bool, bool, bool)                                    DUMMY(E_FAIL)


/* USBFilter.cpp */

#include "VBox/usbfilter.h"

USBFILTERMATCH USBFilterGetMatchingMethod(PCUSBFILTER, USBFILTERIDX)            DUMMY(USBFILTERMATCH_INVALID)
int  USBFilterGetNum(PCUSBFILTER pFilter, USBFILTERIDX enmFieldIdx)             DUMMY(-1)
const char * USBFilterGetString(PCUSBFILTER pFilter, USBFILTERIDX enmFieldIdx)  DUMMY(nullptr)
void USBFilterInit(PUSBFILTER pFilter, USBFILTERTYPE enmType)                   DUMMY()
bool USBFilterIsMethodNumeric(USBFILTERMATCH enmMatchingMethod)                 DUMMY(false)
bool USBFilterIsMethodString(USBFILTERMATCH enmMatchingMethod)                  DUMMY(false)
bool USBFilterIsNumericField(USBFILTERIDX enmFieldIdx)                          DUMMY(false)
bool USBFilterIsStringField(USBFILTERIDX enmFieldIdx)                           DUMMY(false)
bool USBFilterMatch(PCUSBFILTER pFilter, PCUSBFILTER pDevice)                   DUMMY(false)
int  USBFilterSetIgnore(PUSBFILTER pFilter, USBFILTERIDX enmFieldIdx)           DUMMY(-1)
int  USBFilterSetNumExact(PUSBFILTER, USBFILTERIDX, uint16_t, bool)             DUMMY(-1)
int  USBFilterSetNumExpression(PUSBFILTER, USBFILTERIDX, const char *, bool)    DUMMY(-1)
int  USBFilterSetStringExact(PUSBFILTER, USBFILTERIDX, const char *, bool)      DUMMY(-1)
int  USBFilterSetStringPattern(PUSBFILTER, USBFILTERIDX, const char *, bool)    DUMMY(-1)


/* USBProxyService.cpp */

#include "USBProxyService.h"

HRESULT USBProxyService::autoCaptureDevicesForVM(SessionMachine *)              DUMMY(E_FAIL)
HRESULT USBProxyService::captureDeviceForVM(SessionMachine *, IN_GUID)          DUMMY(E_FAIL)
HRESULT USBProxyService::detachAllDevicesFromVM(SessionMachine*, bool, bool)    DUMMY(E_FAIL)
HRESULT USBProxyService::detachDeviceFromVM(SessionMachine*, IN_GUID, bool)     DUMMY(E_FAIL)


/* VirtualBoxImpl.cpp */

#include "VirtualBoxImpl.h"

void fireNATRedirectEvent(IEventSource* aSource,
                          CBSTR a_machineId,
                          ULONG a_slot,
                          BOOL a_remove,
                          CBSTR a_name,
                          NATProtocol_T a_proto,
                          CBSTR a_hostIP,
                          LONG a_hostPort,
                          CBSTR a_guestIP,
                          LONG a_guestPort)                                     TRACE()

void fireNATNetworkChangedEvent(IEventSource* aSource,
                                CBSTR a_networkName)                            TRACE()

void fireNATNetworkStartStopEvent(IEventSource* aSource,
                                  CBSTR a_networkName,
                                  BOOL a_startEvent)                            TRACE()

void fireNATNetworkSettingEvent(IEventSource* aSource,
                                CBSTR a_networkName,
                                BOOL a_enabled,
                                CBSTR a_network,
                                CBSTR a_gateway,
                                BOOL a_advertiseDefaultIPv6RouteEnabled,
                                BOOL a_needDhcpServer)                          TRACE()

void fireNATNetworkPortForwardEvent(IEventSource* aSource,
                                    CBSTR a_networkName,
                                    BOOL a_create,
                                    BOOL a_ipv6,
                                    CBSTR a_name,
                                    NATProtocol_T a_proto,
                                    CBSTR a_hostIp,
                                    LONG a_hostPort,
                                    CBSTR a_guestIp,
                                    LONG a_guestPort)                           TRACE()

void fireHostNameResolutionConfigurationChangeEvent(IEventSource* aSource)      TRACE()
