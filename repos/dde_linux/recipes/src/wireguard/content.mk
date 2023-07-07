MIRRORED_FROM_REP_DIR := \
	lib/mk/spec/arm_64/wireguard.mk \
	lib/mk/spec/arm_64/wireguard_lx_inc_dirs.mk \
	lib/mk/spec/x86_64/wireguard.mk \
	lib/mk/spec/x86_64/wireguard_lx_inc_dirs.mk \
	lib/mk/wireguard.inc \
	lib/mk/wireguard_lx_inc_dirs.inc \
	src/app/wireguard \

content: $(MIRRORED_FROM_REP_DIR)

$(MIRRORED_FROM_REP_DIR):
	$(mirror_from_rep_dir)

content: src/lib/musl_tm
src/lib/musl_tm:
	mkdir -p src/lib
	cp -r $(GENODE_DIR)/repos/libports/$@ $@

content: LICENSE
LICENSE:
	( echo "GNU General Public License version 2, see:"; \
	  echo "https://www.kernel.org/pub/linux/kernel/COPYING" ) > $@
