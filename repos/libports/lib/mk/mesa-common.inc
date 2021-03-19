MESA_PORT_DIR := $(call select_from_ports,mesa)
MESA_SRC_DIR  := $(MESA_PORT_DIR)/src/lib/mesa
MESA_GEN_DIR  := $(MESA_PORT_DIR)/generated
LIB_DIR       := $(REP_DIR)/src/lib/mesa

INC_DIR += $(LIB_DIR)/include \
           $(MESA_PORT_DIR)/include \
           $(MESA_SRC_DIR)/src/gallium/include \
           $(MESA_SRC_DIR)/src \
           $(MESA_GEN_DIR)/src

CC_OPT += -D__GENODE__ -DGALLIUM_SOFTPIPE

#CC_OPT += -DENABLE_SHADER_CACHE
CC_OPT += -DENABLE_ST_OMX_BELLAGIO=0  \
          -DENABLE_ST_OMX_TIZONIA=0  \
          -DHAVE___BUILTIN_BSWAP32  \
          -DHAVE___BUILTIN_BSWAP64  \
          -DHAVE___BUILTIN_CLZ  \
          -DHAVE___BUILTIN_CLZLL  \
          -DHAVE___BUILTIN_CTZ  \
          -DHAVE___BUILTIN_EXPECT  \
          -DHAVE___BUILTIN_FFS  \
          -DHAVE___BUILTIN_FFSLL  \
          -DHAVE___BUILTIN_POPCOUNT  \
          -DHAVE___BUILTIN_POPCOUNTLL  \
          -DHAVE___BUILTIN_TYPES_COMPATIBLE_P  \
          -DHAVE___BUILTIN_UNREACHABLE  \
          -DHAVE_CET_H  \
          -DHAVE_DIRENT_D_TYPE  \
          -DHAVE_DLADDR  \
          -DHAVE_DLFCN_H  \
          -DHAVE_DL_ITERATE_PHDR  \
          -DHAVE_EXECINFO_H  \
          -DHAVE_FLOCK  \
          -DHAVE_FUNC_ATTRIBUTE_ALIAS  \
          -DHAVE_FUNC_ATTRIBUTE_CONST  \
          -DHAVE_FUNC_ATTRIBUTE_FLATTEN  \
          -DHAVE_FUNC_ATTRIBUTE_FORMAT  \
          -DHAVE_FUNC_ATTRIBUTE_MALLOC  \
          -DHAVE_FUNC_ATTRIBUTE_NORETURN  \
          -DHAVE_FUNC_ATTRIBUTE_PACKED  \
          -DHAVE_FUNC_ATTRIBUTE_PURE  \
          -DHAVE_FUNC_ATTRIBUTE_RETURNS_NONNULL  \
          -DHAVE_FUNC_ATTRIBUTE_UNUSED  \
          -DHAVE_FUNC_ATTRIBUTE_VISIBILITY  \
          -DHAVE_FUNC_ATTRIBUTE_WARN_UNUSED_RESULT  \
          -DHAVE_FUNC_ATTRIBUTE_WEAK  \
          -DHAVE_GETRANDOM  \
          -DHAVE_MKOSTEMP  \
          -DHAVE_PIPE_LOADER_DRI \
          -DHAVE_POSIX_MEMALIGN  \
          -DHAVE_PTHREAD  \
          -DHAVE_PTHREAD_NP_H \
          -DHAVE_RANDOM_R  \
          -DHAVE_STRTOF  \
          -DHAVE_STRTOK_R  \
          -DHAVE_TIMESPEC_GET  \
          -DHAVE_ZLIB  \
          -DPACKAGE_BUGREPORT='"https://gitlab.freedesktop.org/mesa/mesa/-/issues"' \
          -DPACKAGE_VERSION='"21.0.0"' \
          -D__STDC_CONSTANT_MACROS  \
          -D__STDC_FORMAT_MACROS  \
          -D__STDC_LIMIT_MACROS \
          -DUSE_GCC_ATOMIC_BUILTINS  \


#-DUSE_X86_64_ASM

# vi: set ft=make :
