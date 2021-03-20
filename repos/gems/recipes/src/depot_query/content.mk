SRC_DIR := src/app/depot_query

include $(GENODE_DIR)/repos/base/recipes/src/content.inc

MIRROR_FROM_REP_DIR := include/depot include/gems/lru_cache.h \
                       src/app/fs_query/for_each_subdir_name.h \
                       src/app/fs_query/sorted_for_each.h

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)
