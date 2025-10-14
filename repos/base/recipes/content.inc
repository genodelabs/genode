#
# Utilities for archive content creation
#

# reproducible GNU tar archives
TAR_OPT := --format=gnu --sort=name --owner=1 --group=1 --numeric-owner --mode="go=" --mtime="2025-09-29 00:00Z"
TAR     := tar $(TAR_OPT)

# mirror selective file tree
#
# Usage:
#
#   $(call mirror_file_list,FROM,TO,LIST)
#
#   FROM, TO   root paths to mirror from resp. mirror to (uses tar -C <path>)
#   LIST       list of subpaths to mirror (e.g., dir1/file dir2/file)
mirror_file_list = tar -C $1 -cf - $3 | tar -C $2 -xf -
