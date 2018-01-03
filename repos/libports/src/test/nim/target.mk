TARGET   = test-nim
LIBS     = nim-threads libc
SRC_NIM  = main.nim

# Enable extra system assertions
NIM_OPT += -d:useSysAssert

CC_CXX_WARN_STRICT =
