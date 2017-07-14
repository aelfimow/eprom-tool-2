#include <stdio.h>
#include <math.h>

#define MAX_VALUES  (65536u)

typedef unsigned int uint32_t;
typedef unsigned short int uint16_t;
typedef unsigned char uint8_t;
typedef double fp64_t;

typedef char Check_uint32_t[sizeof(uint32_t) == 4 ? 1: -1];
typedef char Check_uint16_t[sizeof(uint16_t) == 2 ? 1: -1];
typedef char Check_uint8_t[sizeof(uint8_t) == 1 ? 1: -1];
typedef char Check_fp63_t[sizeof(fp64_t) == 8 ? 1: -1];

int main(int argc, char *argv[])
{
    uint32_t i = 0u;
    const fp64_t step = ((2.0 * M_PI) / static_cast<fp64_t>(MAX_VALUES));
    fp64_t angle = 0.0;
    uint16_t b = 0u;
    uint8_t buffer[2u] = { 0u, 0u };
    FILE* pFile = NULL;

    argc = argc;
    argv = argv;

    /* Open the file */
    pFile = fopen("sinustable.bin", "wb");
    if (NULL == pFile)
    {
        printf("ERROR: Could not open file for writing.\n");
        return -1;
    }

    /* This loop computes sinus values and writes them to the file */
    for (i = 0u; i < MAX_VALUES; ++i)
    {
        /* Compute sinus and scale it to the range [0..+65535] */
        b = static_cast<uint16_t>((32766.0 * sin(angle)) + 32767.0);
        angle += step;

#if 0
        printf("%u: unsigned = 0x%X, unsigned inverted = 0x%X\n", i, b, static_cast<uint16_t>(~b));
#endif

        /* Invert, because the DAC has an inverter */
        b = static_cast<uint16_t>(~b);

        /* Write values to the buffer to write to the file */
        buffer[0u] = static_cast<uint8_t>(b & 0x00FFu);
        buffer[1u] = static_cast<uint8_t>((b >> 8u) & 0x00FFu);

        /* Write to the file */
        if (2 != fwrite(&buffer[0], 1, 2, pFile))
        {
            printf("ERROR: Error writing to file.\n");
            fclose(pFile);
            return -1;
        }

        /* Flush the file */
        if (0 != fflush(pFile))
        {
            printf("ERROR: Error flushing to file.\n");
            fclose(pFile);
            return -1;
        }
    }

    /* Close the file */
    fclose(pFile);

    return 0;
}

