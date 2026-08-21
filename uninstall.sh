#!/bin/bash
# ==============================================================================
# Ricoh DDST Driver Suite - Uninstaller Script
# ==============================================================================

set -euo pipefail

PRINTER_NAME="Ricoh_SP_200_DDST"
OS="$(uname -s)"

echo "Uninstalling Ricoh DDST driver and removing printer queue..."

if [ "$EUID" -ne 0 ]; then
    echo "[*] Superuser permissions required."
    sudo -v || { echo "Error: sudo required."; exit 1; }
fi

# Remove CUPS queue
sudo lpadmin -x "$PRINTER_NAME" 2>/dev/null || true
echo "  [OK] Removed CUPS printer queue '$PRINTER_NAME'"

# Remove files
if [ "$OS" = "Darwin" ]; then
    sudo rm -f /Library/Printers/Ricoh/Filter/rastertoricohddst /Library/Printers/Ricoh/Filter/rastertoricohjbig
    sudo rm -f /Library/Printers/PPDs/Contents/Resources/ricoh-sp*.ppd
    sudo rm -rf /Library/Printers/Ricoh 2>/dev/null || true
else
    sudo rm -f /usr/lib/cups/filter/rastertoricohddst /usr/lib/cups/filter/rastertoricohjbig
    sudo rm -f /usr/share/ppd/cupsfilters/ricoh-sp*.ppd
fi

echo "  [OK] Removed filter and PPD files."
echo "Uninstallation complete."
