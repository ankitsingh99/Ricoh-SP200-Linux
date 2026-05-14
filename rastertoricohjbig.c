/*
 * rastertoricohjbig.c — CUPS raster filter for Ricoh SP 200
 *
 * Converts CUPS raster input to the Ricoh SP 200 PJL+JBIG1 protocol.
 * Protocol reverse engineered from USB captures of the Windows driver.
 *
 * Build:
 *   gcc -O2 -o rastertoricohjbig rastertoricohjbig.c \
 *       $(cups-config --libs) -lcupsimage -ljbig
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

    /* UEL followed by a bare @PJL line — required by the firmware parser.
     * Without the bare @PJL\r\n the printer silently discards the job. */
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

    jbg_enc_init(&enc, w, h, 1, planes, buf_cb, &buf);
    jbg_enc_options(&enc, 0x03, 0x48, 128, 0, 0);
    jbg_enc_out(&enc);
    jbg_enc_free(&enc);

    fprintf(stdout, "@PJL SET IMAGELEN=%zu\r\n", buf.size);
    fwrite(buf.data, 1, buf.size, stdout);
    fflush(stdout);

    unsigned long dots = count_dots(bmp, w, h);
    free(buf.data);
    return dots;
}

int main(int argc, char* argv[])
{
    int copies = (argc > 4) ? atoi(argv[4]) : 1;
    if (copies < 1) copies = 1;

    cups_raster_t* ras = cupsRasterOpen(0, CUPS_RASTER_READ);
    cups_page_header2_t hdr;
    int                 page = 0;
    unsigned long       total_dots = 0;

    write_job_header(copies);

    while (cupsRasterReadHeader2(ras, &hdr)) {
        unsigned w = hdr.cupsWidth;
        unsigned h = hdr.cupsHeight;
        unsigned bpp = hdr.cupsBitsPerPixel;
        unsigned cspace = hdr.cupsColorSpace;
        unsigned dpi = hdr.HWResolution[0];
        unsigned stride = (bpp * w + 7) / 8;

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

        total_dots += write_page(bmp1, w, h, paper, page == 0, dpi);
        if (bpp > 1) free(bmp1);
        free(bmp);

        page++;
        fprintf(stderr, "PAGE: %d %d\n", page, copies);
    }

    cupsRasterClose(ras);

    fprintf(stdout,
        "@PJL SET DOTCOUNT=%lu\r\n"
        "@PJL SET PAGESTATUS=END\r\n"
        "@PJL EOJ\r\n"
        "\x1b%%-12345X\r\n",
        total_dots);
    fflush(stdout);

    return 0;
}
