TARGET   =  loader
INC_DIR += $(PRG_DIR)
LIBS     =  cxx env thread server process signal timed_semaphore
SRC_CC   =  main.cc \
            loader_session_component.cc \
            rom_session_component.cc \
            nitpicker_session_component.cc \
            input_session_component.cc
