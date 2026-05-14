/*
 * rastertoricohjbig.c — CUPS raster filter for Ricoh SP 200
 *
 * Converts CUPS raster input to the Ricoh SP 200 PJL+JBIG1 protocol.
 * Protocol reverse engineered from USB captures of the Windows driver.
 *
 * Build:
 *   gcc -O2 -o rastertoricohjbig rastertoricohjbig.c \
 *       $(cups-config --cflags --libs) -lcupsimage -ljbig
 *
 * Install:
 *   sudo cp rastertoricohjbig /usr/lib/cups/filter/
 *   sudo chmod 755 /usr/lib/cups/filter/rastertoricohjbig
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <jbig.h>
#include <cups/cups.h>
#include <cups/raster.h>

 /* JBIG encoding parameters — derived from BIE header in USB capture */
#define JBIG_L0       128           /* lines per stripe                  */
#define JBIG_ORDER    (JBG_HITOLO | JBG_SEQ)   /* 0x03                  */
#define JBIG_OPTIONS  (JBG_TPDON | JBG_DPPRIV)  // 0x48 (matches Windows capture exactly)

/* ------------------------------------------------------------------ */
/* Growable output buffer for jbg_enc_out callback                    */
/* ------------------------------------------------------------------ */
typedef struct {
    unsigned char* data;
    size_t         size;
    size_t         cap;
} Buf;

static void buf_cb(unsigned char* d, size_t n, void* arg)
{
    Buf* b = arg;
    if (b->size + n > b->cap) {
        b->cap = (b->size + n) * 2;
        b->data = realloc(b->data, b->cap);
    }
    memcpy(b->data + b->size, d, n);
    b->size += n;
}

/* ------------------------------------------------------------------ */
/* Write the PJL job header to stdout                                  */
/* ------------------------------------------------------------------ */
static void write_job_header(int copies)
{
    time_t     now = time(NULL);
    struct tm* t = localtime(&now);

    /* PJL Universal Exit Language escape, followed by bare @PJL marker */
    fputs("\x1b%-12345X@PJL\r\n", stdout);

    fprintf(stdout,
        "@PJL SET TIMESTAMP=%04d/%02d/%02d %02d:%02d:%02d\r\n"
        "@PJL SET FILENAME=printjob\r\n"
        "@PJL SET COMPRESS=JBIG\r\n"
        "@PJL SET USERNAME=lp\r\n"
        "@PJL SET COVER=OFF\r\n"
        "@PJL SET HOLD=OFF\r\n"
        "@PJL SET PAGESTATUS=START\r\n"
        "@PJL SET COPIES=%d\r\n"
        "@PJL SET MEDIASOURCE=TRAY1\r\n"
        "@PJL SET MEDIATYPE=PLAINRECYCLE\r\n",
        t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
        t->tm_hour, t->tm_min, t->tm_sec,
        copies);
}

/* ------------------------------------------------------------------ */
/* Encode one raster page as a JBIG1 BIE and write to stdout          */
/* ------------------------------------------------------------------ */
/* Count set bits (black pixels) in a 1-bit packed bitmap */
static unsigned long count_dots(const unsigned char* bmp, unsigned w, unsigned h)
{
    unsigned stride = (w + 7) / 8;
    unsigned long n = 0;
    for (unsigned y = 0; y < h; y++)
        for (unsigned x = 0; x < stride; x++)
            n += __builtin_popcount(bmp[y * stride + x]);
    return n;
}

static unsigned long write_page(unsigned char* bmp, unsigned w, unsigned h,
    const char* paper, int first_page, unsigned dpi)
{
    Buf buf = { malloc(1 << 17), 0, 1 << 17 };
    struct jbg_enc_state enc;
    unsigned char* planes[1] = { bmp };

    if (first_page) {
        fprintf(stdout,
            "@PJL SET PAPER=%s\r\n"
            "@PJL SET PAPERWIDTH=%u\r\n"
            "@PJL SET PAPERLENGTH=%u\r\n"
            "@PJL SET RESOLUTION=%u\r\n",
            paper, w, h, dpi);
    }

    /* JBIG1 encode */
    jbg_enc_init(&enc, w, h, 1, planes, buf_cb, &buf);
    /* options=0x48 = JBG_LRLTWO(0x40) | JBG_TPBON(0x08) in jbigkit 2.1 constants.
     * jbigkit stores this value directly into BIE byte 19, which must match
     * the Windows driver's output exactly (also 0x48). Using 0x08 here caused
     * all-black output because the encoder used the 3-pixel template but the
     * header declared the 2-pixel template (LRLTWO), confusing the printer's
     * decoder.                                                                */
    jbg_enc_options(&enc,
        0x03,   /* order:   stored directly as BIE byte 18 */
        0x48,   /* options: JBG_LRLTWO|JBG_TPBON — matches Windows BIE byte 19 */
        128,    /* L0:      lines per stripe */
        0,      /* mx */
        0);     /* my */
    jbg_enc_out(&enc);
    jbg_enc_free(&enc);

    if (buf.size >= 20)
        fprintf(stderr, "DEBUG: BIE byte18=0x%02x byte19=0x%02x\n",
                buf.data[18], buf.data[19]);

    unsigned long dots = count_dots(bmp, w, h);

    /* Write IMAGELEN declaration then the BIE */
    fprintf(stdout, "@PJL SET IMAGELEN=%zu\r\n", buf.size);
    fwrite(buf.data, 1, buf.size, stdout);
    fflush(stdout);

    free(buf.data);
    return dots;
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */
int main(int argc, char* argv[])
{
    FILE* dbg = fopen("/tmp/ricoh_filter_debug.log", "w");
    if (dbg) {
        fprintf(dbg, "Filter invoked! argc=%d\n", argc);
        for (int i = 0; i < argc; i++)
            fprintf(dbg, "  argv[%d]=%s\n", i, argv[i]);
        fflush(dbg);
    }

    /* CUPS filter argv: job-id user title copies options [filename] */
    int copies = (argc > 4) ? atoi(argv[4]) : 1;
    if (copies < 1) copies = 1;

    cups_raster_t* ras = cupsRasterOpen(0, CUPS_RASTER_READ);
    cups_page_header2_t hdr;
    int page = 0;
    unsigned long total_dots = 0;

    write_job_header(copies);

    while (cupsRasterReadHeader2(ras, &hdr)) {
        unsigned w      = hdr.cupsWidth;
        unsigned h      = hdr.cupsHeight;
        unsigned bpc    = hdr.cupsBitsPerColor;
        unsigned bpp    = hdr.cupsBitsPerPixel;
        unsigned cspace = hdr.cupsColorSpace;
        unsigned dpi    = hdr.HWResolution[0];

        /* Log the raster header so we know exactly what CUPS sent */
        if (dbg) {
            fprintf(dbg, "\n--- page %d ---\n", page + 1);
            fprintf(dbg, "  Width=%u  Height=%u  DPI=%u\n", w, h, dpi);
            fprintf(dbg, "  ColorSpace=%u  BitsPerColor=%u  BitsPerPixel=%u\n",
                    cspace, bpc, bpp);
            fprintf(dbg, "  PageSize=[%u %u]  NumColors=%u\n",
                    hdr.PageSize[0], hdr.PageSize[1], hdr.cupsNumColors);
            fflush(dbg);
        }

        /* stride depends on what CUPS actually delivers */
        unsigned stride = (bpp * w + 7) / 8;

        /* Determine paper name */
        const char* paper = "A4";
        if (hdr.PageSize[0] > 610) paper = "LETTER";

        /* Read full raster page */
        unsigned char* bmp = malloc(stride * h);
        if (!bmp) {
            fputs("rastertoricohjbig: out of memory\n", stderr);
            if (dbg) { fprintf(dbg, "ERROR: out of memory\n"); fclose(dbg); }
            return 1;
        }
        for (unsigned y = 0; y < h; y++)
            cupsRasterReadPixels(ras, bmp + y * stride, stride);

        /* If CUPS delivered multi-bit gray instead of 1-bit, convert it.
         * threshold: pixel >= 128 → white (0), < 128 → black (1) */
        unsigned char* bmp1 = bmp;
        if (bpp > 1) {
            unsigned stride1 = (w + 7) / 8;
            bmp1 = calloc(stride1, h);
            if (!bmp1) {
                fputs("rastertoricohjbig: out of memory (1-bit buf)\n", stderr);
                free(bmp);
                if (dbg) { fprintf(dbg, "ERROR: out of memory (1-bit)\n"); fclose(dbg); }
                return 1;
            }
            for (unsigned y = 0; y < h; y++) {
                for (unsigned x = 0; x < w; x++) {
                    unsigned char px;
                    if (bpp == 8)
                        px = bmp[y * stride + x];
                    else  /* 24-bit RGB: average */
                        px = ((unsigned)bmp[y*stride + x*3] +
                              bmp[y*stride + x*3+1] +
                              bmp[y*stride + x*3+2]) / 3;
                    /* K=0 means white in CUPS_CSPACE_K; invert for W spaces */
                    int black = (cspace == 3) ? (px > 128) : (px < 128);
                    if (black)
                        bmp1[y * stride1 + x/8] |= (0x80 >> (x & 7));
                }
            }
            if (dbg) {
                fprintf(dbg, "  converted %u-bit → 1-bit (stride %u→%u)\n",
                        bpp, stride, (w+7)/8);
                fflush(dbg);
            }
            stride = (w + 7) / 8;
        }

        total_dots += write_page(bmp1, w, h, paper, page == 0, dpi);
        if (bpp > 1) free(bmp1);
        free(bmp);

        page++;
        fprintf(stderr, "PAGE: %d %d\n", page, copies);
        if (dbg) { fprintf(dbg, "  page %d encoded and sent\n", page); fflush(dbg); }
    }

    cupsRasterClose(ras);

    if (dbg) {
        fprintf(dbg, "\nDone. Total pages=%d  total_dots=%lu\n", page, total_dots);
        fclose(dbg);
    }

    /* End-of-job sequence — must match Windows driver exactly:
     *   DOTCOUNT  = total black pixels across all pages
     *   PAGESTATUS=END triggers page ejection in the firmware
     *   EOJ       = standard PJL end-of-job
     *   UEL       = universal exit language (with trailing CRLF as in capture) */
    fprintf(stdout,
        "@PJL SET DOTCOUNT=%lu\r\n"
        "@PJL SET PAGESTATUS=END\r\n"
        "@PJL EOJ\r\n"
        "\x1b%%-12345X\r\n",
        total_dots);
    fflush(stdout);

    return 0;
}