#include "VirtualBoxBase.h"

class MediumFormatWrap : public VirtualBoxBase, public DummyClass<MediumFormatWrap> {

	public:
		virtual const char* getComponentName() const
		{
			return "MediumFormatWrap";
		}
};
