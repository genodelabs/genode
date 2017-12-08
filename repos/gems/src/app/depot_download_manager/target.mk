TARGET := depot_download_manager
SRC_CC := main.cc gen_depot_query.cc gen_fetchurl.cc gen_verify.cc \
          gen_chroot.cc gen_extract.cc
LIBS   += base
