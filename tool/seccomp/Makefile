.DEFAULT_GOAL := seccomp_bpf_filters

seccomp_bpf_filters: seccomp_bpf_policy_x86_32.bin seccomp_bpf_policy_x86_64.bin seccomp_bpf_policy_arm.bin

seccomp_bpf_policy_%.bin: seccomp_bpf_compiler_%.prg
	./$< > $@

seccomp_bpf_compiler_%.prg: seccomp_bpf_compiler_%.cc seccomp_bpf_compiler.h
	@g++ $< -o $@ -lseccomp

clean:
	@rm seccomp_bpf_policy_*.bin 2> /dev/null; true
	@rm seccomp_bpf_compiler_*.prg 2> /dev/null; true

.PHONY: seccomp_bpf_filters
