include $(call select_from_repositories,src/app/qt5/tmpl/target_defaults.inc)

include $(call select_from_repositories,src/app/qt5/tmpl/target_final.inc)

LIBS += qt5_qpluginwidget qt5_network qoost

TEST_PLUGIN_TAR = $(BUILD_BASE_DIR)/bin/test-plugin.tar

$(TARGET): $(TEST_PLUGIN_TAR)

$(TEST_PLUGIN_TAR): config.plugin
	$(VERBOSE)tar cf $@ -C $(PRG_DIR) config.plugin

clean:
	$(VERBOSE)rm $(TEST_PLUGIN_TAR)

CC_CXX_WARN_STRICT =
