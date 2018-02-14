# keep 'bash/target.inc' but not 'bash/target.mk'
BASH_SRC := src/noux-pkg/bash/target.inc

include $(REP_DIR)/recipes/src/bash/content.mk

content: src/noux-pkg/bash-minimal

src/noux-pkg/bash-minimal:
	$(mirror_from_rep_dir)
