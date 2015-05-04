/** @file
 *
 * VirtualBox API class wrapper header for IMediumFormat.
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

#ifndef MediumFormatWrap_H_
#define MediumFormatWrap_H_

#include "VirtualBoxBase.h"
#include "Wrapper.h"

class ATL_NO_VTABLE MediumFormatWrap:
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IMediumFormat)
{
    Q_OBJECT

public:
    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(MediumFormatWrap, IMediumFormat)
    DECLARE_NOT_AGGREGATABLE(MediumFormatWrap)
    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(MediumFormatWrap)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IMediumFormat)
        COM_INTERFACE_ENTRY2(IDispatch, IMediumFormat)
    END_COM_MAP()

    DECLARE_EMPTY_CTOR_DTOR(MediumFormatWrap)

    // public IMediumFormat properties
    STDMETHOD(COMGETTER(Id))(BSTR *aId);
    STDMETHOD(COMGETTER(Name))(BSTR *aName);
    STDMETHOD(COMGETTER(Capabilities))(ComSafeArrayOut(MediumFormatCapabilities_T, aCapabilities));

    // public IMediumFormat methods
    STDMETHOD(DescribeFileExtensions)(ComSafeArrayOut(BSTR, aExtensions),
                                      ComSafeArrayOut(DeviceType_T, aTypes));
    STDMETHOD(DescribeProperties)(ComSafeArrayOut(BSTR, aNames),
                                  ComSafeArrayOut(BSTR, aDescriptions),
                                  ComSafeArrayOut(DataType_T, aTypes),
                                  ComSafeArrayOut(ULONG, aFlags),
                                  ComSafeArrayOut(BSTR, aDefaults));

private:
    // wrapped IMediumFormat properties
    virtual HRESULT getId(com::Utf8Str &aId) = 0;
    virtual HRESULT getName(com::Utf8Str &aName) = 0;
    virtual HRESULT getCapabilities(std::vector<MediumFormatCapabilities_T> &aCapabilities) = 0;

    // wrapped IMediumFormat methods
    virtual HRESULT describeFileExtensions(std::vector<com::Utf8Str> &aExtensions,
                                           std::vector<DeviceType_T> &aTypes) = 0;
    virtual HRESULT describeProperties(std::vector<com::Utf8Str> &aNames,
                                       std::vector<com::Utf8Str> &aDescriptions,
                                       std::vector<DataType_T> &aTypes,
                                       std::vector<ULONG> &aFlags,
                                       std::vector<com::Utf8Str> &aDefaults) = 0;
};

#endif // !MediumFormatWrap_H_
