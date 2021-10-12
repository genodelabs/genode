include $(REP_DIR)/lib/mk/virtualbox6-common.inc

SHARED_LIB = yes
LIBS       = stdcxx mesa

SRC_C  += Devices/Graphics/shaderlib/directx.c
SRC_C  += Devices/Graphics/shaderlib/glsl_shader.c
SRC_C  += Devices/Graphics/shaderlib/libWineStub/debug.c
SRC_C  += Devices/Graphics/shaderlib/shader.c
SRC_C  += Devices/Graphics/shaderlib/shader_sm1.c
SRC_C  += Devices/Graphics/shaderlib/shader_sm4.c
SRC_C  += Devices/Graphics/shaderlib/shaderapi.c
SRC_C  += Devices/Graphics/shaderlib/stateblock.c
SRC_C  += Devices/Graphics/shaderlib/utils.c

INC_DIR += $(VBOX_DIR)/Devices/Graphics/shaderlib/wine/include
INC_DIR += $(VIRTUALBOX_DIR)/include/VBox/Graphics

# SVGA3D/wine specific config
WINE_CC_OPT := -D__WINESRC__ -DWINE_NOWINSOCK -D_USE_MATH_DEFINES \
               -DVBOX_USING_WINDDK_W7_OR_LATER \
               -DVBOX_WINE_WITH_SINGLE_SWAPCHAIN_CONTEXT \
               -DVBOX_WINE_WITH_IPRT \
               -UVBOX_WITH_WDDM

CC_OPT_Devices/Graphics/shaderlib/directx           = $(WINE_CC_OPT)
CC_OPT_Devices/Graphics/shaderlib/glsl_shader       = $(WINE_CC_OPT)
CC_OPT_Devices/Graphics/shaderlib/libWineStub/debug = $(WINE_CC_OPT)
CC_OPT_Devices/Graphics/shaderlib/shader            = $(WINE_CC_OPT)
CC_OPT_Devices/Graphics/shaderlib/shader_sm1        = $(WINE_CC_OPT)
CC_OPT_Devices/Graphics/shaderlib/shader_sm4        = $(WINE_CC_OPT)
CC_OPT_Devices/Graphics/shaderlib/shaderapi         = $(WINE_CC_OPT)
CC_OPT_Devices/Graphics/shaderlib/stateblock        = $(WINE_CC_OPT)
CC_OPT_Devices/Graphics/shaderlib/utils             = $(WINE_CC_OPT)
