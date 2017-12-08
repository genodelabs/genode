# keep 'coreutils/target.inc' but not 'coreutils/target.mk'
COREUTILS_SRC := src/noux-pkg/coreutils/target.inc

include $(REP_DIR)/recipes/src/coreutils/content.mk

content: src/noux-pkg/coreutils-minimal

src/noux-pkg/coreutils-minimal:
	$(mirror_from_rep_dir)
