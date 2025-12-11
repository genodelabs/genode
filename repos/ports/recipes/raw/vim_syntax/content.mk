content: vim_syntax.tar

SYNTAX_DIR   := share/vim/vim73/syntax
SYNTAX_FILES := genode_log.vim hid.vim

include $(GENODE_DIR)/repos/base/recipes/content.inc

vim_syntax.tar:
	mkdir -p $(SYNTAX_DIR)
	cp $(addprefix $(GENODE_DIR)/tool/vim/,$(SYNTAX_FILES)) $(SYNTAX_DIR)
	$(TAR) -cf $@ $(addprefix $(SYNTAX_DIR)/,$(SYNTAX_FILES))
	rm -r share
