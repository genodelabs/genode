/** @file
 *
 * VirtualBox API class wrapper code for IMediumFormat.
 *
 * DO NOT EDIT! This is a generated file.
 * Generated from: src/VBox/Main/idl/VirtualBox.xidl
 * Generator: src/VBox/Main/idl/apiwrap-server.xsl
 */

/**
 * Copyright (C) 2010-2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#define LOG_GROUP_MAIN_OVERRIDE LOG_GROUP_MAIN_MEDIUMFORMAT

#include "MediumFormatWrap.h"
#include "Logging.h"

DEFINE_EMPTY_CTOR_DTOR(MediumFormatWrap)

//
// IMediumFormat properties
//

STDMETHODIMP MediumFormatWrap::COMGETTER(Id)(BSTR *aId)
{
    LogRelFlow(("{%p} %s: enter aId=%p\n", this, "MediumFormat::getId", aId));

    VirtualBoxBase::clearError();

    HRESULT hrc;

    try
    {
        CheckComArgOutPointerValidThrow(aId);

        AutoCaller autoCaller(this);
        if (FAILED(autoCaller.rc()))
            throw autoCaller.rc();

        hrc = getId(BSTROutConverter(aId).str());
    }
    catch (HRESULT hrc2)
    {
        hrc = hrc2;
    }
    catch (...)
    {
        hrc = VirtualBoxBase::handleUnexpectedExceptions(this, RT_SRC_POS);
    }

    LogRelFlow(("{%p} %s: leave *aId=%ls hrc=%Rhrc\n", this, "MediumFormat::getId", *aId, hrc));
    return hrc;
}

STDMETHODIMP MediumFormatWrap::COMGETTER(Name)(BSTR *aName)
{
    LogRelFlow(("{%p} %s: enter aName=%p\n", this, "MediumFormat::getName", aName));

    VirtualBoxBase::clearError();

    HRESULT hrc;

    try
    {
        CheckComArgOutPointerValidThrow(aName);

        AutoCaller autoCaller(this);
        if (FAILED(autoCaller.rc()))
            throw autoCaller.rc();

        hrc = getName(BSTROutConverter(aName).str());
    }
    catch (HRESULT hrc2)
    {
        hrc = hrc2;
    }
    catch (...)
    {
        hrc = VirtualBoxBase::handleUnexpectedExceptions(this, RT_SRC_POS);
    }

    LogRelFlow(("{%p} %s: leave *aName=%ls hrc=%Rhrc\n", this, "MediumFormat::getName", *aName, hrc));
    return hrc;
}

STDMETHODIMP MediumFormatWrap::COMGETTER(Capabilities)(ComSafeArrayOut(MediumFormatCapabilities_T, aCapabilities))
{
    LogRelFlow(("{%p} %s: enter aCapabilities=%p\n", this, "MediumFormat::getCapabilities", aCapabilities));

    VirtualBoxBase::clearError();

    HRESULT hrc;

    try
    {
        CheckComArgOutPointerValidThrow(aCapabilities);

        AutoCaller autoCaller(this);
        if (FAILED(autoCaller.rc()))
            throw autoCaller.rc();

        hrc = getCapabilities(ArrayOutConverter<MediumFormatCapabilities_T>(ComSafeArrayOutArg(aCapabilities)).array());
    }
    catch (HRESULT hrc2)
    {
        hrc = hrc2;
    }
    catch (...)
    {
        hrc = VirtualBoxBase::handleUnexpectedExceptions(this, RT_SRC_POS);
    }

    LogRelFlow(("{%p} %s: leave *aCapabilities=%zu hrc=%Rhrc\n", this, "MediumFormat::getCapabilities", ComSafeArraySize(*aCapabilities), hrc));
    return hrc;
}

//
// IMediumFormat methods
//

STDMETHODIMP MediumFormatWrap::DescribeFileExtensions(ComSafeArrayOut(BSTR, aExtensions),
                                                      ComSafeArrayOut(DeviceType_T, aTypes))
{
    LogRelFlow(("{%p} %s:enter aExtensions=%p aTypes=%p\n", this, "MediumFormat::describeFileExtensions", aExtensions, aTypes));

    VirtualBoxBase::clearError();

    HRESULT hrc;

    try
    {
        CheckComArgOutPointerValidThrow(aExtensions);
        CheckComArgOutPointerValidThrow(aTypes);

        AutoCaller autoCaller(this);
        if (FAILED(autoCaller.rc()))
            throw autoCaller.rc();

        hrc = describeFileExtensions(ArrayBSTROutConverter(ComSafeArrayOutArg(aExtensions)).array(),
                                     ArrayOutConverter<DeviceType_T>(ComSafeArrayOutArg(aTypes)).array());
    }
    catch (HRESULT hrc2)
    {
        hrc = hrc2;
    }
    catch (...)
    {
        hrc = VirtualBoxBase::handleUnexpectedExceptions(this, RT_SRC_POS);
    }

    LogRelFlow(("{%p} %s: leave *aExtensions=%zu *aTypes=%zu hrc=%Rhrc\n", this, "MediumFormat::describeFileExtensions", ComSafeArraySize(*aExtensions), ComSafeArraySize(*aTypes), hrc));
    return hrc;
}

STDMETHODIMP MediumFormatWrap::DescribeProperties(ComSafeArrayOut(BSTR, aNames),
                                                  ComSafeArrayOut(BSTR, aDescriptions),
                                                  ComSafeArrayOut(DataType_T, aTypes),
                                                  ComSafeArrayOut(ULONG, aFlags),
                                                  ComSafeArrayOut(BSTR, aDefaults))
{
    LogRelFlow(("{%p} %s:enter aNames=%p aDescriptions=%p aTypes=%p aFlags=%p aDefaults=%p\n", this, "MediumFormat::describeProperties", aNames, aDescriptions, aTypes, aFlags, aDefaults));

    VirtualBoxBase::clearError();

    HRESULT hrc;

    try
    {
        CheckComArgOutPointerValidThrow(aNames);
        CheckComArgOutPointerValidThrow(aDescriptions);
        CheckComArgOutPointerValidThrow(aTypes);
        CheckComArgOutPointerValidThrow(aFlags);
        CheckComArgOutPointerValidThrow(aDefaults);

        AutoCaller autoCaller(this);
        if (FAILED(autoCaller.rc()))
            throw autoCaller.rc();

        hrc = describeProperties(ArrayBSTROutConverter(ComSafeArrayOutArg(aNames)).array(),
                                 ArrayBSTROutConverter(ComSafeArrayOutArg(aDescriptions)).array(),
                                 ArrayOutConverter<DataType_T>(ComSafeArrayOutArg(aTypes)).array(),
                                 ArrayOutConverter<ULONG>(ComSafeArrayOutArg(aFlags)).array(),
                                 ArrayBSTROutConverter(ComSafeArrayOutArg(aDefaults)).array());
    }
    catch (HRESULT hrc2)
    {
        hrc = hrc2;
    }
    catch (...)
    {
        hrc = VirtualBoxBase::handleUnexpectedExceptions(this, RT_SRC_POS);
    }

    LogRelFlow(("{%p} %s: leave *aNames=%zu *aDescriptions=%zu *aTypes=%zu *aFlags=%zu *aDefaults=%zu hrc=%Rhrc\n", this, "MediumFormat::describeProperties", ComSafeArraySize(*aNames), ComSafeArraySize(*aDescriptions), ComSafeArraySize(*aTypes), ComSafeArraySize(*aFlags), ComSafeArraySize(*aDefaults), hrc));
    return hrc;
}

#ifdef VBOX_WITH_XPCOM
NS_DECL_CLASSINFO(MediumFormatWrap)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(MediumFormatWrap, IMediumFormat)
#endif // VBOX_WITH_XPCOM
