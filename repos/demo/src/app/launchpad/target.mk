TARGET    = launchpad
LIBS      = launchpad scout_widgets
SRC_CC    = launchpad_window.cc \
            launcher.cc \
            main.cc

SCOUT_DIR = $(REP_DIR)/src/app/scout

INC_DIR   = $(PRG_DIR) $(SCOUT_DIR)
