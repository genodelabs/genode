#!/bin/bash

# Build and update seccomp Berkeley Packet Filter binary
# for use in base-linux

SCRIPT_FILE=$(realpath "$0")
SCRIPT_DIR=$(dirname $SCRIPT_FILE)

make -C $SCRIPT_DIR/../../../../../tool/seccomp \
&& cp $SCRIPT_DIR/../../../../../tool/seccomp/seccomp_bpf_policy_x86_32.bin $SCRIPT_DIR/spec/x86_32/seccomp_bpf_policy.bin\
&& cp $SCRIPT_DIR/../../../../../tool/seccomp/seccomp_bpf_policy_x86_64.bin $SCRIPT_DIR/spec/x86_64/seccomp_bpf_policy.bin\
&& cp $SCRIPT_DIR/../../../../../tool/seccomp/seccomp_bpf_policy_arm.bin $SCRIPT_DIR/spec/arm/seccomp_bpf_policy.bin \
&& make -C $SCRIPT_DIR/../../../../../tool/seccomp clean \
|| exit $?

