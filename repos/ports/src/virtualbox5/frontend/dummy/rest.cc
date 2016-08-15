#include "dummy/macros.h"

static bool debug = false;


/* ApplianceImplExport.cpp */

#include "MachineImpl.h"

HRESULT Machine::exportTo(const ComPtr<IAppliance> &aAppliance,
                          const com::Utf8Str &aLocation,
                          ComPtr<IVirtualSystemDescription> &aDescription)      DUMMY(E_FAIL)

/* com.cpp */

#include "VBox/com/Guid.h"

const com::Guid com::Guid::Empty;


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


/* initterm.cpp */

#include "VBox/com/com.h"

HRESULT com::Initialize(bool fGui)                                              TRACE(S_OK)
HRESULT com::Shutdown() DUMMY(E_FAIL)


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
int  USBFilterSetStringExact(PUSBFILTER, USBFILTERIDX, const char *, bool,
                             bool)                                              DUMMY(-1)


/* USBProxyService.cpp */

#include "USBProxyService.h"

HRESULT USBProxyService::autoCaptureDevicesForVM(SessionMachine *)              DUMMY(E_FAIL)
HRESULT USBProxyService::captureDeviceForVM(SessionMachine *, IN_GUID,
                                            com::Utf8Str const&)                DUMMY(E_FAIL)
HRESULT USBProxyService::detachAllDevicesFromVM(SessionMachine*, bool, bool)    DUMMY(E_FAIL)
HRESULT USBProxyService::detachDeviceFromVM(SessionMachine*, IN_GUID, bool)     DUMMY(E_FAIL)
