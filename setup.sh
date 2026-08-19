#!/bin/bash
# ==============================================================================
# Ricoh SP 200 CUPS Driver - Automated Setup & Installer
# Works on macOS (Apple Silicon & Intel) and Linux (Debian/Ubuntu/Fedora/Arch)
# ==============================================================================

set -e

PRINTER_NAME="Ricoh_SP_200_DDST"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OS="$(uname -s)"

echo "========================================================"
echo "  Ricoh SP 200 CUPS Driver Installer"
echo "  Detected OS: $OS ($(uname -m))"
echo "========================================================"

# Check if running with sudo when needed
require_sudo() {
    if [ "$EUID" -ne 0 ]; then
        echo ""
        echo "[*] Superuser permissions required for system installation."
        sudo -v || { echo "Error: sudo required to install printer driver."; exit 1; }
    fi
}

# ------------------------------------------------------------------------------
# 1. Dependency Resolution
# ------------------------------------------------------------------------------
echo ""
echo "[1/5] Checking and installing dependencies..."

if [ "$OS" = "Darwin" ]; then
    # macOS
    if ! command -v brew >/dev/null 2>&1; then
        echo "Error: Homebrew is not installed. Please install Homebrew from https://brew.sh/"
        exit 1
    fi

    BREW_PREFIX="$(brew --prefix)"
    
    # Check packages
    for pkg in cups jbigkit ghostscript; do
        if ! brew list --formula "$pkg" >/dev/null 2>&1; then
            echo "Installing $pkg via Homebrew..."
            brew install "$pkg"
        else
            echo "  ✓ $pkg is installed"
        fi
    done

    FILTER_DIR="/Library/Printers/Ricoh/Filter"
    PPD_DIR="/Library/Printers/PPDs/Contents/Resources"
    PPD_FILE="$PPD_DIR/ricoh-sp200.ppd"
    FILTER_BIN="$FILTER_DIR/rastertoricohjbig"
    CFLAGS="-O2 -Wall -Wextra -I${BREW_PREFIX}/include"
    LIBS="-L${BREW_PREFIX}/lib -lcups -lcupsimage -ljbig"

elif [ -f /etc/debian_version ]; then
    # Debian / Ubuntu
    require_sudo
    echo "Updating packages and installing build dependencies..."
    sudo apt-get update
    sudo apt-get install -y libcups2-dev libcupsimage2-dev libjbig-dev jbigkit-bin gcc ghostscript cups
    
    FILTER_DIR="/usr/lib/cups/filter"
    PPD_DIR="/usr/share/ppd/cupsfilters"
    PPD_FILE="$PPD_DIR/ricoh-sp200.ppd"
    FILTER_BIN="$FILTER_DIR/rastertoricohjbig"
    CFLAGS="-O2 -Wall -Wextra"
    LIBS="$(cups-config --libs) -lcupsimage -ljbig"

elif [ -f /etc/fedora-release ] || [ -f /etc/redhat-release ]; then
    # Fedora / RHEL
    require_sudo
    sudo dnf install -y cups-devel cups-libs jbigkit-devel jbigkit-libs gcc ghostscript
    
    FILTER_DIR="/usr/lib/cups/filter"
    PPD_DIR="/usr/share/ppd/cupsfilters"
    PPD_FILE="$PPD_DIR/ricoh-sp200.ppd"
    FILTER_BIN="$FILTER_DIR/rastertoricohjbig"
    CFLAGS="-O2 -Wall -Wextra"
    LIBS="$(cups-config --libs) -lcupsimage -ljbig"

elif [ -f /etc/arch-release ]; then
    # Arch Linux
    require_sudo
    sudo pacman -S --needed --noconfirm cups ghostscript jbigkit gcc
    
    FILTER_DIR="/usr/lib/cups/filter"
    PPD_DIR="/usr/share/ppd/cupsfilters"
    PPD_FILE="$PPD_DIR/ricoh-sp200.ppd"
    FILTER_BIN="$FILTER_DIR/rastertoricohjbig"
    CFLAGS="-O2 -Wall -Wextra"
    LIBS="$(cups-config --libs) -lcupsimage -ljbig"
else
    echo "Unsupported or unknown OS. Proceeding with generic Linux paths..."
    FILTER_DIR="/usr/lib/cups/filter"
    PPD_DIR="/usr/share/ppd/cupsfilters"
    PPD_FILE="$PPD_DIR/ricoh-sp200.ppd"
    FILTER_BIN="$FILTER_DIR/rastertoricohjbig"
    CFLAGS="-O2 -Wall -Wextra"
    LIBS="$(cups-config --libs 2>/dev/null || echo -lcups) -lcupsimage -ljbig"
fi

# ------------------------------------------------------------------------------
# 2. Compile Filter
# ------------------------------------------------------------------------------
echo ""
echo "[2/5] Compiling rastertoricohjbig driver binary..."
cd "$SCRIPT_DIR"

gcc $CFLAGS -o rastertoricohjbig rastertoricohjbig.c $LIBS
echo "  ✓ Filter compiled successfully: $SCRIPT_DIR/rastertoricohjbig"

# ------------------------------------------------------------------------------
# 3. Install Filter and PPD
# ------------------------------------------------------------------------------
echo ""
echo "[3/5] Installing driver files to system directories..."
require_sudo

sudo mkdir -p "$FILTER_DIR" "$PPD_DIR"

sudo install -m 755 rastertoricohjbig "$FILTER_BIN"
sudo install -m 644 ricoh-sp200.ppd "$PPD_FILE"

if [ "$OS" = "Darwin" ]; then
    # Fix ownership to prevent macOS CUPS sandbox insecure permissions error
    sudo chown -R root:wheel /Library/Printers/Ricoh
    sudo chown root:wheel "$FILTER_BIN" "$PPD_FILE"
    sudo chmod 755 "$FILTER_BIN"
    sudo chmod 644 "$PPD_FILE"
    sudo xattr -d com.apple.quarantine "$FILTER_BIN" 2>/dev/null || true
fi

echo "  ✓ Filter installed at: $FILTER_BIN"
echo "  ✓ PPD installed at:    $PPD_FILE"

# ------------------------------------------------------------------------------
# 4. Printer Discovery & Registration
# ------------------------------------------------------------------------------
echo ""
echo "[4/5] Detecting connected Ricoh SP 200 printer..."

# Attempt discovery via CUPS backend
DEVICE_URI=""
if [ "$OS" = "Darwin" ]; then
    DEVICE_URI=$(/usr/libexec/cups/backend/usb 2>/dev/null | grep -i "ricoh" | awk '{print $2}' | head -1 || true)
fi

if [ -z "$DEVICE_URI" ]; then
    DEVICE_URI=$(lpinfo -v 2>/dev/null | grep -i "ricoh" | awk '{print $2}' | head -1 || true)
fi

if [ -z "$DEVICE_URI" ]; then
    # Fallback to general USB if only one USB printer is attached
    DEVICE_URI=$(lpinfo -v 2>/dev/null | grep -i "usb:" | awk '{print $2}' | head -1 || true)
fi

if [ -n "$DEVICE_URI" ]; then
    echo "  ✓ Detected printer device URI: $DEVICE_URI"
else
    echo "  ! Note: Printer not detected on USB right now."
    echo "    Using default placeholder URI: usb://RICOH/SP%20200%20DDST"
    DEVICE_URI="usb://RICOH/SP%20200%20DDST"
fi

echo "Registering printer queue '$PRINTER_NAME'..."
sudo lpadmin -x "$PRINTER_NAME" 2>/dev/null || true
sudo lpadmin -p "$PRINTER_NAME" -v "$DEVICE_URI" -P "$PPD_FILE" -E
sudo cupsenable "$PRINTER_NAME" 2>/dev/null || true
sudo cupsaccept "$PRINTER_NAME" 2>/dev/null || true

echo "  ✓ Printer '$PRINTER_NAME' is registered and enabled."

# ------------------------------------------------------------------------------
# 5. Status & Test
# ------------------------------------------------------------------------------
echo ""
echo "[5/5] Checking printer queue status..."
lpstat -p "$PRINTER_NAME" -l

echo ""
echo "========================================================"
echo "  INSTALLATION COMPLETE!"
echo "========================================================"
echo "  Printer Name: $PRINTER_NAME"
echo "  PPD File:     $PPD_FILE"
echo "  Filter:       $FILTER_BIN"
echo ""
echo "  To send a test page:"
echo "    echo 'Hello from Ricoh SP 200' | lpr -P $PRINTER_NAME"
echo "  or run:"
echo "    ./test_print.sh"
echo "========================================================"
