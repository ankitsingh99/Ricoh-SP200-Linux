# Ricoh SP 200 Series CUPS Driver (macOS & Linux)

A native CUPS filter and PPD driver for the **Ricoh SP 200** series monochrome laser printer (Ricoh SP 200, SP 201, SP 202, SP 203, SP 204, and DDST series).

The Ricoh SP 200 has no official Linux/macOS driver and is absent from standard print stacks (`foo2zjs`, OpenPrinting, Gutenprint, HPLIP). This driver implements the native PJL + JBIG1 bi-level protocol reverse-engineered from USB traffic.

---

## Quick Start (Automated Setup)

Run the included automated setup script:

```bash
chmod +x setup.sh test_print.sh uninstall.sh
./setup.sh
```

The script will automatically detect your OS, install any missing dependencies, compile the driver binary, configure root permissions, register the printer with CUPS, and check queue readiness.

---

## Dependencies

### macOS (Apple Silicon M1/M2/M3/M4 & Intel)
```bash
brew install cups jbigkit ghostscript
```

### Debian / Ubuntu / Linux Mint / Raspberry Pi OS
```bash
sudo apt update
sudo apt install libcups2-dev libcupsimage2-dev libjbig-dev jbigkit-bin gcc ghostscript cups
```

### Fedora / RHEL / AlmaLinux
```bash
sudo dnf install cups-devel cups-libs jbigkit-devel jbigkit-libs gcc ghostscript
```

### Arch Linux / Manjaro
```bash
sudo pacman -S cups ghostscript jbigkit gcc
```

---

## Manual Build & Installation

### Using Make

```bash
# 1. Build and install filter + PPD
sudo make install

# 2. Register printer queue (printer must be plugged in via USB)
sudo make register

# 3. Test print
make test

# To remove
sudo make uninstall
```

---

## Manual Step-by-Step Guide

### macOS

1. **Compile Driver**:
   ```bash
   BREW=$(brew --prefix)
   gcc -O2 -I$BREW/include -o rastertoricohjbig rastertoricohjbig.c \
       -L$BREW/lib -lcups -lcupsimage -ljbig
   ```

2. **Install Filter and PPD with Root Ownership**:
   > **Note for macOS:** macOS CUPS runs in sandbox mode and strictly rejects filters stored in `/opt/homebrew/...` or user-owned paths with *“insecure permissions”* error. Custom filters must be placed in `/Library/Printers/` with `root:wheel` ownership.

   ```bash
   sudo mkdir -p /Library/Printers/Ricoh/Filter /Library/Printers/PPDs/Contents/Resources
   sudo install -m 755 rastertoricohjbig /Library/Printers/Ricoh/Filter/rastertoricohjbig
   sudo install -m 644 ricoh-sp200.ppd    /Library/Printers/PPDs/Contents/Resources/ricoh-sp200.ppd
   sudo chown -R root:wheel /Library/Printers/Ricoh
   sudo chmod 755 /Library/Printers/Ricoh/Filter/rastertoricohjbig
   sudo chmod 644 /Library/Printers/PPDs/Contents/Resources/ricoh-sp200.ppd
   sudo xattr -d com.apple.quarantine /Library/Printers/Ricoh/Filter/rastertoricohjbig 2>/dev/null || true
   ```

3. **Register & Enable Printer**:
   ```bash
   # Discover USB URI
   URI=$(/usr/libexec/cups/backend/usb | grep -i ricoh | awk '{print $2}' | head -1)

   sudo lpadmin -p Ricoh_SP_200_DDST -v "$URI" \
       -P /Library/Printers/PPDs/Contents/Resources/ricoh-sp200.ppd -E
   sudo cupsenable Ricoh_SP_200_DDST
   sudo cupsaccept Ricoh_SP_200_DDST
   ```

4. **Send Test Print**:
   ```bash
   echo "Hello from Ricoh SP 200!" | lpr -P Ricoh_SP_200_DDST
   ```

---

### Linux

1. **Compile Driver**:
   ```bash
   gcc -O2 -o rastertoricohjbig rastertoricohjbig.c \
       $(cups-config --libs) -lcupsimage -ljbig
   ```

2. **Install Filter & PPD**:
   ```bash
   sudo install -m 755 rastertoricohjbig /usr/lib/cups/filter/rastertoricohjbig
   sudo install -m 644 ricoh-sp200.ppd    /usr/share/ppd/cupsfilters/
   ```

3. **Register Printer**:
   ```bash
   URI=$(lpinfo -v | grep -i ricoh | awk '{print $2}' | head -1)

   sudo lpadmin -p Ricoh_SP_200_DDST -v "$URI" \
       -P /usr/share/ppd/cupsfilters/ricoh-sp200.ppd -E
   ```

---

## Troubleshooting Guide

### 1. Error: `File ".../rastertoricohjbig" has insecure permissions (0100755/uid=501)`
- **Cause:** On macOS, CUPS rejects filters located in user directories or `/opt/homebrew` because non-root users have write access to parent directories.
- **Fix:** Install the filter into `/Library/Printers/Ricoh/Filter/rastertoricohjbig` and ensure ownership is `root:wheel`:
  ```bash
  sudo chown -R root:wheel /Library/Printers/Ricoh
  sudo chmod 755 /Library/Printers/Ricoh/Filter/rastertoricohjbig
  ```

### 2. Printer shows as `Offline`
- **Cause 1: Apple Silicon Accessory Prompt:** On macOS Ventura/Sonoma/Sequoia, check for the prompt **"Allow accessory to connect?"** or enable it in **System Settings → Privacy & Security → Allow accessories to connect**.
- **Cause 2: USB Cable / Hub:** Ensure the printer power switch is ON (solid green light) and the USB-B cable is securely connected. Check if macOS sees it:
  ```bash
  /usr/libexec/cups/backend/usb
  ```
  *(Should output `direct usb://RICOH/SP%20200%20DDST?serial=...`)*.

### 3. Clear Stuck Print Jobs
```bash
cancel -a Ricoh_SP_200_DDST
```

---

## How the Protocol Works

The Ricoh SP 200 uses **PJL (Printer Job Language)** wrapping **JBIG1 (ITU-T T.82)** compressed 1-bit raster data.

1. **Job Header**:
   ```pjl
   \x1b%-12345X@PJL
   @PJL SET TIMESTAMP=YYYY/MM/DD HH:MM:SS
   @PJL SET FILENAME=printjob
   @PJL SET COMPRESS=JBIG
   @PJL SET USERNAME=lp
   @PJL SET COVER=OFF
   @PJL SET HOLD=OFF
   @PJL SET PAGESTATUS=START
   @PJL SET COPIES=1
   @PJL SET MEDIASOURCE=TRAY1
   @PJL SET MEDIATYPE=PLAINRECYCLE
   ```

2. **Page Data**:
   - For each page, CUPS raster bitmap (600 DPI monochrome 1-bit) is compressed using `jbigkit` (`jbg_enc_init`, `jbg_enc_out`).
   - The compressed JBIG stream is chunked into payloads $\le 65508$ bytes.
   - Each chunk is preceded by: `@PJL SET IMAGELEN=<chunk_size>\r\n`.
   - At page end: `@PJL SET DOTCOUNT=<black_pixels>\r\n@PJL SET PAGESTATUS=END\r\n`.

3. **Job Trailer**:
   ```pjl
   \x1b%-12345X
   ```

---

## Repository Files

| File | Description |
|---|---|
| `rastertoricohjbig.c` | Native C source code for the CUPS raster filter |
| `ricoh-sp200.ppd` | Adobe-compliant PostScript Printer Description file |
| `setup.sh` | Automated multi-platform installer and configuration script |
| `test_print.sh` | Quick test print utility script |
| `uninstall.sh` | Uninstaller script to clean up printer queues and files |
| `Makefile` | Multi-platform build and install recipes |
