#!/bin/bash
# ==============================================================================
# Ricoh SP 200 - Test Print Script
# ==============================================================================

PRINTER_NAME="Ricoh_SP_200_DDST"

echo "Checking printer '$PRINTER_NAME' status..."
lpstat -p "$PRINTER_NAME" -l

echo ""
echo "Sending test print to $PRINTER_NAME..."
printf "========================================\n" | lpr -P "$PRINTER_NAME"
printf "  Ricoh SP 200 Test Print\n"              | lpr -P "$PRINTER_NAME"
printf "  Date: %s\n" "$(date)"                  | lpr -P "$PRINTER_NAME"
printf "  System: %s (%s)\n" "$(uname -s)" "$(uname -m)" | lpr -P "$PRINTER_NAME"
printf "  Driver: Native JBIG1 CUPS Filter\n"    | lpr -P "$PRINTER_NAME"
printf "========================================\n" | lpr -P "$PRINTER_NAME"

echo "Print job submitted. Checking queue..."
sleep 1
lpstat -o
