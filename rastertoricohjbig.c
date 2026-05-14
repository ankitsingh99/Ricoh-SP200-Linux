/*
 * rastertoricohjbig.c — CUPS raster filter for Ricoh SP 200
 *
 * Converts CUPS raster input to the Ricoh SP 200 PJL+JBIG1 protocol.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <jbig.h>
#include <cups/cups.h>
#include <cups/raster.h>

 /* Growable output buffer used as the jbg_enc_out callback target */
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

static void write_job_header(int copies)
{
    time_t     now = time(NULL);
    struct tm* t = localtime(&now);

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

/* Count black pixels across a 1-bit packed bitmap (for DOTCOUNT). */
static unsigned long count_dots(const unsigned char* bmp, unsigned w, unsigned h)
{
    unsigned stride = (w + 7) / 8;
    unsigned long n = 0;
    for (unsigned y = 0; y < h; y++)
        for (unsigned x = 0; x < stride; x++)
            n += __builtin_popcount(bmp[y * stride + x]);
    return n;
}

/*
 * Encode and send one page. Protocol (from USB capture of Windows driver):
 *
 *   For page 1:  job header already emitted PAGESTATUS=START + COPIES + MEDIA.
 *   For page N>1: we emit PAGESTATUS=END (closes page N-1) then the full
 *                 per-page setup block before this page's IMAGELEN.
 *
 *   After every page's JBIG data: DOTCOUNT + PAGESTATUS=END.
 *   After the final page: caller emits EOJ + UEL.
 *
 *   Without the per-page PAGESTATUS=END the firmware treats every subsequent
 *   IMAGELEN as another chunk of the same page and never ejects it.
 */
static void write_page(unsigned char* bmp, unsigned w, unsigned h,
    const char* paper, int first_page,
    unsigned dpi, int copies)
{
    Buf buf = { malloc(1 << 17), 0, 1 << 17 };
    struct jbg_enc_state enc;
    unsigned char* planes[1] = { bmp };

    /* Pages 2+ need their own PAGESTATUS=START + media header.
     * Page 1 inherits these from the job header. */
    if (!first_page) {
        fprintf(stdout,
            "@PJL SET PAGESTATUS=START\r\n"
            "@PJL SET COPIES=%d\r\n"
            "@PJL SET MEDIASOURCE=TRAY1\r\n"
            "@PJL SET MEDIATYPE=PLAINRECYCLE\r\n",
            copies);
    }

    /* PAPERWIDTH and PAPERLENGTH are both required and are repeated for every
     * page (not just the first). Omitting PAPERLENGTH causes the printer to
     * initialise but never feed the page. */
    fprintf(stdout,
        "@PJL SET PAPER=%s\r\n"
        "@PJL SET PAPERWIDTH=%u\r\n"
        "@PJL SET PAPERLENGTH=%u\r\n"
        "@PJL SET RESOLUTION=%u\r\n",
        paper, w, h, dpi);

    jbg_enc_init(&enc, w, h, 1, planes, buf_cb, &buf);
    /* jbigkit 2.1 stores the order and options arguments directly into BIE
     * bytes 18 and 19. Both must match the Windows driver's output exactly:
     * byte 18 = 0x03, byte 19 = 0x48. Sending byte 19 = 0x08 causes the
     * printer to warm up but refuse to pull the page. */
    jbg_enc_options(&enc, 0x03, 0x48, 128, 0, 0);
    jbg_enc_out(&enc);
    jbg_enc_free(&enc);

    fprintf(stdout, "@PJL SET IMAGELEN=%zu\r\n", buf.size);
    fwrite(buf.data, 1, buf.size, stdout);

    /* DOTCOUNT + PAGESTATUS=END must follow every page's JBIG data.
     * This triggers paper ejection. Without it the printer accumulates all
     * subsequent IMAGELEN blocks as extra chunks of the same page. */
    unsigned long dots = count_dots(bmp, w, h);
    fprintf(stdout,
        "@PJL SET DOTCOUNT=%lu\r\n"
        "@PJL SET PAGESTATUS=END\r\n",
        dots);
    fflush(stdout);

    free(buf.data);
}

int main(int argc, char* argv[])
{
    int copies = (argc > 4) ? atoi(argv[4]) : 1;
    if (copies < 1) copies = 1;

    cups_raster_t* ras = cupsRasterOpen(0, CUPS_RASTER_READ);
    cups_page_header2_t hdr;
    int                 page = 0;

    write_job_header(copies);

    while (cupsRasterReadHeader2(ras, &hdr)) {
        unsigned w = hdr.cupsWidth;
        unsigned h = hdr.cupsHeight;
        unsigned bpp = hdr.cupsBitsPerPixel;
        unsigned cspace = hdr.cupsColorSpace;
        unsigned dpi = hdr.HWResolution[0];
        /* Use cupsBytesPerLine — not a manual calculation — so that the stream
         * position stays correct across pages regardless of row padding. */
        unsigned stride = hdr.cupsBytesPerLine;

        const char* paper = (hdr.PageSize[0] > 610) ? "LETTER" : "A4";

        unsigned char* bmp = malloc(stride * h);
        if (!bmp) {
            fputs("rastertoricohjbig: out of memory\n", stderr);
            return 1;
        }
        for (unsigned y = 0; y < h; y++)
            cupsRasterReadPixels(ras, bmp + y * stride, stride);

        /* The PPD requests 1-bit K raster, so bpp == 1 in normal operation.
         * This fallback converts 8-bit gray or 24-bit RGB if a different CUPS
         * pipeline delivers it. */
        unsigned char* bmp1 = bmp;
        if (bpp > 1) {
            unsigned stride1 = (w + 7) / 8;
            bmp1 = calloc(stride1, h);
            if (!bmp1) {
                fputs("rastertoricohjbig: out of memory\n", stderr);
                free(bmp);
                return 1;
            }
            for (unsigned y = 0; y < h; y++) {
                for (unsigned x = 0; x < w; x++) {
                    unsigned char px;
                    if (bpp == 8)
                        px = bmp[y * stride + x];
                    else  /* 24-bit RGB → luminance average */
                        px = ((unsigned)bmp[y * stride + x * 3] +
                            bmp[y * stride + x * 3 + 1] +
                            bmp[y * stride + x * 3 + 2]) / 3;
                    /* CUPS_CSPACE_K (3): 0 = white, 1 = black.
                     * All other spaces: 0 = black, 255 = white. */
                    int black = (cspace == 3) ? (px > 128) : (px < 128);
                    if (black)
                        bmp1[y * stride1 + x / 8] |= (0x80 >> (x & 7));
                }
            }
        }

        write_page(bmp1, w, h, paper, page == 0, dpi, copies);
        if (bpp > 1) free(bmp1);
        free(bmp);

        page++;
        fprintf(stderr, "PAGE: %d %d\n", page, copies);
    }

    cupsRasterClose(ras);

    fputs("@PJL EOJ\r\n\x1b%-12345X\r\n", stdout);
    fflush(stdout);

    return 0;
}
