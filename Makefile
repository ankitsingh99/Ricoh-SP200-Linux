CC      = gcc
CFLAGS  = -O2 -Wall -Wextra
LIBS    = $(shell cups-config --libs) -lcupsimage -ljbig

FILTER  = rastertoricohjbig
SRC     = rastertoricohjbig.c
PPD     = ricoh-sp200.ppd

FILTER_DIR = /usr/lib/cups/filter
PPD_DIR    = /usr/share/ppd/cupsfilters
PRINTER    = RicohSP200

.PHONY: all build install uninstall register clean

all: build

build: $(FILTER)

$(FILTER): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

install: build
	install -m 755 $(FILTER) $(FILTER_DIR)/$(FILTER)
	install -m 644 $(PPD)    $(PPD_DIR)/$(PPD)
	@echo "Filter and PPD installed."
	@echo "Run 'make register' to add the printer to CUPS (requires USB connection)."

register:
	@URI=$$(lpinfo -v 2>/dev/null | grep -i ricoh | awk '{print $$2}' | head -1); \
	if [ -z "$$URI" ]; then \
		echo "Error: Ricoh printer not found. Check USB connection and try again."; \
		exit 1; \
	fi; \
	lpadmin -p $(PRINTER) -v "$$URI" -P $(PPD_DIR)/$(PPD) -E && \
	echo "Printer '$(PRINTER)' registered at $$URI."

uninstall:
	lpadmin -x $(PRINTER) 2>/dev/null || true
	rm -f $(FILTER_DIR)/$(FILTER)
	rm -f $(PPD_DIR)/$(PPD)
	@echo "Uninstalled."

clean:
	rm -f $(FILTER)
