#
# \brief  Build configs that are specific to base-hw/x86_64 on the Muen SK
# \author Reto Buerki
# \date   2015-04-14
#

# denote wich specs are also fullfilled by this spec
SPECS += hw_x86_64 muen

# include implied specs
include $(call select_from_repositories,mk/spec/hw_x86_64.mk)
