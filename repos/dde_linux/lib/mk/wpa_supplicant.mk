WS_CONTRIB_DIR := $(call select_from_ports,dde_linux)/src/app/wpa_supplicant
WS_DIR         := $(REP_DIR)/src/lib/wpa_supplicant

LIBS += libc libcrypto libssl wpa_driver_nl80211

SHARED_LIB = yes
LD_OPT += --version-script=$(WS_DIR)/symbol.map

CC_OPT += -Wno-unused-function

CC_CXX_OPT += -fpermissive

SRC_C  += main.c
SRC_CC += reporter.cc

# wpa_supplicant
SRC_C_wpa_supplicant = blacklist.c      \
                       bss.c            \
                       config.c         \
                       config_file.c    \
                       eap_register.c   \
                       events.c         \
                       notify.c         \
                       scan.c           \
                       sme.c            \
                       wpa_supplicant.c \
                       wpas_glue.c
SRC_C   += $(addprefix wpa_supplicant/, $(SRC_C_wpa_supplicant))
INC_DIR += $(WS_CONTRIB_DIR)/wpa_supplicant
CC_OPT  += -DCONFIG_BACKEND_FILE -DCONFIG_NO_CONFIG_WRITE \
           -DCONFIG_SME

INC_DIR += $(WS_CONTRIB_DIR)/src/

# common
SRC_C_common = ieee802_11_common.c wpa_common.c
SRC_C += $(addprefix src/common/, $(SRC_C_common))
INC_DIR += $(WS_CONTRIB_DIR)/src/common

# crypto
SRC_C_crypto = aes-unwrap.c     \
               crypto_openssl.c \
               md5.c            \
               ms_funcs.c       \
               random.c         \
               sha1-prf.c       \
               sha1-tlsprf.c    \
               tls_openssl.c
SRC_C += $(addprefix src/crypto/, $(SRC_C_crypto))
INC_DIR += $(WS_CONTRIB_DIR)/src/crypto

SRC_C += src/drivers/driver_common.c

# eap_common
SRC_C_eap_common = chap.c            \
                   eap_common.c      \
                   eap_peap_common.c
SRC_C += $(addprefix src/eap_common/, $(SRC_C_eap_common))
INC_DIR += $(WS_CONTRIB_DIR)/src/eap_common

# eap_peer
SRC_C_eap_peer = eap.c            \
                 eap_gtc.c        \
                 eap_leap.c       \
                 eap_md5.c        \
                 eap_methods.c    \
                 eap_mschapv2.c   \
                 eap_otp.c        \
                 eap_peap.c       \
                 eap_tls.c        \
                 eap_tls_common.c \
                 eap_ttls.c       \
                 mschapv2.c
SRC_C   += $(addprefix src/eap_peer/, $(SRC_C_eap_peer))
INC_DIR += $(WS_CONTRIB_DIR)/src/eap_peer
CC_OPT  += -DEAP_TLS -DEAP_PEAP -DEAP_TTLS -DEAP_MD5 -DEAP_MSCHAPv2 \
           -DEAP_OTP -DEAP_LEAP

# eapol_supp
SRC_C   += src/eapol_supp/eapol_supp_sm.c
INC_DIR += $(WS_CONTRIB_DIR)/src/eapol_supp
CC_OPT  += -DIEEE8021X_EAPOL

# rsn_supp
SRC_C_rsn_supp = peerkey.c     \
                 pmksa_cache.c \
                 preauth.c     \
                 wpa.c         \
                 wpa_ie.c
SRC_C   += $(addprefix src/rsn_supp/, $(SRC_C_rsn_supp))
INC_DIR += $(WS_CONTRIB_DIR)/src/rsn_supp
CC_OPT  += -DCONFIG_PEERKEY

# utils
SRC_C_utils = base64.c    \
              common.c    \
              eloop.c     \
              os_unix.c   \
              radiotap.c  \
              wpa_debug.c \
              wpabuf.c
SRC_C += $(addprefix src/utils/, $(SRC_C_utils))
INC_DIR += $(WS_CONTRIB_DIR)/src/utils
CC_OPT  += -DCONFIG_ELOOP_POLL

vpath %.c  $(WS_CONTRIB_DIR)
vpath %.c  $(WS_DIR)
vpath %.cc $(WS_DIR)
