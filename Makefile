CC      ?= gcc
CFLAGS  ?= -O2 -Wall -Wextra
FILTER   = rastertoricohjbig
SRC      = rastertoricohjbig.c
PPD      = ricoh-sp200.ppd
PRINTER ?= Ricoh_SP_200_DDST

UNAME := $(shell uname -s)

ifeq ($(UNAME), Darwin)
    # macOS CUPS environment
    BREW_PREFIX ?= $(shell brew --prefix 2>/dev/null || echo /opt/homebrew)
    FILTER_DIR   = /Library/Printers/Ricoh/Filter
    PPD_DIR      = /Library/Printers/PPDs/Contents/Resources
    CPPFLAGS    += -I$(BREW_PREFIX)/include
    LDFLAGS     += -L$(BREW_PREFIX)/lib
    LIBS         = -lcups -lcupsimage -ljbig
else
    # Linux CUPS environment
    FILTER_DIR   = /usr/lib/cups/filter
    PPD_DIR      = /usr/share/ppd/cupsfilters
    CUPS_CFLAGS := $(shell cups-config --cflags 2>/dev/null)
    CUPS_LIBS   := $(shell cups-config --libs 2>/dev/null || echo -lcups)
    CPPFLAGS    += $(CUPS_CFLAGS)
    LIBS         = $(CUPS_LIBS) -lcupsimage -ljbig
endif

.PHONY: all build install uninstall register test clean help

all: build

help:
	@echo "Ricoh SP 200 Driver Makefile"
	@echo "----------------------------"
	@echo "make build         - Compile the CUPS raster filter binary"
	@echo "sudo make install  - Install filter and PPD into system directories"
	@echo "sudo make register - Register and enable the printer queue with CUPS"
	@echo "make test          - Send a test page to $(PRINTER)"
	@echo "sudo make uninstall- Remove printer queue and driver files"
	@echo "make clean         - Remove compiled binaries"

build: $(FILTER)

$(FILTER): $(SRC)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

install: build
	mkdir -p $(DESTDIR)$(FILTER_DIR) $(DESTDIR)$(PPD_DIR)
	install -m 755 $(FILTER) $(DESTDIR)$(FILTER_DIR)/$(FILTER)
ifeq ($(UNAME), Darwin)
	# macOS sandbox requires absolute path in PPD and root:wheel ownership
	sed 's|application/vnd.cups-raster 0 .*|application/vnd.cups-raster 0 /Library/Printers/Ricoh/Filter/rastertoricohjbig|' $(PPD) > $(DESTDIR)$(PPD_DIR)/$(PPD)
	chmod 644 $(DESTDIR)$(PPD_DIR)/$(PPD)
	chown -R root:wheel /Library/Printers/Ricoh $(DESTDIR)$(PPD_DIR)/$(PPD) 2>/dev/null || true
	xattr -d com.apple.quarantine $(DESTDIR)$(FILTER_DIR)/$(FILTER) 2>/dev/null || true
else
	install -m 644 $(PPD) $(DESTDIR)$(PPD_DIR)/$(PPD)
endif
	@echo "Filter and PPD installed successfully."
	@echo "Run 'sudo make register' to register printer with CUPS."

register:
	@URI=$$(/usr/libexec/cups/backend/usb 2>/dev/null | grep -i ricoh | awk '{print $$2}' | head -1); \
	if [ -z "$$URI" ]; then \
		URI=$$(lpinfo -v 2>/dev/null | grep -i ricoh | awk '{print $$2}' | head -1); \
	fi; \
	if [ -z "$$URI" ]; then \
		URI=$$(lpinfo -v 2>/dev/null | grep -i usb | awk '{print $$2}' | head -1); \
	fi; \
	if [ -z "$$URI" ]; then \
		echo "Note: Specific USB ID not discovered yet; registering with default 'usb://RICOH/SP%20200%20DDST'..."; \
		URI="usb://RICOH/SP%20200%20DDST"; \
	fi; \
	lpadmin -x $(PRINTER) 2>/dev/null || true; \
	lpadmin -p $(PRINTER) -v "$$URI" -P $(PPD_DIR)/$(PPD) -E && \
	cupsenable $(PRINTER) 2>/dev/null || true; \
	cupsaccept $(PRINTER) 2>/dev/null || true; \
	echo "Printer '$(PRINTER)' registered and enabled at $$URI."

test:
	@echo "Sending test print to $(PRINTER)..."
	@{ \
		printf "========================================\n"; \
		printf "  Ricoh SP 200 Test Page\n"; \
		printf "  Date: %s\n" "$$(date)"; \
		printf "  Driver: Native JBIG1 CUPS Filter\n"; \
		printf "========================================\n"; \
	} | lpr -P $(PRINTER)
	@echo "Job submitted."

uninstall:
	lpadmin -x $(PRINTER) 2>/dev/null || true
	rm -f $(DESTDIR)$(FILTER_DIR)/$(FILTER)
	rm -f $(DESTDIR)$(PPD_DIR)/$(PPD)
ifeq ($(UNAME), Darwin)
	rm -rf /Library/Printers/Ricoh 2>/dev/null || true
endif
	@echo "Uninstalled."

clean:
	rm -f $(FILTER)
