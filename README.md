# Ricoh SP 200 Linux CUPS Driver

A native Linux CUPS filter for the Ricoh SP 200 monochrome laser printer, reverse engineered from USB captures of the official Windows driver.

The printer has no official Linux driver and is absent from foo2zjs, OpenPrinting, HPLIP, and Gutenprint. This driver implements the complete print protocol from scratch.

---

## Files

| File | Purpose |
|---|---|
| `rastertoricohjbig.c` | CUPS filter source |
| `ricoh-sp200.ppd` | PPD printer description |

---

## Dependencies

**Arch Linux**
```bash
sudo pacman -S cups ghostscript jbigkit
```

**Debian / Ubuntu**
```bash
sudo apt install libcups2-dev libcupsimage2-dev libjbig-dev jbigkit-bin gcc ghostscript
```

**Fedora / RHEL**
```bash
sudo dnf install cups-devel cups-libs jbigkit-devel jbigkit-libs gcc ghostscript
```

---

## Build and Install

### Using make (recommended)

```bash
# Build and install the filter + PPD
sudo make install

# Register the printer with CUPS (printer must be plugged in via USB)
sudo make register

# Test print
echo "Hello from Linux" | lpr -P RicohSP200

# To remove everything
sudo make uninstall
```

### Manual steps

```bash
# Compile
gcc -O2 -Wall -Wextra -o rastertoricohjbig rastertoricohjbig.c \
    $(cups-config --libs) -lcupsimage -ljbig

# Install filter
sudo install -m 755 rastertoricohjbig /usr/lib/cups/filter/rastertoricohjbig

# Install PPD
sudo install -m 644 ricoh-sp200.ppd /usr/share/ppd/cupsfilters/

# Register printer with CUPS (printer must be plugged in via USB)
sudo lpadmin -p RicohSP200 \
    -v "$(lpinfo -v | grep -i ricoh | awk '{print $2}' | head -1)" \
    -P /usr/share/ppd/cupsfilters/ricoh-sp200.ppd \
    -E

# Test print
echo "Hello from Linux" | lpr -P RicohSP200
```

---

## How It Was Reverse Engineered

### 1. Capturing USB Traffic

The printer was passed through to a Windows VM via USB. On the Linux host, `usbmon` + Wireshark captured all USB bulk transfers while printing a test document.

```
# Wireshark filter used:
usb.transfer_type == 0x03 && usb.endpoint_address.direction == OUT
```

A single-page job produced two `URB_BULK out` packets (~65 KB and ~59 KB). A two-page job produced three packets, revealing that the Windows driver chunks the JBIG stream across as many USB bulk transfers as needed (each ≤ 65 508 bytes).

### 2. Identifying the Protocol

The first bytes of packet 1 decoded as ASCII:

```
ESC%-12345X@PJL
@PJL SET COMPRESS=JBIG
@PJL SET PAPERWIDTH=4961
@PJL SET IMAGELEN=65556
```

This immediately identified the protocol: **PJL (Printer Job Language)** wrapping **JBIG1 (ITU-T T.82)** compressed raster data. The initial hypothesis that the printer used HBPL2 (like other foo2zjs Ricoh printers) was wrong — a grep for `HBPL` found nothing in the stream.

### 3. Decoding the JBIG BIE Header

The 20 bytes following the PJL header were initially assumed to be a proprietary Ricoh header. They turned out to be a standard **JBIG1 BIE (Bi-level Image Entity) header**:

| Offset | Value | Field | Meaning |
|---|---|---|---|
| 0–3 | `00 00 01 00` | DL, D, P, reserved | Layer 0, 0 diffs, 1 plane |
| 4–7 | `00 00 13 61` | Xd | **4961 px** = A4 width @ 600 dpi |
| 8–11 | `00 00 1b 68` | Yd | **7016 px** = A4 height @ 600 dpi |
| 12–15 | `00 00 00 80` | L0 | 128 lines per stripe |
| 16–17 | `00 00` | Mx, Dmax | 0 |
| 18 | `03` | order | stored directly in BIE byte 18 |
| 19 | `48` | options | stored directly in BIE byte 19 |

### 4. Discovering the Multi-Page Protocol

A second capture was made printing a two-page document. Extracting all `@PJL` tokens from the concatenated USB stream revealed the complete page lifecycle:

```
[page 1 JBIG data — first chunk]
@PJL SET IMAGELEN=13451          ← second chunk of page 1 (driver splits large pages)
[13451 bytes — continuation of page 1 JBIG stream]
@PJL SET DOTCOUNT=1477935        ← black pixel count for page 1
@PJL SET PAGESTATUS=END          ← eject page 1
@PJL SET PAGESTATUS=START        ← begin page 2
@PJL SET COPIES=1
@PJL SET MEDIASOURCE=TRAY1
@PJL SET MEDIATYPE=PLAINRECYCLE
@PJL SET PAPER=A4                ← page 2 media settings (repeated every page)
@PJL SET PAPERWIDTH=4961
@PJL SET PAPERLENGTH=7016
@PJL SET RESOLUTION=600
@PJL SET IMAGELEN=65556          ← page 2 first chunk
[65556 bytes — page 2 JBIG data, first chunk]
@PJL SET IMAGELEN=10550          ← page 2 second chunk
[10550 bytes — page 2 JBIG continuation]
@PJL SET DOTCOUNT=1266291
@PJL SET PAGESTATUS=END          ← eject page 2
@PJL EOJ
ESC%-12345X
```

Key findings:
- **`PAGESTATUS=END` is the page eject trigger.** Without it the firmware treats every subsequent `IMAGELEN` as another chunk of the still-open page and never ejects.
- **Each page may span multiple `IMAGELEN` chunks.** The driver splits any JBIG stream that exceeds a USB bulk transfer into consecutive `@PJL SET IMAGELEN` blocks, all belonging to the same page. The Linux filter produces smaller JBIG streams (no chunking needed) but the firmware handles both.
- **Every page except the first gets its own `PAGESTATUS=START` + full media header** (`COPIES`, `MEDIASOURCE`, `MEDIATYPE`, `PAPER`, `PAPERWIDTH`, `PAPERLENGTH`, `RESOLUTION`) before its first `IMAGELEN`.
- **`DOTCOUNT`** reports the total black pixels for the page and is used by the printer for toner life estimation.

### 5. Complete Protocol Structure

```
── Job header (once) ──────────────────────────────────────────────────────────
ESC%-12345X@PJL\r\n
@PJL SET TIMESTAMP=YYYY/MM/DD HH:MM:SS\r\n
@PJL SET FILENAME=...\r\n
@PJL SET COMPRESS=JBIG\r\n
@PJL SET USERNAME=...\r\n
@PJL SET COVER=OFF\r\n
@PJL SET HOLD=OFF\r\n
@PJL SET PAGESTATUS=START\r\n      ← covers page 1
@PJL SET COPIES=N\r\n
@PJL SET MEDIASOURCE=TRAY1\r\n
@PJL SET MEDIATYPE=PLAINRECYCLE\r\n

── Per-page block (repeat for every page) ─────────────────────────────────────
  [pages 2+ only:]
  @PJL SET PAGESTATUS=START\r\n
  @PJL SET COPIES=N\r\n
  @PJL SET MEDIASOURCE=TRAY1\r\n
  @PJL SET MEDIATYPE=PLAINRECYCLE\r\n

  @PJL SET PAPER=<A4|LETTER>\r\n
  @PJL SET PAPERWIDTH=<px>\r\n
  @PJL SET PAPERLENGTH=<px>\r\n
  @PJL SET RESOLUTION=600\r\n
  @PJL SET IMAGELEN=<N>\r\n
  <N bytes: JBIG1 BIE header + compressed raster>
  @PJL SET DOTCOUNT=<page_black_pixels>\r\n
  @PJL SET PAGESTATUS=END\r\n      ← triggers paper ejection

── End of job (once) ──────────────────────────────────────────────────────────
@PJL EOJ\r\n
ESC%-12345X\r\n
```

### 6. Building the CUPS Filter

The filter (`rastertoricohjbig.c`) uses:
- **libcups / libcupsimage** — reads the CUPS raster stream from stdin
- **libjbig** (jbigkit 2.1) — encodes each page as a JBIG1 BIE
- The encoded BIE is buffered in memory, its size measured, then emitted to stdout preceded by `@PJL SET IMAGELEN=N`

The PPD declares `*cupsBitsPerColor: 1` and `*cupsColorSpace: 3` so CUPS delivers a 1-bit packed K-channel bitmap directly — no conversion needed in the normal path.

### 7. Bugs Found During Development

**Bug 1 — Missing bare `@PJL\r\n` after UEL**
The Windows driver emits `ESC%-12345X@PJL\r\n` before the first SET command. Without the bare `@PJL\r\n` line the printer silently discards the entire job.

**Bug 2 — Missing `@PJL SET PAPERLENGTH`**
The printer requires both `PAPERWIDTH` and `PAPERLENGTH`. Without `PAPERLENGTH` the printer initialises its engine (noise + LED blink) but never pulls the page.

**Bug 3 — Wrong JBIG BIE options byte**
jbigkit 2.1 stores the `options` argument to `jbg_enc_options()` **directly** into BIE byte 19 with no bit translation. The Windows driver produces byte 19 = `0x48`. Passing `0x08` caused the printer to accept the job, warm up, and then refuse to feed the page. Passing `0x48` fixed it.

**Bug 4 — PPD lacked raster format directives**
Without `*cupsBitsPerColor: 1`, `*cupsColorSpace: 3`, and related PPD keys, CUPS delivered 8-bit grayscale to the filter. The stride was calculated as `(w+7)/8` (1-bit assumption) but the actual data was 8× wider, producing a malformed JBIG stream.

**Bug 5 — Multi-page: only first page printed**
Discovered by capturing a two-page job from the Windows driver. The firmware uses `@PJL SET PAGESTATUS=END` as the page eject trigger — not the arrival of the next `IMAGELEN`. Without `PAGESTATUS=END` after each page's JBIG data, every subsequent `IMAGELEN` block was treated as another chunk of the same open page. The printer accumulated all pages' data as one giant page 1 and never ejected it. Three additional omissions were found in the same capture:
- `PAGESTATUS=START` + full media header (`COPIES`, `MEDIASOURCE`, `MEDIATYPE`, `PAPER`, `PAPERWIDTH`, `PAPERLENGTH`, `RESOLUTION`) must precede each page except the first.
- `DOTCOUNT` (black pixel count) must be emitted per page, immediately before `PAGESTATUS=END`.
- `cupsBytesPerLine` from the CUPS raster header must be used as the row stride when reading pixel data — a manual `(bpp*w+7)/8` calculation can diverge from CUPS's actual row size, misaligning the stream and causing `cupsRasterReadHeader2` to fail on page 2.

---

## Troubleshooting

**Printer not found by `lpinfo -v`**
```bash
lsusb | grep -i ricoh
```

**Filter not invoked (job disappears silently)**
```bash
ls -la /usr/lib/cups/filter/rastertoricohjbig
# Must be: -rwxr-xr-x root root
sudo chmod 755 /usr/lib/cups/filter/rastertoricohjbig
sudo chown root:root /usr/lib/cups/filter/rastertoricohjbig
```

**CUPS shows printer stopped**
```bash
sudo cupsenable RicohSP200
sudo cupsaccept RicohSP200
```

**Check CUPS error log**
```bash
sudo tail -40 /var/log/cups/error_log
```

**Build fails — missing jbig.h**
```bash
# Arch:   sudo pacman -S jbigkit
# Debian: sudo apt install libjbig-dev
# Fedora: sudo dnf install jbigkit-devel
```

---

*Reverse engineered May 2026. No proprietary code used or distributed.*
*Protocol wire format documentation is not subject to copyright.*
