#
# \brief  Check validity of user input
# \author Norman Feske
# \date   2014-05-27
#

ifeq ($(ARGUMENT),)
$(TARGET): missing_argument
missing_argument: usage
	@$(ECHO) "Error: Missing port name as argument"; false
endif

ifneq ($(words $(ARGUMENT)),1)
$(TARGET): too_many_arguments
too_many_arguments: usage
	@$(ECHO) "Error: Too many arguments specified, expecting one argument"; false
endif

ifeq ($(PORT),)
$(TARGET): nonexisting_port
nonexisting_port:
	@$(ECHO) "Error: Port $(PORT_NAME) does not exist"; false
endif

