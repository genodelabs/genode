#ifndef ____H_VIRTUALBOXERRORINFO
#define ____H_VIRTUALBOXERRORINFO

#include <VirtualBoxBase.h>

class VirtualBoxErrorInfo :
	public VirtualBoxBase,
	VBOX_SCRIPTABLE_IMPL(IVirtualBoxErrorInfo)
{
	public:

		VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(VirtualBoxErrorInfo, IVirtualBoxErrorInfo)

		DECLARE_NOT_AGGREGATABLE(VirtualBoxErrorInfo)

		DECLARE_PROTECT_FINAL_CONSTRUCT()

		BEGIN_COM_MAP(VirtualBoxErrorInfo)
			COM_INTERFACE_ENTRY(IErrorInfo)
			COM_INTERFACE_ENTRY(IVirtualBoxErrorInfo)
			COM_INTERFACE_ENTRY(IDispatch)
		END_COM_MAP()

		HRESULT init(HRESULT aResultCode, const GUID &aIID,
		             const char *pcszComponent, const com::Utf8Str &strText,
		             IVirtualBoxErrorInfo *aNext = NULL);
		HRESULT FinalConstruct() { return S_OK; }

		/* readonly attribute long resultCode; */
		HRESULT GetResultCode(PRInt32 *aResultCode) { return S_OK; }

		/* readonly attribute long resultDetail; */
		HRESULT GetResultDetail(PRInt32 *aResultDetail) { return S_OK; }

		/* readonly attribute wstring interfaceID; */
		HRESULT GetInterfaceID(PRUnichar * *aInterfaceID) { return S_OK; }

		/* readonly attribute wstring component; */
		HRESULT GetComponent(PRUnichar * *aComponent) { return S_OK; }

		/* readonly attribute wstring text; */
		HRESULT GetText(PRUnichar * *aText) { return S_OK; }

		/* readonly attribute IVirtualBoxErrorInfo next; */
		HRESULT GetNext(IVirtualBoxErrorInfo * *aNext) { return S_OK; }

};

#endif /* ____H_VIRTUALBOXERRORINFO */
