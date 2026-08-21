# Ricoh SP 200 Series CUPS Driver (Linux & macOS)

[![CI](https://github.com/ankitsingh99/Ricoh-SP200-Linux/actions/workflows/ci.yml/badge.svg)](https://github.com/ankitsingh99/Ricoh-SP200-Linux/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20macOS-blue.svg)](#supported-operating-systems)
[![CUPS Version](https://img.shields.io/badge/CUPS-2.0%2B-green.svg)](#prerequisites)

A native CUPS filter and PPD driver for the **Ricoh SP 200** series monochrome laser printers (including Ricoh SP 200, SP 201, SP 202, SP 203, SP 204, and related DDST series printers).

The Ricoh SP 200 family lacks official Linux/macOS drivers and is not supported by standard printer stacks (`foo2zjs`, OpenPrinting, Gutenprint, HPLIP). This driver implements the native PJL + JBIG1 bi-level protocol reverse-engineered from USB packet captures.

---

## Supported Printers & Systems

### Tested Models
- **Ricoh SP 200 / SP 200S / SP 200N / SP 200NW**
- **Ricoh SP 201 / SP 201N / SP 201NW**
- **Ricoh SP 202 / SP 203 / SP 204 / SP 204SF**
- Other Ricoh DDST GDI monochrome laser printers using the same PJL+JBIG1 stream

### Supported Operating Systems
- **Linux**: Debian, Ubuntu, Linux Mint, Raspberry Pi OS, Fedora, RHEL, AlmaLinux, Arch Linux, Manjaro
- **macOS**: Sonoma, Ventura, Monterey, Big Sur (Apple Silicon M1/M2/M3/M4 & Intel x86_64)

---

## Quick Start (Automated Installation)

Run the included automated setup script:

```bash
git clone https://github.com/ankitsingh99/Ricoh-SP200-Linux.git
cd Ricoh-SP200-Linux
chmod +x setup.sh test_print.sh uninstall.sh
./setup.sh
```

The script will:
1. Detect your operating system and package manager.
2. Install necessary build and runtime dependencies (`cups`, `jbigkit`, `ghostscript`, `gcc`).
3. Compile the `rastertoricohjbig` binary with optimal compiler optimizations.
4. Install the filter and PPD into system directories (handling macOS CUPS sandbox permissions automatically).
5. Auto-discover the USB printer and register the CUPS queue.

---

## Prerequisites & Dependencies

If you prefer installing dependencies manually:

### macOS (Homebrew)
```bash
brew install cups jbigkit ghostscript
```

### Debian / Ubuntu / Linux Mint / Raspberry Pi OS
```bash
sudo apt update
sudo apt install -y libcups2-dev libcupsimage2-dev libjbig-dev jbigkit-bin gcc ghostscript cups cups-client
```

### Fedora / RHEL / AlmaLinux
```bash
sudo dnf install -y cups-devel cups-libs jbigkit-devel jbigkit-libs gcc ghostscript
```

### Arch Linux / Manjaro
```bash
sudo pacman -S --needed cups ghostscript jbigkit gcc
```

---

## Manual Build & Installation

### Using Makefile

```bash
# 1. Build and install filter + PPD
sudo make install

# 2. Register printer queue (printer must be connected via USB)
sudo make register

# 3. Send a test print
make test

# 4. To uninstall
sudo make uninstall
```

### Manual Step-by-Step

<details>
<summary><b>macOS Step-by-Step Instructions</b></summary>

1. **Compile the driver binary**:
   ```bash
   BREW=$(brew --prefix)
   gcc -O2 -Wall -Wextra -I$BREW/include -o rastertoricohjbig rastertoricohjbig.c \
       -L$BREW/lib -lcups -lcupsimage -ljbig
   ```

2. **Install filter and PPD with root ownership**:
   > **Note for macOS:** macOS CUPS runs in sandbox mode and strictly rejects filters stored in `/opt/homebrew/...` or user-owned directories with *“insecure permissions”*. Custom filters must be placed in `/Library/Printers/` with `root:wheel` ownership.

   ```bash
   sudo mkdir -p /Library/Printers/Ricoh/Filter /Library/Printers/PPDs/Contents/Resources
   sudo install -m 755 rastertoricohjbig /Library/Printers/Ricoh/Filter/rastertoricohjbig
   sed 's|application/vnd.cups-raster 0 .*|application/vnd.cups-raster 0 /Library/Printers/Ricoh/Filter/rastertoricohjbig|' ricoh-sp200.ppd | sudo tee /Library/Printers/PPDs/Contents/Resources/ricoh-sp200.ppd >/dev/null
   sudo chown -R root:wheel /Library/Printers/Ricoh /Library/Printers/PPDs/Contents/Resources/ricoh-sp200.ppd
   sudo chmod 755 /Library/Printers/Ricoh/Filter/rastertoricohjbig
   sudo chmod 644 /Library/Printers/PPDs/Contents/Resources/ricoh-sp200.ppd
   sudo xattr -d com.apple.quarantine /Library/Printers/Ricoh/Filter/rastertoricohjbig 2>/dev/null || true
   ```

3. **Register and enable printer**:
   ```bash
   URI=$(/usr/libexec/cups/backend/usb 2>/dev/null | grep -i ricoh | awk '{print $2}' | head -1)
   sudo lpadmin -p Ricoh_SP_200_DDST -v "$URI" \
       -P /Library/Printers/PPDs/Contents/Resources/ricoh-sp200.ppd -E
   sudo cupsenable Ricoh_SP_200_DDST
   sudo cupsaccept Ricoh_SP_200_DDST
   ```

4. **Send test page**:
   ```bash
   ./test_print.sh
   ```
</details>

<details>
<summary><b>Linux Step-by-Step Instructions</b></summary>

1. **Compile driver**:
   ```bash
   gcc -O2 -Wall -Wextra -o rastertoricohjbig rastertoricohjbig.c \
       $(cups-config --cflags --libs) -lcupsimage -ljbig
   ```

2. **Install filter and PPD**:
   ```bash
   sudo install -m 755 rastertoricohjbig /usr/lib/cups/filter/rastertoricohjbig
   sudo install -m 644 ricoh-sp200.ppd    /usr/share/ppd/cupsfilters/ricoh-sp200.ppd
   ```

3. **Register and enable printer**:
   ```bash
   URI=$(lpinfo -v | grep -i ricoh | awk '{print $2}' | head -1)
   sudo lpadmin -p Ricoh_SP_200_DDST -v "$URI" \
       -P /usr/share/ppd/cupsfilters/ricoh-sp200.ppd -E
   sudo cupsenable Ricoh_SP_200_DDST
   sudo cupsaccept Ricoh_SP_200_DDST
   ```

4. **Send test page**:
   ```bash
   ./test_print.sh
   ```
</details>

---

## Troubleshooting Guide

### 1. `File ".../rastertoricohjbig" has insecure permissions (0100755/uid=501)`
- **Cause**: On macOS, CUPS rejects filters located in user directories or Homebrew directories because non-root users have write access to parent paths.
- **Solution**: Install the filter in `/Library/Printers/Ricoh/Filter/` with `root:wheel` ownership:
  ```bash
  sudo chown -R root:wheel /Library/Printers/Ricoh
  sudo chmod 755 /Library/Printers/Ricoh/Filter/rastertoricohjbig
  ```

### 2. Printer status shows `Offline` or Not Responding
- **Apple Silicon Accessory Authorization**: On macOS Ventura/Sonoma/Sequoia, grant permission when prompted *"Allow accessory to connect?"* or check **System Settings → Privacy & Security → Allow accessories to connect**.
- **USB Cable**: Ensure the printer is powered ON (solid green status LED) and connected via USB. Check if the OS detects the device:
  - macOS: `/usr/libexec/cups/backend/usb`
  - Linux: `lsusb` and `lpinfo -v | grep ricoh`

### 3. Clearing Stuck Jobs
```bash
cancel -a Ricoh_SP_200_DDST
```

---

## Technical Overview & Protocol

The Ricoh SP 200 uses **PJL (Printer Job Language)** encapsulating **JBIG1 (ITU-T T.82)** compressed 1-bit raster data:

```
[CUPS Raster Stream]
        │
        ▼
[rastertoricohjbig Filter]
   ├─ Converts input raster to 1-bit monochrome (600 DPI)
   ├─ Compresses image using libjbig (ITU-T T.82)
   ├─ Wraps payload in PJL job headers & chunked IMAGELEN packets
   └─ Emits DOTCOUNT & PAGESTATUS=END to trigger page feed
        │
        ▼
[USB Backend → Ricoh SP 200 Printer]
```

### Key PJL Commands
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

2. **Page Header & Data**:
   ```pjl
   @PJL SET PAPER=A4
   @PJL SET PAPERWIDTH=4960
   @PJL SET PAPERLENGTH=7016
   @PJL SET RESOLUTION=600
   @PJL SET IMAGELEN=<bytes>
   <JBIG1 payload>
   @PJL SET DOTCOUNT=<black_pixels>
   @PJL SET PAGESTATUS=END
   ```

3. **Job Trailer**:
   ```pjl
   @PJL EOJ
   \x1b%-12345X
   ```

---

## Repository Structure

| File / Directory | Description |
|---|---|
| [`rastertoricohjbig.c`](rastertoricohjbig.c) | Native C source code for the CUPS raster filter |
| [`ricoh-sp200.ppd`](ricoh-sp200.ppd) | PostScript Printer Description file |
| [`setup.sh`](setup.sh) | Automated multi-platform installer and configuration script |
| [`test_print.sh`](test_print.sh) | Quick single-page test print utility |
| [`uninstall.sh`](uninstall.sh) | Uninstaller script to clean up printer queues and files |
| [`Makefile`](Makefile) | Multi-platform build and install recipes |
| [`.github/workflows/ci.yml`](.github/workflows/ci.yml) | Continuous Integration pipeline for Linux & macOS |
| [`LICENSE`](LICENSE) | MIT License |
| [`CONTRIBUTING.md`](CONTRIBUTING.md) | Contribution guidelines |
| [`CODE_OF_CONDUCT.md`](CODE_OF_CONDUCT.md) | Contributor Covenant Code of Conduct |
| [`SECURITY.md`](SECURITY.md) | Security vulnerability disclosure policy |

---

## Contributing

We welcome issues, compatibility reports, and pull requests! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for details on setting up your environment and submitting changes.

---

## License

This project is licensed under the [MIT License](LICENSE).
