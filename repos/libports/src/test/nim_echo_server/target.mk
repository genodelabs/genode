TARGET   = test-nim_echo_server
LIBS = libc libc_resolv
SRC_NIM = main.nim

# Enable extra system assertions
NIM_OPT += -d:useSysAssert
