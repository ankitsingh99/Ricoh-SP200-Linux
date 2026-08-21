#!/bin/bash
# ==============================================================================
# Ricoh SP 200 - Test Print Script
# ==============================================================================

set -euo pipefail

PRINTER_NAME="Ricoh_SP_200_DDST"

echo "Checking printer '$PRINTER_NAME' status..."
lpstat -p "$PRINTER_NAME" -l 2>/dev/null || echo "Warning: Printer queue '$PRINTER_NAME' not found or CUPS not running."

echo ""
echo "Sending single test print page to '$PRINTER_NAME'..."

{
  echo "=================================================="
  echo "         Ricoh SP 200 Series Test Print           "
  echo "=================================================="
  echo ""
  echo "  Timestamp : $(date)"
  echo "  Platform  : $(uname -s) ($(uname -m))"
  echo "  Printer   : $PRINTER_NAME"
  echo "  Driver    : Native JBIG1 CUPS Filter"
  echo ""
  echo "  If you can read this, your Ricoh SP 200 driver"
  echo "  is functioning properly on your system!"
  echo ""
  echo "=================================================="
} | lpr -P "$PRINTER_NAME"

echo "✓ Print job submitted. Checking queue..."
sleep 1
lpstat -o "$PRINTER_NAME" 2>/dev/null || lpstat -o 2>/dev/null || true
