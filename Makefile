.PHONY: all clean zlog zlog-clean

ZLOG_DIR := third_party/zlog
ZLOG_SRC_DIR := $(ZLOG_DIR)/src

all: zlog
	$(MAKE) -C src -B
	$(MAKE) -C test -B

clean:
	$(MAKE) -C src clean
	$(MAKE) -C test clean
	$(MAKE) zlog-clean
	rm -f config.mk

zlog:
	@if [ ! -f "$(ZLOG_SRC_DIR)/Makefile" ]; then \
		echo "[ERROR] zlog submodule is not initialized."; \
		echo "        Run: git submodule update --init --recursive"; \
		exit 1; \
	fi
	$(MAKE) -C $(ZLOG_SRC_DIR) clean static CFLAGS="" DEBUG=""

zlog-clean:
	@if [ -f "$(ZLOG_SRC_DIR)/Makefile" ]; then \
		$(MAKE) -C $(ZLOG_SRC_DIR) clean; \
	fi
