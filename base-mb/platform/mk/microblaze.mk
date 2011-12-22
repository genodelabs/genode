#
# \brief  Upload an image to the supported Microblaze SoC's
# \author Martin Stein
# \date   2011-05-23
#

UPLOAD_XMD = $(WORK_DIR)/upload.xmd

upload: $(UPLOAD_XMD)
	$(VERBOSE) xmd -opt $(UPLOAD_XMD)

$(UPLOAD_XMD):
	$(VERBOSE)echo \
		"connect mb mdm"\
		"\ndow $(IMAGE)"\
	> $(UPLOAD_XMD)

.INTERMEDIATE: $(UPLOAD_XMD)
.PHONY: upload
