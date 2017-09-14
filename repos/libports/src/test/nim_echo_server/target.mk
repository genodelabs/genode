TARGET   = test-nim_echo_server
LIBS = libc
SRC_NIM = main.nim

# Enable extra system assertions
NIM_OPT += -d:useSysAssert
