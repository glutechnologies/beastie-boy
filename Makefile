ROOT_DIR := $(CURDIR)
BUILD_ROOT := $(ROOT_DIR)/build-root

.PHONY: all build build-release build-debug configure configure-release configure-debug \
	install install-release install-debug test package package-release package-debug \
	package-deb package-deb-release package-deb-debug clean distclean

all: build

build: build-release

build-release:
	$(MAKE) -C $(BUILD_ROOT) TAG=release build

build-debug:
	$(MAKE) -C $(BUILD_ROOT) TAG=debug build

configure: configure-release

configure-release:
	$(MAKE) -C $(BUILD_ROOT) TAG=release configure

configure-debug:
	$(MAKE) -C $(BUILD_ROOT) TAG=debug configure

install: install-release

install-release:
	$(MAKE) -C $(BUILD_ROOT) TAG=release install

install-debug:
	$(MAKE) -C $(BUILD_ROOT) TAG=debug install

test:
	$(MAKE) -C $(BUILD_ROOT) TAG=debug test

package: package-release

package-release:
	$(MAKE) -C $(BUILD_ROOT) TAG=release package

package-debug:
	$(MAKE) -C $(BUILD_ROOT) TAG=debug package

package-deb: package-deb-release

package-deb-release:
	$(MAKE) -C $(BUILD_ROOT) TAG=release package-deb

package-deb-debug:
	$(MAKE) -C $(BUILD_ROOT) TAG=debug package-deb

clean:
	$(MAKE) -C $(BUILD_ROOT) TAG=release clean
	$(MAKE) -C $(BUILD_ROOT) TAG=debug clean

distclean: clean
	rm -rf $(ROOT_DIR)/bin
