#
# All dynamic executables must be linked to the component entry-point library
# (a trampoline for component startup from ldso), so, enforce the library
# dependency here.
#
LIBS += component_entry_point
