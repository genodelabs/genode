CONTENT  = index.html
CONTENT += example.pem lighttpd.conf
CONTENT += dynamic.config

content: $(CONTENT)

$(CONTENT):
	cp $(REP_DIR)/recipes/raw/test-wg_reconnect/$@ $@
