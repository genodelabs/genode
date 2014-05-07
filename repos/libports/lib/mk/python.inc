PYTHON     = python-2.6.4
PYTHON_DIR = $(REP_DIR)/contrib/$(PYTHON)
LIBS      += libc libm
SHARED_LIB = yes

# use our custom 'pyconfig.h' file
INC_DIR += $(REP_DIR)/include/python

# Python headres
INC_DIR += $(PYTHON_DIR)/Include

D_OPTS  = Py_BUILD_CORE NDBEUG PREFIX='""' EXEC_PREFIX='"lib"' VERSION='"2.6"'
F_OPTS  = no-strict-aliasing wrapv
D_OPTS := $(addprefix -D,$(D_OPTS))
F_OPTS := $(addprefix -f,$(F_OPTS))
CC_DEF += $(F_OPTS) $(D_OPTS)

#        python.c
#
SRC_C = \
        dynload_shlib.c \
        dup.c \
        getbuildinfo.c \
        acceler.c \
        grammar1.c \
        listnode.c \
        node.c \
        parser.c \
        parsetok.c \
        bitset.c \
        metagrammar.c \
        firstsets.c \
        grammar.c \
        pgen.c \
        myreadline.c \
        tokenizer.c \
        abstract.c \
        boolobject.c \
        bufferobject.c \
        bytes_methods.c \
        bytearrayobject.c \
        cellobject.c \
        classobject.c \
        cobject.c \
        codeobject.c \
        complexobject.c \
        descrobject.c \
        enumobject.c \
        exceptions.c \
        genobject.c \
        fileobject.c \
        floatobject.c \
        frameobject.c \
        funcobject.c \
        intobject.c \
        iterobject.c \
        listobject.c \
        longobject.c \
        dictobject.c \
        methodobject.c \
        moduleobject.c \
        object.c \
        obmalloc.c \
        rangeobject.c \
        setobject.c \
        sliceobject.c \
        stringobject.c \
        structseq.c \
        tupleobject.c \
        typeobject.c \
        weakrefobject.c \
        unicodeobject.c \
        unicodectype.c \
        _warnings.c \
        Python-ast.c \
        asdl.c \
        ast.c \
        bltinmodule.c \
        ceval.c \
        compile.c \
        codecs.c \
        errors.c \
        frozen.c \
        frozenmain.c \
        future.c \
        getargs.c \
        getcompiler.c \
        getcopyright.c \
        getmtime.c \
        getplatform.c \
        getversion.c \
        graminit.c \
        import.c \
        importdl.c \
        marshal.c \
        modsupport.c \
        mystrtoul.c \
        mysnprintf.c \
        peephole.c \
        pyarena.c \
        pyfpe.c \
        pymath.c \
        pystate.c \
        pythonrun.c \
        structmember.c \
        symtable.c \
        sysmodule.c \
        traceback.c \
        getopt.c \
        pystrcmp.c \
        pystrtod.c \
        formatter_unicode.c \
        formatter_string.c \
        config.c \
        getpath.c \
        main.c \
        gcmodule.c \
        signalmodule.c \
        posixmodule.c \
        errnomodule.c \
        pwdmodule.c \
        _sre.c \
        _codecsmodule.c \
        zipimport.c \
        symtablemodule.c \
        xxsubtype.c

CC_C_OPT = -Wno-implicit-function-declaration \
           -Wno-int-to-pointer-cast \
           -Wno-unused-but-set-variable \
           -Wno-unused-function \
           -Wno-unused-variable

vpath %.c $(PYTHON_DIR)/Modules
vpath %.c $(PYTHON_DIR)/Objects
vpath %.c $(PYTHON_DIR)/Parser
vpath %.c $(PYTHON_DIR)/Python
vpath %.c $(REP_DIR)/src/lib/python
vpath %.cc $(REP_DIR)/src/lib/python

