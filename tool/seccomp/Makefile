.DEFAULT_GOAL := seccomp_bpf_filters

ARCHS := x86_32 x86_64 arm_32 arm_64

seccomp_bpf_filters: $(foreach A,$(ARCHS),seccomp_bpf_policy_$A.bin)

seccomp_bpf_policy_%.bin: seccomp_bpf_compiler_%.prg
	./$< > $@

seccomp_bpf_compiler_%.prg: seccomp_bpf_compiler_%.cc seccomp_bpf_compiler.h
	@g++ $< -o $@ -lseccomp

clean:
	@rm seccomp_bpf_policy_*.bin 2> /dev/null; true
	@rm seccomp_bpf_compiler_*.prg 2> /dev/null; true

.PHONY: seccomp_bpf_filters
