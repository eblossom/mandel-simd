#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <getopt.h>
#include "mandel.h"

void mandel_basic(unsigned char *image, const struct spec *s);
void mandel_altivec(unsigned char *image, const struct spec *s);
void mandel_avx(unsigned char *image, const struct spec *s);
void mandel_sse2(unsigned char *image, const struct spec *s);
void mandel_neon(unsigned char *image, const struct spec *s);

void
mandel_basic(unsigned char *image, const struct spec *s)
{
    float xscale = (s->xlim[1] - s->xlim[0]) / s->width;
    float yscale = (s->ylim[1] - s->ylim[0]) / s->height;
    #pragma omp parallel for schedule(dynamic, 1)
    for (int y = 0; y < s->height; y++) {
        for (int x = 0; x < s->width; x++) {
            float cr = x * xscale  + s->xlim[0];
            float ci = y * yscale + s->ylim[0];
            float zr = cr;
            float zi = ci;
            int k = 0;
            float mk = 0.0f;
            while (++k < s->iterations) {
                float zr1 = zr * zr - zi * zi + cr;
                float zi1 = zr * zi + zr * zi + ci;
                zr = zr1;
                zi = zi1;
                mk += 1.0f;
                if (zr * zr + zi * zi >= 4.0f)
                    break;
            }
            int pixel = mk;
	    // netpgm - portable grayscale image format.
	    // 2 byte version is big endian.
	    if (s->depth > 256) {
	        image[y * s->width * 2 + x * 2 + 0] = pixel >> 8;
		image[y * s->width * 2 + x * 2 + 1] = pixel;
            }
	    else {
	        image[y * s->width + x] = pixel;
	    }
        }
    }
}

#ifdef __x86_64__
#include <cpuid.h>

static inline int
is_avx_supported(void)
{
    unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;
    __get_cpuid(1, &eax, &ebx, &ecx, &edx);
    return ecx & bit_AVX ? 1 : 0;
}
#endif // __x86_64__

int
main(int argc, char *argv[])
{
    /* Config */
    struct spec spec = {
        .width = 1440,
        .height = 1080,
        .depth = 256,
        .xlim = {-2.5, 1.5},
        .ylim = {-1.5, 1.5},
        .iterations = 256
    };

    #ifdef __x86_64__
    int use_avx = 1;
    int use_sse2 = 1;
    const char *optstring = "w:h:d:k:x:y:AS";
    #endif // __x86_64__

    #if defined(__arm__) || defined(__aarch64__)
    int use_neon = 1;
    const char *optstring = "w:h:d:k:x:y:N";
    #endif // __arm__ || __aarch64__

    #ifdef __ppc__
    int use_altivec = 1;
    const char *optstring = "w:h:d:k:x:y:A";
    #endif // __ppc__

    /* Parse Options */
    int option;
    while ((option = getopt(argc, argv, optstring)) != -1) {
        switch (option) {
            case 'w':
                spec.width = atoi(optarg);
                break;
            case 'h':
                spec.height = atoi(optarg);
                break;
#if 0
            case 'd':
                spec.depth = atoi(optarg);
                break;
#endif
            case 'k':
                spec.iterations = atoi(optarg);
                break;
            case 'x':
                sscanf(optarg, "%f:%f", &spec.xlim[0], &spec.xlim[1]);
                break;
            case 'y':
                sscanf(optarg, "%f:%f", &spec.ylim[0], &spec.ylim[1]);
                break;

            #ifdef __x86_64__
            case 'A':
                use_avx = 0;
                break;
            case 'S':
                use_sse2 = 0;
                break;
            #endif // __x86_64__

            #if defined(__arm__) || defined(__aarch64__)
            case 'N':
                use_neon = 0;
                break;
            #endif // __arm__ || __aarch64__

            #ifdef __ppc__
            case 'A':
                use_altivec = 0;
                break;
            #endif // __ppc__

            default:
                exit(EXIT_FAILURE);
                break;
        }
    }

    spec.depth = spec.iterations;
    if (spec.iterations < 0 || spec.iterations >= 65536){
        fprintf(stderr, "iterations must be > 0 and < 65536\n");
        exit(1);
    }

    /* Render */

    size_t nbytes = spec.width * spec.height;
    if (spec.depth > 256)
        nbytes *= 2;

    unsigned char *image = malloc(nbytes);

    #ifdef __x86_64__
    if (0 && use_avx && is_avx_supported())
        mandel_avx(image, &spec);
    else if (0 && use_sse2)
        mandel_sse2(image, &spec);
    #endif // __x86_64__

    #if defined(__arm__) || defined(__aarch64__)
    if (use_neon)
        mandel_neon(image, &spec);
    #endif // __arm__ || __aarch64__

    #ifdef __ppc__
    if (use_altivec)
        mandel_altivec(image, &spec);
    #endif // __ppc__

    else
        mandel_basic(image, &spec);

    /* Write result */
    fprintf(stdout, "P5\n%d %d\n%d\n", spec.width, spec.height, spec.depth - 1);
    fwrite(image, spec.width * spec.height, spec.depth > 256 ? 2 : 1, stdout);
    free(image);

    return 0;
}
