/** @file
 *
 * VirtualBox API class wrapper header for IToken.
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

#ifndef TokenWrap_H_
#define TokenWrap_H_

#include "VirtualBoxBase.h"
#include "Wrapper.h"

class ATL_NO_VTABLE TokenWrap:
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IToken)
{
    Q_OBJECT

public:
    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(TokenWrap, IToken)
    DECLARE_NOT_AGGREGATABLE(TokenWrap)
    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(TokenWrap)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IToken)
        COM_INTERFACE_ENTRY2(IDispatch, IToken)
    END_COM_MAP()

    DECLARE_EMPTY_CTOR_DTOR(TokenWrap)

    // public IToken properties

    // public IToken methods
    STDMETHOD(Abandon)();
    STDMETHOD(Dummy)();

private:
    // wrapped IToken properties

    // wrapped IToken methods
    virtual HRESULT abandon(AutoCaller &aAutoCaller) = 0;
    virtual HRESULT dummy() = 0;
};

#endif // !TokenWrap_H_
