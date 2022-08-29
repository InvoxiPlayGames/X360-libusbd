TARGET = x360_controller

DEBUG   ?= 0
ARCH    ?= arm64
SDK     ?= macosx

SYSROOT  := $(shell xcrun --sdk $(SDK) --show-sdk-path)
ifeq ($(SYSROOT),)
$(error Could not find SDK "$(SDK)")
endif
CLANG    := clang
CC       := $(CLANG) -isysroot $(SYSROOT) -arch $(ARCH)

CFLAGS  = -O1 -Wall -g -fstack-protector-all
LDFLAGS = -L. -lusbd

ifneq ($(DEBUG),0)
DEFINES += -DDEBUG=$(DEBUG)
endif

FRAMEWORKS = -framework CoreFoundation -framework IOKit

SOURCES = main.c

HEADERS = 

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) $(FRAMEWORKS) $(DEFINES) $(LDFLAGS) -o $@ $(SOURCES)
#	codesign -s - $@

clean:
	rm -f -- $(TARGET)
