#include "VirtualBoxBase.h"

class Token : public VirtualBoxBase
{
	public:
		virtual HRESULT abandon(AutoCaller &aAutoCaller) = 0;

		HRESULT Abandon() {
			AutoCaller autoCaller(this);
			abandon(autoCaller);
			return S_OK;
		}

	virtual const char* getComponentName() const
	{
		return "Token";
	}
};

class TokenWrap : public Token
{
};
