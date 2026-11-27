NAME = CEAGLE
VERSION = 1.0.0
DESCRIPTION = "CEagle program registry"
COMPRESSED = NO
ARCHIVED = YES
BSSHEAP_LOW = 0xD072C6

CFLAGS = -Wall -Wextra -Oz -DAPP_NAME=\"$(NAME)\" -DAPP_VERSION=\"$(VERSION)\"
ifdef CEAGLE_TEST
CFLAGS += -DCEAGLE_TEST
endif
EXTRA_LDFLAGS += --no-warn-execstack
EXTRA_PRE_LDFLAGS += --no-warn-execstack

include $(shell cedev-config --makefile)
