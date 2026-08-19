CC     = gcc
CFLAGS = -O2 -Wall -Wextra

FILTER  = rastertoricohjbig
SRC     = rastertoricohjbig.c
PPD     = ricoh-sp200.ppd
PRINTER = Ricoh_SP_200_DDST

UNAME := $(shell uname)

ifeq ($(UNAME), Darwin)
    # macOS: custom filter goes into /Library/Printers/Ricoh/Filter/ to satisfy CUPS sandbox & root ownership
    BREW_PREFIX := $(shell brew --prefix 2>/dev/null || echo /usr/local)
    FILTER_DIR   = /Library/Printers/Ricoh/Filter
    PPD_DIR      = /Library/Printers/PPDs/Contents/Resources
    CFLAGS      += -I$(BREW_PREFIX)/include
    LIBS         = -L$(BREW_PREFIX)/lib -lcups -lcupsimage -ljbig
else
    FILTER_DIR   = /usr/lib/cups/filter
    PPD_DIR      = /usr/share/ppd/cupsfilters
    LIBS         = $(shell cups-config --libs 2>/dev/null || echo -lcups) -lcupsimage -ljbig
endif

.PHONY: all build install uninstall register test clean

all: build

build: $(FILTER)

$(FILTER): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

install: build
	mkdir -p $(FILTER_DIR) $(PPD_DIR)
	install -m 755 $(FILTER) $(FILTER_DIR)/$(FILTER)
	install -m 644 $(PPD)    $(PPD_DIR)/$(PPD)
ifeq ($(UNAME), Darwin)
	chown -R root:wheel /Library/Printers/Ricoh 2>/dev/null || true
	chown root:wheel $(FILTER_DIR)/$(FILTER) $(PPD_DIR)/$(PPD) 2>/dev/null || true
	chmod 755 $(FILTER_DIR)/$(FILTER)
	chmod 644 $(PPD_DIR)/$(PPD)
	xattr -d com.apple.quarantine $(FILTER_DIR)/$(FILTER) 2>/dev/null || true
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
	@printf "Ricoh SP 200 Test Page\nDate: %s\n" "$$(date)" | lpr -P $(PRINTER)
	@echo "Job sent."

uninstall:
	lpadmin -x $(PRINTER) 2>/dev/null || true
	rm -f $(FILTER_DIR)/$(FILTER)
	rm -f $(PPD_DIR)/$(PPD)
ifeq ($(UNAME), Darwin)
	rm -rf /Library/Printers/Ricoh 2>/dev/null || true
endif
	@echo "Uninstalled."

clean:
	rm -f $(FILTER)
