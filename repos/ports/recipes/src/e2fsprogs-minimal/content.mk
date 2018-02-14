# keep 'e2fsprogs/target.inc' but not 'e2fsprogs/target.mk'
E2FSPROGS_SRC := src/noux-pkg/e2fsprogs/target.inc

include $(REP_DIR)/recipes/src/e2fsprogs/content.mk

content: src/noux-pkg/e2fsprogs-minimal

src/noux-pkg/e2fsprogs-minimal:
	$(mirror_from_rep_dir)
