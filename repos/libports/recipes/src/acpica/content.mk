PORT_DIR := $(call port_dir,$(REP_DIR)/ports/acpica)

content: src/lib/acpica src/app/acpica lib/mk/acpica.mk \
         lib/import/import-acpica.mk include/acpica LICENSE

src/lib/acpica:
	$(mirror_from_rep_dir)
	cp -r $(PORT_DIR)/src/lib/acpica/* $@

src/app/acpica include/acpica lib/mk/acpica.mk lib/import/import-acpica.mk:
	$(mirror_from_rep_dir)

LICENSE:
	( echo "GNU General Public License version 2,"; \
	  echo "see https://www.gnu.org/licenses/gpl-2.0.html" ) > $@

