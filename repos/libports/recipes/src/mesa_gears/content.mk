MIRROR_FROM_GEARS := src/test/mesa_demo/gears
MIRROR_FROM_EGLUT := src/test/mesa_demo/eglut

content: $(MIRROR_FROM_GEARS) $(MIRROR_FROM_EGLUT) LICENSE

$(MIRROR_FROM_GEARS):
	$(mirror_from_rep_dir)

$(MIRROR_FROM_EGLUT):
	$(mirror_from_rep_dir)

LICENSE:
	mv $(MIRROR_FROM_GEARS)/LICENSE $@
