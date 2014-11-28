#include <base/printf.h>

#include "VirtualBoxBase.h"
#include "ClientToken.h"
#include "TokenImpl.h"
#include "ProgressProxyImpl.h"
#include "SharedFolderImpl.h"

static bool debug = false;

#define TRACE(X) \
	{ \
		if (debug) \
			PDBG(" called (%s) - eip=%p", __FILE__, \
			     __builtin_return_address(0)); \
		return X; \
	}

#define DUMMY(X) \
	{ \
		PERR("%s called (%s:%u), not implemented, eip=%p", __func__, \
		      __FILE__, __LINE__, \
		     __builtin_return_address(0)); \
		while (1) \
			asm volatile ("ud2a"); \
		\
		return X; \
	}

#define DUMMY_STATIC(X) \
	{ \
		static X dummy; \
		PERR("%s called (%s), not implemented, eip=%p", __func__, __FILE__, \
		     __builtin_return_address(0)); \
		while (1) \
			asm volatile ("ud2a"); \
		\
		return dummy; \
	}

/* static */
const Guid Guid::Empty;

HRESULT IMediumFormat::get_Capabilities(unsigned int*,
                                        MediumFormatCapabilities_T**)           DUMMY(E_FAIL)

HRESULT Machine::ExportTo(IAppliance *aAppliance, IN_BSTR location,
                          IVirtualSystemDescription **aDescription)             DUMMY(E_FAIL)

int DisplayMakePNG(uint8_t *, uint32_t, uint32_t, uint8_t **, uint32_t *,
                   uint32_t *, uint32_t *, uint8_t)                             DUMMY(-1)

ProgressErrorInfo::ProgressErrorInfo(Progress*) DUMMY()

HRESULT ProgressProxy::init(VirtualBox*, IUnknown*, unsigned short const*,
        bool)                                                                   DUMMY(E_FAIL)
HRESULT ProgressProxy::init(VirtualBox*, void*, unsigned short const*, bool,
                            unsigned int, unsigned short const*, unsigned int,
                            unsigned int)                                       DUMMY(E_FAIL)
HRESULT ProgressProxy::notifyComplete(HRESULT)                                  DUMMY(E_FAIL)
HRESULT ProgressProxy::notifyComplete(HRESULT, GUID const&, char const*,
                                      char const*, ...)                         DUMMY(E_FAIL)
bool    ProgressProxy::setOtherProgressObject(Progress*)                        DUMMY(false)
HRESULT ProgressProxy::FinalConstruct()                                         DUMMY(E_FAIL)
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

STDMETHODIMP ProgressProxy::WaitForCompletion(LONG aTimeout)                    DUMMY(E_FAIL)
STDMETHODIMP ProgressProxy::WaitForOperationCompletion(ULONG, LONG)             DUMMY(E_FAIL)
STDMETHODIMP ProgressProxy::Cancel()                                            DUMMY(E_FAIL)
STDMETHODIMP ProgressProxy::SetCurrentOperationProgress(ULONG aPercent)         DUMMY(E_FAIL)
STDMETHODIMP ProgressProxy::SetNextOperation(IN_BSTR, ULONG)                    DUMMY(E_FAIL)

void ProgressProxy::clearOtherProgressObjectInternal(bool fEarly)               DUMMY()
void ProgressProxy::copyProgressInfo(IProgress *pOtherProgress, bool fEarly)    DUMMY()
void ProgressProxy::uninit()                                                    DUMMY()
void ProgressProxy::FinalRelease()                                              DUMMY()

template<>
void DummyClass<VirtualBox>::fireNATRedirectEvent(ComObjPtr<EventSource> const&,
                                                  unsigned short*,
                                                  unsigned int&, bool&,
                                                  unsigned short*&,
                                                  NATProtocol_T&,
                                                  unsigned short*&,
                                                  unsigned short&,
                                                  unsigned short*&,
                                                   unsigned short&)             DUMMY()

template<>
void DummyClass<Console>::fireStateChangedEvent(ComObjPtr<EventSource> const&,
                                                MachineState_T)                 TRACE()
template<>
void DummyClass<Console>::fireRuntimeErrorEvent(ComObjPtr<EventSource> const&,
                                                bool aFatal, IN_BSTR aErrorID,
                                                IN_BSTR aMessage)
{
	PERR("%s : %u %s %s", __func__, aFatal,
	     Utf8Str(aErrorID).c_str(), Utf8Str(aMessage).c_str());

	TRACE();
}

template<>
void DummyClass<Machine>::fireHostPCIDevicePlugEvent(ComPtr<EventSource>&,
                                                     unsigned short*, bool,
                                                     bool, ComObjPtr<PCIDeviceAttachment>&,
                                                     void*)                     DUMMY()

NATNetwork::NATNetwork() : mVirtualBox(nullptr)                                 DUMMY()
void    NATNetwork::uninit()                                                    DUMMY()
HRESULT NATNetwork::init(VirtualBox *aVirtualBox, IN_BSTR aName)                DUMMY(E_FAIL)
HRESULT NATNetwork::init(VirtualBox *aVirtualBox, const settings::NATNetwork &) DUMMY(E_FAIL)
HRESULT NATNetwork::FinalConstruct() DUMMY(E_FAIL)
HRESULT NATNetwork::saveSettings(settings::NATNetwork &data)                    DUMMY(E_FAIL)

STDMETHODIMP NATNetwork::COMGETTER(EventSource)(IEventSource **IEventSource)    DUMMY(E_FAIL)
STDMETHODIMP NATNetwork::COMGETTER(Enabled)(BOOL *aEnabled) DUMMY(E_FAIL)
STDMETHODIMP NATNetwork::COMSETTER(Enabled)(BOOL aEnabled) DUMMY(E_FAIL)

STDMETHODIMP NATNetwork::COMGETTER(NetworkName)(BSTR *aName) DUMMY(E_FAIL)
STDMETHODIMP NATNetwork::COMSETTER(NetworkName)(IN_BSTR aName) DUMMY(E_FAIL)

STDMETHODIMP NATNetwork::COMGETTER(Gateway)(BSTR *aIPGateway) DUMMY(E_FAIL)

STDMETHODIMP NATNetwork::COMGETTER(Network)(BSTR *aIPNetwork) DUMMY(E_FAIL)
STDMETHODIMP NATNetwork::COMSETTER(Network)(IN_BSTR aIPNetwork) DUMMY(E_FAIL)

STDMETHODIMP NATNetwork::COMGETTER(IPv6Enabled)(BOOL *aEnabled) DUMMY(E_FAIL)
STDMETHODIMP NATNetwork::COMSETTER(IPv6Enabled)(BOOL aEnabled) DUMMY(E_FAIL)

STDMETHODIMP NATNetwork::COMGETTER(IPv6Prefix)(BSTR *aName) DUMMY(E_FAIL)
STDMETHODIMP NATNetwork::COMSETTER(IPv6Prefix)(IN_BSTR aName) DUMMY(E_FAIL)

STDMETHODIMP NATNetwork::COMGETTER(AdvertiseDefaultIPv6RouteEnabled)(BOOL *aEnabled) DUMMY(E_FAIL)
STDMETHODIMP NATNetwork::COMSETTER(AdvertiseDefaultIPv6RouteEnabled)(BOOL aEnabled) DUMMY(E_FAIL)

STDMETHODIMP NATNetwork::COMGETTER(NeedDhcpServer)(BOOL *aEnabled) DUMMY(E_FAIL)
STDMETHODIMP NATNetwork::COMSETTER(NeedDhcpServer)(BOOL aEnabled) DUMMY(E_FAIL)

STDMETHODIMP NATNetwork::COMGETTER(LocalMappings)(ComSafeArrayOut(BSTR, aLocalMappings)) DUMMY(E_FAIL)
STDMETHODIMP NATNetwork::AddLocalMapping(IN_BSTR aHostId, LONG aOffset) DUMMY(E_FAIL)

STDMETHODIMP NATNetwork::COMGETTER(LoopbackIp6)(LONG *aLoopbackIp6) DUMMY(E_FAIL)
STDMETHODIMP NATNetwork::COMSETTER(LoopbackIp6)(LONG aLoopbackIp6) DUMMY(E_FAIL)

STDMETHODIMP NATNetwork::COMGETTER(PortForwardRules4)(ComSafeArrayOut(BSTR, aPortForwardRules4)) DUMMY(E_FAIL)
STDMETHODIMP NATNetwork::COMGETTER(PortForwardRules6)(ComSafeArrayOut(BSTR, aPortForwardRules6)) DUMMY(E_FAIL)

STDMETHODIMP NATNetwork::AddPortForwardRule(BOOL aIsIpv6,
                                  IN_BSTR aPortForwardRuleName,
                                  NATProtocol_T aProto,
                                  IN_BSTR aHostIp,
                                  USHORT aHostPort,
                                  IN_BSTR aGuestIp,
                                  USHORT aGuestPort) DUMMY(E_FAIL)
STDMETHODIMP NATNetwork::RemovePortForwardRule(BOOL aIsIpv6, IN_BSTR aPortForwardRuleName) DUMMY(E_FAIL)

STDMETHODIMP NATNetwork::Start(IN_BSTR aTrunkType) DUMMY(E_FAIL)
STDMETHODIMP NATNetwork::Stop() DUMMY(E_FAIL)


HRESULT com::Shutdown() DUMMY(E_FAIL)

void Display::fireGuestMonitorChangedEvent(EventSource*, GuestMonitorChangedEventType, int, int, int, int, int) DUMMY()

STDMETHODIMP Guest::UpdateGuestAdditions(IN_BSTR,
                                         ComSafeArrayIn(IN_BSTR, aArguments),
                                         ComSafeArrayIn(AdditionsUpdateFlag_T, aFlags),
                                         IProgress **aProgress)                 DUMMY(E_FAIL)
STDMETHODIMP Guest::FindSession(IN_BSTR,
                                ComSafeArrayOut(IGuestSession *, aSessions))    DUMMY(E_FAIL)
STDMETHODIMP Guest::CreateSession(IN_BSTR, IN_BSTR, IN_BSTR, IN_BSTR,
                                  IGuestSession **)                             DUMMY(E_FAIL)

void ConsoleVRDPServer::SendUpdate(unsigned int, void*, unsigned int) const     DUMMY()
void ConsoleVRDPServer::SendUpdateBitmap(unsigned int, unsigned int,
                                         unsigned int, unsigned int,
                                         unsigned int) const                    DUMMY()
void ConsoleVRDPServer::Stop()                                                  DUMMY()

void IStateChangedEvent::get_State(MachineState_T*)                             DUMMY()

HRESULT IEventListener::HandleEvent(IEvent*)                                    DUMMY(E_FAIL)
HRESULT SharedFolder::init(Console*, com::Utf8Str const&, com::Utf8Str const&,
                           bool, bool, bool)                                    DUMMY(E_FAIL)

Machine::ClientToken::ClientToken(const ComObjPtr<Machine> &, SessionMachine *) TRACE()
bool Machine::ClientToken::isReady()                                            TRACE(true)
void Machine::ClientToken::getId(Utf8Str &strId)
{
	strId = "ClientTokenID DUMMY";
	TRACE()
}

HRESULT com::Initialize(bool fGui)                                              TRACE(S_OK)

