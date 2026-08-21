# Contributing to Ricoh SP 200 Driver

Thank you for your interest in improving the Ricoh SP 200 CUPS Driver! Community contributions help keep this driver working across diverse Linux distributions, macOS versions, and related printer models.

---

## Code of Conduct

Please note that this project is released with a [Contributor Code of Conduct](CODE_OF_CONDUCT.md). By participating in this project you agree to abide by its terms.

---

## How Can I Contribute?

### 1. Reporting Bugs
Before submitting an issue, please check existing issues to avoid duplicates. When opening an issue, provide:
- Your Operating System and architecture (`uname -a`).
- CUPS version (`cups-config --version` or `cupsd --version`).
- Printer model (e.g., Ricoh SP 200, SP 201N, SP 202, SP 203, SP 204, SP 210).
- Connection type (USB or Network).
- Relevant CUPS log output from `/var/log/cups/error_log` (or `cupsctl --debug-logging` enabled logs).

### 2. Testing Other Printer Models
If you own a similar Ricoh DDST printer (such as Ricoh SP 201, SP 202, SP 203, SP 204, SP 210, SP 211, SP 212, SP 213, SP 311), test the driver and let us know your results!
- If the printer works or needs tweaks, open an issue or PR to update the compatibility list.
- If you have USB packet captures from the Windows driver, see [Capturing USB Traffic](#capturing-usb-traffic) below.

### 3. Submitting Pull Requests
1. **Fork the repository** and clone your fork locally.
2. **Create a feature branch** from `main`:
   ```bash
   git checkout -b feature/my-feature-name
   ```
3. **Make your changes** following our coding standards.
4. **Test your changes**:
   - Verify PPD syntax:
     ```bash
     cupstestppd ricoh-sp200.ppd
     ```
   - Compile filter cleanly with warnings enabled:
     ```bash
     make clean && make build
     ```
   - Test shell scripts for syntax:
     ```bash
     bash -n setup.sh test_print.sh uninstall.sh
     ```
5. **Commit with descriptive commit messages** (e.g. Conventional Commits format `feat: ...`, `fix: ...`, `docs: ...`).
6. **Push to your fork** and submit a Pull Request.

---

## Development & Build Environment

### Prerequisites

- **C Compiler**: GCC or Clang (supporting C99).
- **Libraries**:
  - `libcups` and `libcupsimage`
  - `jbigkit` (`libjbig`)
- **Utilities**: `make`, `cups`, `ghostscript`.

### Building Locally

```bash
# Build filter binary
make build

# Validate PPD syntax
cupstestppd ricoh-sp200.ppd

# Clean build artifacts
make clean
```

---

## Coding Standards

### C Source (`rastertoricohjbig.c`)
- Adhere to **C99** standards.
- Follow CUPS filter conventions:
  - Read input from file descriptor passed in `argv[6]` if provided, or from stdin (`0`).
  - Output page status messages (`PAGE: page-number num-copies`) to `stderr`.
  - Output diagnostic messages (`DEBUG:`, `INFO:`, `ERROR:`) to `stderr`.
  - Write binary PJL / JBIG1 payload to `stdout`.
- Check all memory allocations and return codes safely.
- Avoid memory leaks.

### Shell Scripts (`*.sh`)
- Use portable POSIX/Bash syntax.
- Always quote variables properly (e.g. `"$VAR"`).
- Handle error conditions cleanly and provide informative output.

---

## Capturing USB Traffic (For Protocol Reverse Engineering)

If you are investigating unsupported features (e.g., duplexing, manual tray, resolution modes):
1. On a Windows machine running the official Ricoh driver, use **Wireshark** with **USBPcap** or **USBlyzer**.
2. Capture traffic while sending a 1-page document with known contents.
3. Save the `.pcap` capture and inspect PJL command headers and JBIG compression parameters (BIE header bytes).
