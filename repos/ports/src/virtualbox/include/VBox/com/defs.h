#ifndef ___VBox_com_defs_h
#define ___VBox_com_defs_h

#include <iprt/types.h>

#include <xpcom/nsISupports.h>
#include <nsID.h>

#define ATL_NO_VTABLE
#define DECLARE_CLASSFACTORY()
#define DECLARE_CLASSFACTORY_SINGLETON(X)
#define DECLARE_REGISTRY_RESOURCEID(X)
#define DECLARE_NOT_AGGREGATABLE(X)
#define DECLARE_PROTECT_FINAL_CONSTRUCT(Y)
#define BEGIN_COM_MAP(X)
#define COM_INTERFACE_ENTRY(X)
#define COM_INTERFACE_ENTRY2(X,Y)
#define COM_INTERFACE_ENTRY_AGGREGATE(X, Y)
#define END_COM_MAP()

#define HRESULT nsresult
#define SUCCEEDED(X) ((X) == VINF_SUCCESS)
#define FAILED(X) ((X) != VINF_SUCCESS)

#define FAILED_DEAD_INTERFACE(rc)  (   (rc) == NS_ERROR_ABORT \
                                    || (rc) == NS_ERROR_CALL_FAILED \
                                   )

#define IUnknown nsISupports

typedef       PRBool               BOOL;
typedef       PRUint8              BYTE;
typedef       PRInt16              SHORT;
typedef       PRUint16             USHORT;
typedef       PRInt32              LONG;
typedef       PRUint32             ULONG;
typedef       PRInt64              LONG64;
typedef       PRUint64             ULONG64;

#define       FALSE                false
#define       TRUE                 true

typedef       wchar_t              OLECHAR;
typedef       PRUnichar           *BSTR;
typedef const PRUnichar           *CBSTR;
typedef       CBSTR                IN_BSTR;

#define       GUID                 nsID
#define       IN_GUID              const nsID &
#define       OUT_GUID             nsID **

#define COMGETTER(n)    Get##n
#define COMSETTER(n)    Set##n

#define ComSafeArrayIn(aType, aArg)         unsigned aArg##Size, aType *aArg
#define ComSafeArrayInIsNull(aArg)          ((aArg) == NULL)
#define ComSafeArrayInArg(aArg)             aArg##Size, aArg

#define ComSafeArrayOut(aType, aArg)        PRUint32 *aArg##Size, aType **aArg
#define ComSafeArrayOutIsNull(aArg)         ((aArg) == NULL)
#define ComSafeArrayOutArg(aArg)            aArg##Size, aArg

#define ComSafeGUIDArrayIn(aArg)            PRUint32 aArg##Size, const nsID **aArg
#define ComSafeGUIDArrayInArg(aArg)         ComSafeArrayInArg(aArg)

#define ComSafeArraySize(aArg)  ((aArg) == NULL ? 0 : (aArg##Size))

/* OLE error codes */
#define S_OK                ((nsresult)NS_OK)
#define E_UNEXPECTED        NS_ERROR_UNEXPECTED
#define E_NOTIMPL           NS_ERROR_NOT_IMPLEMENTED
#define E_OUTOFMEMORY       NS_ERROR_OUT_OF_MEMORY
#define E_INVALIDARG        NS_ERROR_INVALID_ARG
#define E_NOINTERFACE       NS_ERROR_NO_INTERFACE
#define E_POINTER           NS_ERROR_NULL_POINTER
#define E_ABORT             NS_ERROR_ABORT
#define E_FAIL              NS_ERROR_FAILURE
/* Note: a better analog for E_ACCESSDENIED would probably be
 * NS_ERROR_NOT_AVAILABLE, but we want binary compatibility for now. */
#define E_ACCESSDENIED      ((nsresult)0x80070005L)

#define STDMETHOD(X) virtual HRESULT X
#define STDMETHODIMP HRESULT

inline GUID& stuffstuff() {
	static GUID stuff;
	return stuff;
}
#define COM_IIDOF(X) stuffstuff()

#define COM_STRUCT_OR_CLASS(I) class I

extern "C"
{
	BSTR SysAllocString(const OLECHAR* sz);
	BSTR SysAllocStringByteLen(char *psz, unsigned int len);
	BSTR SysAllocStringLen(const OLECHAR *pch, unsigned int cch);
	void SysFreeString(BSTR bstr);
	unsigned int SysStringByteLen(BSTR bstr);
	unsigned int SysStringLen(BSTR bstr);
}

namespace com {

#define VBOX_SCRIPTABLE_IMPL(iface)                     \
    public iface

#define VBOX_DEFAULT_INTERFACE_ENTRIES(X)

} /* namespace com */

#endif /* !___VBox_com_defs_h */
