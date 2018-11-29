REQUIRES += 64bit

INC_DIR += $(call select_from_repositories,include/solo5)
INC_DIR += $(call select_from_ports,solo5)/include/solo5
