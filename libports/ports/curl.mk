include ports/curl.inc

CURL_TGZ      = $(CURL).tar.gz
CURL_SIG      = $(CURL_TGZ).asc
CURL_BASE_URL = http://curl.haxx.se/download
CURL_URL      = $(CURL_BASE_URL)/$(CURL_TGZ)
CURL_URL_SIG  = $(CURL_BASE_URL)/$(CURL_SIG)
CURL_KEY      = daniel@haxx.se

#
# Interface to top-level prepare Makefile
#
PORTS += $(CURL)

prepare-curl: $(CONTRIB_DIR)/$(CURL) include/curl

$(CONTRIB_DIR)/$(CURL): clean-curl

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(CURL_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(CURL_URL) && touch $@

$(DOWNLOAD_DIR)/$(CURL_SIG):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(CURL_URL_SIG) && touch $@

$(DOWNLOAD_DIR)/$(CURL_TGZ).verified: $(DOWNLOAD_DIR)/$(CURL_TGZ) $(DOWNLOAD_DIR)/$(CURL_SIG)
	$(VERBOSE)$(SIGVERIFIER) $(DOWNLOAD_DIR)/$(CURL_TGZ) $(DOWNLOAD_DIR)/$(CURL_SIG) $(CURL_KEY)
	$(VERBOSE)touch $@

$(CONTRIB_DIR)/$(CURL): $(DOWNLOAD_DIR)/$(CURL_TGZ).verified
	$(VERBOSE)tar xfz $(<:.verified=) -C $(CONTRIB_DIR) && touch $@
	$(VERBOSE)find ./src/lib/curl/ -name "*.patch" |\
		xargs -ixxx sh -c "patch -p1 -r - -N -d $(CONTRIB_DIR)/$(CURL) < xxx" || true

include/curl:
	$(VERBOSE)mkdir -p $@
	$(VERBOSE)for f in $(shell find $(CONTRIB_DIR)/$(CURL)/include/curl -name *.h); do \
		ln -sf ../../$$f $@; done
	@# This header is located in src/lib/curl/$(TARGET_ARCH)/curl
	$(VERBOSE)rm -rf $@/curlbuild.h

clean-curl:
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(CURL)
	$(VERBOSE)rm -rf include/curl
