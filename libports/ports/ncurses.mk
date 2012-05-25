NCURSES     := ncurses-5.9
NCURSES_TGZ := $(NCURSES).tar.gz
NCURSES_URL := http://ftp.gnu.org/pub/gnu/ncurses/$(NCURSES_TGZ)

#
# Interface to top-level prepare Makefile
#
PORTS += $(NCURSES)

#
# Check for tools
#
$(call check_tool,sed)
$(call check_tool,mawk)

NCURSES_SYMLINKED_INC := nc_alloc.h nc_panel.h nc_tparm.h term_entry.h \
                         tic.h hashed_db.h capdefaults.c
NCURSES_GENERATED_INC := curses.h ncurses_def.h ncurses_dll.h term.h \
                         unctrl.h termcap.h parametrized.h hashsize.h \
                         init_keytry.h keys.list make_keys MKterm.h.awk

NCURSES_GENERATED_SRC := names.c unctrl.c fallback.c comp_captab.c codes.c \
                         make_hash make_keys

NCURSES_GEN_SYMLINKS  := $(addprefix include/ncurses/,$(NCURSES_SYMLINKED_INC))

NCURSES_GEN_FILES     := $(addprefix include/ncurses/,$(NCURSES_GENERATED_INC)) \
                         $(addprefix src/lib/ncurses/,$(NCURSES_GENERATED_SRC))

prepare-ncurses: $(NCURSES_GEN_SYMLINKS) $(NCURSES_GEN_FILES)

$(CONTRIB_DIR)/$(NCURSES): clean-ncurses

$(NCURSES_GEN_SYMLINKS) $(NCURSES_GEN_FILES): $(CONTRIB_DIR)/$(NCURSES) src/lib/ncurses

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(NCURSES_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(NCURSES_URL) && touch $@

$(CONTRIB_DIR)/$(NCURSES): $(DOWNLOAD_DIR)/$(NCURSES_TGZ)
	$(VERBOSE)tar xfz $< -C $(CONTRIB_DIR) && touch $@

src/lib/ncurses:
	$(VERBOSE)mkdir -p $@

#
# Create symlinks to ncurses contrib dir
#
$(NCURSES_GEN_SYMLINKS):
	$(VERBOSE)for i in $(NCURSES_SYMLINKED_INC); do \
		ln -sf ../../$(CONTRIB_DIR)/$(NCURSES)/include/$$i include/ncurses/$$i; done

#
# Produce generated includes
#

NCURSES_SUBST := \
	"@NCURSES_MAJOR@/5" \
	"@NCURSES_MINOR@/9" \
	"@NCURSES_PATCH@/20110404" \
	"@NCURSES_MOUSE_VERSION@/1" \
	"@NCURSES_CONST@/\/*nothing*\/" \
	"@NCURSES_INLINE@/inline" \
	"@NCURSES_OPAQUE@/0" \
	"@NCURSES_INTEROP_FUNCS@/0" \
	"@NCURSES_SIZE_T@/short" \
	"@NCURSES_TPARM_VARARGS@/1" \
	"@NCURSES_CH_T@/chtype" \
	"@NCURSES_LIBUTF8@/0" \
	"@NCURSES_OSPEED@/short" \
	"@NCURSES_WCHAR_T@/0" \
	"@NCURSES_WINT_T@/0" \
	"@NCURSES_SBOOL@/char" \
	"@NCURSES_XNAMES@/1" \
	"@HAVE_TERMIOS_H@/1" \
	"@HAVE_TCGETATTR@/1" \
	"@NCURSES_CCHARW_MAX@/5" \
	"@NCURSES_EXT_COLORS@/0" \
	"@NCURSES_EXT_FUNCS@/1" \
	"@NCURSES_SP_FUNCS@/0" \
	"@NCURSES_OK_WCHAR_T@/" \
	"@NCURSES_WRAP_PREFIX@/_nc_" \
	"@cf_cv_header_stdbool_h@/1" \
	"@cf_cv_enable_opaque@/NCURSES_OPAQUE" \
	"@cf_cv_enable_reentrant@/0" \
	"@cf_cv_enable_lp64@/0" \
	"@cf_cv_typeof_chtype@/long" \
	"@cf_cv_typeof_mmask_t@/long" \
	"@cf_cv_type_of_bool@/unsigned char" \
	"@cf_cv_1UL@/1UL" \
	"@USE_CXX_BOOL@/defined(__cplusplus)" \
	"@BROKEN_LINKER@/0" \
	"@NEED_WCHAR_H@/0" \
	"@GENERATED_EXT_FUNCS@/generated" \
	"@HAVE_TERMIO_H@/1" \
	"@HAVE_VSSCANF@/1"

NCURSES_INCLUDE_DIR := $(CONTRIB_DIR)/$(NCURSES)/include

NCURSES_SRC_DIR := $(CONTRIB_DIR)/$(NCURSES)/ncurses

apply_substitutions = $(VERBOSE)for i in $(NCURSES_SUBST); do sed -i "s/$$i/g" $(1); done

#
# Generate files in 'include/ncurses/'
#

include/ncurses/curses.h:
	$(VERBOSE)cp $(CONTRIB_DIR)/$(NCURSES)/include/curses.h.in $@
	$(call apply_substitutions,$@)
	$(VERBOSE)AWK=mawk sh $(NCURSES_INCLUDE_DIR)/MKkey_defs.sh $(NCURSES_INCLUDE_DIR)/Caps >> $@
	$(VERBOSE)cat $(NCURSES_INCLUDE_DIR)/curses.tail >> $@

include/ncurses/ncurses_def.h:
	$(VERBOSE)AWK=mawk sh $(NCURSES_INCLUDE_DIR)/MKncurses_def.sh $(NCURSES_INCLUDE_DIR)/ncurses_defs > $@

include/ncurses/parametrized.h:
	$(VERBOSE)AWK=mawk sh $(NCURSES_INCLUDE_DIR)/MKparametrized.sh $(NCURSES_INCLUDE_DIR)/Caps > $@

include/ncurses/hashsize.h: $(NCURSES_INCLUDE_DIR)/MKhashsize.sh
	$(VERBOSE)AWK=mawk sh $< $(NCURSES_INCLUDE_DIR)/Caps > $@

include/ncurses/keys.list:
	$(VERBOSE)AWK=mawk sh $(NCURSES_SRC_DIR)/tinfo/MKkeys_list.sh $(NCURSES_INCLUDE_DIR)/Caps | sort > $@

include/ncurses/init_keytry.h: src/lib/ncurses/make_keys include/ncurses/keys.list
	$(VERBOSE)src/lib/ncurses/make_keys include/ncurses/keys.list > $@

include/ncurses/term.h: include/ncurses/MKterm.h.awk
	$(VERBOSE)mawk -f $< $(NCURSES_INCLUDE_DIR)/Caps > $@

include/ncurses/MKterm.h.awk: $(NCURSES_INCLUDE_DIR)/MKterm.h.awk.in
	$(VERBOSE)cp $< $@
	$(call apply_substitutions,$@)

include/ncurses/ncurses_dll.h: $(NCURSES_INCLUDE_DIR)/ncurses_dll.h.in
	$(VERBOSE)cp $< $@
	$(call apply_substitutions,$@)

include/ncurses/termcap.h: $(NCURSES_INCLUDE_DIR)/termcap.h.in
	$(VERBOSE)cp $< $@
	$(call apply_substitutions,$@)

include/ncurses/unctrl.h: $(NCURSES_INCLUDE_DIR)/unctrl.h.in
	$(VERBOSE)cp $< $@
	$(call apply_substitutions,$@)

#
# Generate files in 'src/lib/ncurses/'
#

src/lib/ncurses/names.c:
	$(VERBOSE)mawk -f $(NCURSES_SRC_DIR)/tinfo/MKnames.awk bigstrings=1 $(NCURSES_INCLUDE_DIR)/Caps > $@

src/lib/ncurses/codes.c:
	$(VERBOSE)mawk -f $(NCURSES_SRC_DIR)/tinfo/MKcodes.awk bigstrings=1 $(NCURSES_INCLUDE_DIR)/Caps > $@

src/lib/ncurses/fallback.c: $(NCURSES_SRC_DIR)/tinfo/MKfallback.sh
	$(VERBOSE)sh -e $< x $(CONTRIB_DIR)/$(NCURSES)/misc/terminfo.src tic linux > $@
	$(VERBOSE)#sh -e $< /usr/share/terminfo $(NCURSES_SRC_DIR)/misc/terminfo.src /usr/bin/tic > $@

src/lib/ncurses/unctrl.c:
	$(VERBOSE)echo | mawk -f $(NCURSES_SRC_DIR)/base/MKunctrl.awk bigstrings=1 > $@

src/lib/ncurses/comp_captab.c: src/lib/ncurses/make_hash
	$(VERBOSE)cd $(dir $@); sh -e $(realpath $(NCURSES_SRC_DIR))/tinfo/MKcaptab.sh mawk 1 $(realpath $(NCURSES_SRC_DIR))/tinfo/MKcaptab.awk $(realpath $(NCURSES_INCLUDE_DIR))/Caps > $(notdir $@)

src/lib/ncurses/make_keys: $(NCURSES_SRC_DIR)/tinfo/make_keys.c
	$(VERBOSE)$(CC) -o $@ -DHAVE_CONFIG_H -Iinclude/ncurses -Isrc/lib/ncurses -I$(NCURSES_SRC_DIR) $<

src/lib/ncurses/make_hash: $(NCURSES_SRC_DIR)/tinfo/make_hash.c
	$(VERBOSE)$(CC) -o $@ -DHAVE_CONFIG_H -Iinclude/ncurses -Isrc/lib/ncurses -I$(NCURSES_SRC_DIR) $<

src/lib/ncurses/make_keys: src/lib/ncurses/names.c


#
# Clean rules
#

clean-ncurses: clean_ncurses_symlinks clean_ncurses_gen_files
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(NCURSES)

clean_ncurses_symlinks:
	$(VERBOSE)rm -f $(NCURSES_GEN_SYMLINKS)

clean_ncurses_gen_files:
	$(VERBOSE)rm -f $(NCURSES_GEN_FILES)

