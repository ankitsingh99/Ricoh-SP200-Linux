# Ricoh Universal DDST/GDI CUPS Driver Suite (Linux & macOS)

[![CI](https://github.com/ankitsingh99/Ricoh-SP200-Linux/actions/workflows/ci.yml/badge.svg)](https://github.com/ankitsingh99/Ricoh-SP200-Linux/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/ankitsingh99/Ricoh-SP200-Linux?color=blue)](https://github.com/ankitsingh99/Ricoh-SP200-Linux/releases)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20macOS-blue.svg)](#supported-operating-systems)
[![CUPS Version](https://img.shields.io/badge/CUPS-2.0%2B-green.svg)](#prerequisites--dependencies)

A native, high-performance CUPS raster filter and Adobe-compliant PPD driver suite for **Ricoh DDST/GDI monochrome and laser printers** on **Linux** and **macOS** (Apple Silicon M1/M2/M3/M4 & Intel x86_64).

Consumer and SMB Ricoh DDST/GDI printers lack official Linux and macOS drivers and are absent from standard printer stacks (`foo2zjs`, OpenPrinting, Gutenprint, HPLIP). This project provides a direct native C implementation of the PJL + ITU-T T.82 JBIG1 wire protocol reverse-engineered from USB traffic.

---

## Supported Printers & Hardware Matrix

| Series Family | Supported Models | PPD Profile | Max Resolution | Duplex / Trays |
|---|---|---|---|---|
| **Ricoh SP 100 Series** | SP 100, SP 100SU, SP 100SF | [`ppd/ricoh-sp100.ppd`](ppd/ricoh-sp100.ppd) | 600 DPI | Manual |
| **Ricoh SP 110 Series** | SP 110, SP 111, SP 112 | [`ppd/ricoh-sp111.ppd`](ppd/ricoh-sp111.ppd) | 1200x600 DPI | Manual |
| **Ricoh SP 150 Series** | SP 150, SP 150SU, SP 150w | [`ppd/ricoh-sp150.ppd`](ppd/ricoh-sp150.ppd) | 1200x600 DPI | Manual |
| **Ricoh SP 200 Series** | SP 200, SP 201N/NW, SP 202, SP 203, SP 204S/SF | [`ppd/ricoh-sp200.ppd`](ppd/ricoh-sp200.ppd) | 1200x600 DPI | Manual |
| **Ricoh SP 210 Series** | SP 210, SP 211, SP 212, SP 213w | [`ppd/ricoh-sp210.ppd`](ppd/ricoh-sp210.ppd) | 1200x600 DPI | Manual |
| **Ricoh SP 230 Series** | SP 230DNw, SP 230SFNw | [`ppd/ricoh-sp230.ppd`](ppd/ricoh-sp230.ppd) | 1200x600 DPI | Auto Duplex |
| **Ricoh SP 310 / 325 / 3710** | SP 310DN, SP 311DN/SFN, SP 325DNw, SP 3710DN | [`ppd/ricoh-sp310.ppd`](ppd/ricoh-sp310.ppd) | 1200x600 DPI | Auto Duplex + Multi-Tray |
| **OEM Clones** | Equivalent Gestetner, Lanier, Savin, Nashuatec models | Compatible PPD | Match Base | Match Base |

---

## Supported Operating Systems

- **Linux**: Debian, Ubuntu, Linux Mint, Raspberry Pi OS (ARM32/ARM64), Fedora, RHEL, AlmaLinux, Arch Linux, Manjaro
- **macOS**: macOS Sonoma, Ventura, Monterey, Big Sur (Apple Silicon M1/M2/M3/M4 & Intel x86_64)

---

## Quick Start (Automated Installation)

Run the included automated multi-platform installer:

```bash
git clone https://github.com/ankitsingh99/Ricoh-SP200-Linux.git
cd Ricoh-SP200-Linux
chmod +x setup.sh test_print.sh uninstall.sh
./setup.sh
```

The installer will:
1. Detect your OS and package manager.
2. Install build and runtime dependencies (`cups`, `jbigkit`, `ghostscript`, `gcc`).
3. Compile the unified `rastertoricohddst` filter binary with native optimizations.
4. Install the complete PPD library and configure macOS CUPS sandbox permissions.
5. Auto-detect the connected Ricoh USB device and register the printer queue.

---

## Prerequisites & Dependencies

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

```bash
# Build the universal filter and backward-compatible binary
make build

# Install filters and all PPD profiles to system directories
sudo make install

# Register printer queue (with auto USB discovery)
sudo make register

# Send a test page
make test

# Uninstall and clean queues
sudo make uninstall
```

---

## Architecture & Technical Protocol

```
                  ┌───────────────────────────────────────────────────────────┐
                  │                 CUPS Print Framework                      │
                  │        (Input: application/vnd.cups-raster)                │
                  └─────────────────────────────┬─────────────────────────────┘
                                                │
                                                ▼
                  ┌───────────────────────────────────────────────────────────┐
                  │          rastertoricohddst (Native C Filter)              │
                  ├───────────────────────────────────────────────────────────┤
                  │ 1. Model Profile & PPD parser (Tray, Duplex, Resolution)  │
                  │ 2. Color Conversion Engine (1-bit Monochrome)             │
                  │ 3. ITU-T T.82 JBIG1 Compression via libjbig               │
                  │ 4. PJL Stream Builder & Dynamic Dot Counter               │
                  └─────────────────────────────┬─────────────────────────────┘
                                                │
                                                ▼
                  ┌───────────────────────────────────────────────────────────┐
                  │          USB / Network (IPP / AppSocket) Backend          │
                  │                   Ricoh Laser Printer                     │
                  └───────────────────────────────────────────────────────────┘
```

---

## Repository Structure

| File / Directory | Description |
|---|---|
| [`rastertoricohddst.c`](rastertoricohddst.c) | Native universal C source code for Ricoh DDST filter |
| [`rastertoricohjbig.c`](rastertoricohjbig.c) | Legacy SP 200 filter (maintained for backward compatibility) |
| [`ppd/`](ppd/) | Adobe-compliant PPD library covering SP 100, 110, 150, 200, 210, 230, 310 series |
| [`setup.sh`](setup.sh) | Automated multi-model installer script |
| [`test_print.sh`](test_print.sh) | Single-page diagnostic test print script |
| [`uninstall.sh`](uninstall.sh) | Clean uninstallation script |
| [`Makefile`](Makefile) | Multi-platform build and install system |
| [`.github/workflows/ci.yml`](.github/workflows/ci.yml) | Multi-OS GitHub Actions CI pipeline |
| [`LICENSE`](LICENSE) | MIT License |
| [`CONTRIBUTING.md`](CONTRIBUTING.md) | Contribution & reverse engineering guidelines |
| [`SECURITY.md`](SECURITY.md) | Security vulnerability disclosure policy |

---

## Contributing

We welcome reports on new printer models, packet captures, and pull requests! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for full details.

---

## License

This project is licensed under the [MIT License](LICENSE).
