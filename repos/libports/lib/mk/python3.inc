PYTHON_PORT_DIR := $(call select_from_ports,python3)
PYTHON_DIR      := $(PYTHON_PORT_DIR)/src/lib/python3
LIBS            += libc libm
SHARED_LIB       = yes

# use our custom 'pyconfig.h' file
INC_DIR += $(REP_DIR)/include/python3

# Python headres
INC_DIR += $(PYTHON_DIR)/Include

# zlib headers
INC_DIR += $(PYTHON_DIR)/Modules/zlib

# expat headers
INC_DIR += $(PYTHON_DIR)/Modules/expat

D_OPTS  = Py_BUILD_CORE NDBEUG PREFIX='""' VPATH='"."' EXEC_PREFIX='"lib"' PYTHONPATH='""' HAVE_EXPAT_CONFIG_H='""' VERSION='"3"'
F_OPTS  = no-strict-aliasing wrapv
D_OPTS := $(addprefix -D,$(D_OPTS))
F_OPTS := $(addprefix -f,$(F_OPTS))
CC_DEF += $(F_OPTS) $(D_OPTS)

#        python.c
#
SRC_C = \
        dynload_shlib.c \
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
        bytes_methods.c \
        bytearrayobject.c \
		  bytesobject.c \
		  capsule.c \
        cellobject.c \
        classobject.c \
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
		  faulthandler.c \
		  fileutils.c \
        frozen.c \
        frozenmain.c \
        future.c \
        getargs.c \
        getcompiler.c \
        getcopyright.c \
        getplatform.c \
        getversion.c \
        graminit.c \
		  hashtable.c \
        import.c \
        importdl.c \
        marshal.c \
		  _math.c \
		  memoryobject.c \
        modsupport.c \
        mystrtoul.c \
        mysnprintf.c \
		  namespaceobject.c \
		  odictobject.c \
        peephole.c \
        pyarena.c \
		  pyctype.c \
        pyfpe.c \
		  pyhash.c \
		  pylifecycle.c \
        pymath.c \
        pystate.c \
		  pystrhex.c \
		  pytime.c \
        pythonrun.c \
		  random.c \
        structmember.c \
        symtable.c \
        sysmodule.c \
        traceback.c \
		  _tracemalloc.c \
        getopt.c \
        pystrcmp.c \
        pystrtod.c \
        formatter_unicode.c \
        config.c \
        getpath.c \
        main.c \
        gcmodule.c \
        signalmodule.c \
        posixmodule.c \
        errnomodule.c \
        pwdmodule.c \
		  md5module.c \
		  sha1module.c \
		  sha256module.c \
		  sha512module.c \
		  _blake2/blake2module.c \
		  _blake2/blake2b_impl.c \
		  _blake2/blake2s_impl.c \
		  _sha3/sha3module.c \
        _sre.c \
        _codecsmodule.c \
        timemodule.c \
        _operator.c \
        _randommodule.c \
        itertoolsmodule.c \
        _functoolsmodule.c \
        _collectionsmodule.c \
		  _weakref.c \
		  _io/iobase.c \
		  _io/fileio.c \
		  _io/bufferedio.c \
		  _io/stringio.c \
		  _io/textio.c \
		  _io/bytesio.c \
		  _io/winconsoleio.c \
		  _io/_iomodule.c \
		  _localemodule.c \
		  atexitmodule.c \
		  pyexpat.c \
		  expat/xmlparse.c \
		  expat/xmlrole.c \
		  expat/xmltok.c \
		  _struct.c \
		  accu.c \
        zipimport.c \
        zlibmodule.c \
        zlib/adler32.c \
        zlib/compress.c \
        zlib/crc32.c \
        zlib/deflate.c \
        zlib/gzlib.c \
        zlib/gzread.c \
        zlib/gzwrite.c \
        zlib/infback.c \
        zlib/inffast.c \
        zlib/inflate.c \
        zlib/inftrees.c \
        zlib/trees.c \
        zlib/uncompr.c \
        zlib/zutil.c \
        mathmodule.c \
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
vpath %.c $(REP_DIR)/src/lib/python3
vpath %.cc $(REP_DIR)/src/lib/python3

