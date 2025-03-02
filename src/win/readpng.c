/*
 * File: readpng.c
 * Purpose: PNG support
 *
 * This package provides a routine to read a PNG file and set up the
 * device dependent version of the image.
 *
 * This file has been modified for use with "Angband 2.9.2"
 * This file has been modified for use with "z+Angband 0.3.3"
 *
 * COPYRIGHT:
 *
 *   (C) Copyright Microsoft Corp. 1993.  All rights reserved.
 *
 *   You have a royalty-free right to use, modify, reproduce and
 *   distribute the Sample Files (and/or any modified version) in
 *   any way you find useful, provided that you agree that
 *   Microsoft has no warranty obligations or liability for any
 *   Sample Application Files which are modified.
 */

#include <windows.h>
#include "png.h"
#include "readdib.h"

/*
 * Extract the "WIN32" flag from the compiler
 */
#if defined(__WIN32__) || defined(__WINNT__) || defined(__NT__)
# ifndef WIN32
#  define WIN32
# endif
#endif

/*
 * Forces libpng to use the version of fread() from the run time library
 * that this program was compiled with.
 */
static void ReadFileFunc(png_structp png_ptr, png_bytep data, png_size_t length)
{
    FILE *file = (FILE *)png_get_io_ptr(png_ptr);
    fread(data, sizeof(png_byte), length, file);
}

/*
 * Imports a DIB from a PNG file. Once
 * the DIB is loaded, the function also creates a bitmap
 * and palette out of the DIB for a device-dependent form.
 *
 * Returns TRUE if the DIB is loaded and the bitmap/palette created, in which
 * case, the DIBINIT structure pointed to by pInfo is filled with the appropriate
 * handles, and FALSE if something went wrong.
 */
BOOL ReadDIB2_PNG(HWND hWnd, LPSTR lpFileName, DIBINIT *pInfo, DIBINIT *pMask, BOOL premultiply)
{
    png_structp png_ptr;
    png_infop info_ptr;
    byte header[8];
    png_bytep *row_pointers = NULL;
    BOOL noerror = TRUE;
    HBITMAP hBitmap;
    HPALETTE hPalette, hOldPal;
    BITMAPINFO bi, biSrc;
    HDC hDC;
    png_byte color_type;
    png_byte bit_depth;
    int width, height;
    int y;
    BOOL update = FALSE;

    /* Open the file and test it for being a png */
    FILE *fp = fopen(lpFileName, "rb");
    if (!fp) return FALSE;

    fread(header, 1, 8, fp);
    if (png_sig_cmp(header, 0, 8))
    {
        fclose(fp);
        return FALSE;
    }
    
    /* Create the png structure */
    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
    {
        fclose(fp);
        return FALSE;
    }
    
    /* Create the info structure */
    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
        fclose(fp);
        return FALSE;
    }

    /* Setup error handling for init */
    png_set_read_fn(png_ptr, fp, ReadFileFunc);
    png_set_sig_bytes(png_ptr, 8);
    
    png_read_info(png_ptr, info_ptr);
    
    width = png_get_image_width(png_ptr, info_ptr);
    height = png_get_image_height(png_ptr, info_ptr);
    color_type = png_get_color_type(png_ptr, info_ptr);
    bit_depth = png_get_bit_depth(png_ptr, info_ptr);
    
    png_set_interlace_handling(png_ptr);
    if (color_type == PNG_COLOR_TYPE_PALETTE)
    {
        png_set_palette_to_rgb(png_ptr);
        update = TRUE;
    }

    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
    {
        png_set_tRNS_to_alpha(png_ptr);
        update = TRUE;
    }

    if (bit_depth == 16)
    {
        png_set_strip_16(png_ptr);
        update = TRUE;
    }

    if ((color_type == PNG_COLOR_TYPE_GRAY) || (color_type == PNG_COLOR_TYPE_GRAY_ALPHA))
    {
        png_set_gray_to_rgb(png_ptr);
        update = TRUE;
    }

    if (update)
    {
        png_read_update_info(png_ptr, info_ptr);
        color_type = png_get_color_type(png_ptr, info_ptr);
        bit_depth = png_get_bit_depth(png_ptr, info_ptr);
    }

    png_set_bgr(png_ptr);

    /* After these requests, the data should always be RGB or ARGB */

    /* Initialize row_pointers */
    row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height);
    if (!row_pointers) return FALSE;
    for (y = 0; y < height; ++y)
    {
        row_pointers[y] = (png_bytep)malloc(png_get_rowbytes(png_ptr, info_ptr));
        if (!row_pointers[y]) return FALSE;
    }

    /* Read the image data into row_pointers */
    png_read_image(png_ptr, row_pointers);

    /* We are done with the file pointer, so close it */
    fclose(fp);

    /* Pre-multiply the image colors by the alpha if thats what we want */
    if (premultiply && (color_type == PNG_COLOR_TYPE_RGB_ALPHA))
    {
        int x;
        png_byte r, g, b, a;
        png_bytep row;

        /* Process the file */
        for (y = 0; y < height; ++y)
        {
            row = row_pointers[y];
            for (x = 0; x < width; ++x)
            {
                a = *(row + x * 4 + 3);
                if (a == 0)
                {
                    /* For every alpha that is fully transparent, make the corresponding color true black */
                    *(row + x * 4 + 0) = 0;
                    *(row + x * 4 + 1) = 0;
                    *(row + x * 4 + 2) = 0;
                }
                else if (a != 255)
                {
                    float rf, gf, bf, af;

                    /* Blend the color value based on this value */
                    r = *(row + x * 4 + 0);
                    g = *(row + x * 4 + 1);
                    b = *(row + x * 4 + 2);

                    rf = ((float)r) / 255.f;
                    gf = ((float)g) / 255.f;
                    bf = ((float)b) / 255.f;
                    af = ((float)a) / 255.f;

                    r = (png_byte)(rf * af * 255.f);
                    g = (png_byte)(gf * af * 255.f);
                    b = (png_byte)(bf * af * 255.f);

                    *(row + x * 4 + 0) = r;
                    *(row + x * 4 + 1) = g;
                    *(row + x * 4 + 2) = b;
                }
            }
        }
    }

    /* Create the DIB */
    bi.bmiHeader.biWidth = (LONG)width;
    bi.bmiHeader.biHeight = 0 - (LONG)height;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biClrUsed = 0;
    bi.bmiHeader.biClrImportant = 0;
    bi.bmiHeader.biBitCount = 24;
    bi.bmiHeader.biCompression = BI_RGB;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biSize = 40; /* the size of the structure */
    bi.bmiHeader.biXPelsPerMeter = 3424; /* Just a number I saw when testing this with a sample */
    bi.bmiHeader.biYPelsPerMeter = 3424; /* Just a number I saw when testing this with a sample */
    bi.bmiHeader.biSizeImage = width * height * 3;
    
    biSrc.bmiHeader.biWidth = (LONG)width;
    biSrc.bmiHeader.biHeight = 0 - (LONG)height;
    biSrc.bmiHeader.biPlanes = 1;
    biSrc.bmiHeader.biClrUsed = 0;
    biSrc.bmiHeader.biClrImportant = 0;
    biSrc.bmiHeader.biCompression = BI_RGB;
    biSrc.bmiHeader.biPlanes = 1;
    biSrc.bmiHeader.biSize = 40; /* the size of the structure */
    biSrc.bmiHeader.biXPelsPerMeter = 3424; /* Just a number I saw when testing this with a sample */
    biSrc.bmiHeader.biYPelsPerMeter = 3424; /* Just a number I saw when testing this with a sample */

    if (color_type == PNG_COLOR_TYPE_RGB_ALPHA)
    {
        biSrc.bmiHeader.biBitCount = 32;
        biSrc.bmiHeader.biSizeImage = width * height * 4;
        if (!pMask)
        {
            bi.bmiHeader.biBitCount = 32;
            bi.bmiHeader.biSizeImage = width * height * 4;
        }
    }
    else
    {
        biSrc.bmiHeader.biBitCount = 24;
        biSrc.bmiHeader.biSizeImage = width * height * 3;
    }

    hDC = GetDC(hWnd);

    hPalette = GetStockObject(DEFAULT_PALETTE);

    /* Need to realize palette for converting DIB to bitmap. */
    hOldPal = SelectPalette(hDC, hPalette, TRUE);
    RealizePalette(hDC);

    /* Copy the data to the DIB */
    hBitmap = CreateDIBitmap(hDC, &(bi.bmiHeader), 0, NULL, &biSrc, DIB_RGB_COLORS);
        
    if (hBitmap)
    {
        /* Hack for large PNG files */
        byte errline = 2;

        for (y = 0; y < height; ++y)
        {
            if (SetDIBits(hDC, hBitmap, height - y - 1, 1, row_pointers[y], &biSrc,
                DIB_RGB_COLORS) != 1)
            {
                /* Hack -- replace unreadable line by data from previous line */
                if (!errline)
                {
                    errline = 1;
                    /*plog_fmt("Error scanning line #%d, using previous line data instead.", y);*/
                    if (SetDIBits(hDC, hBitmap, height - y - 1, 1, row_pointers[y - 1], &biSrc,
                        DIB_RGB_COLORS) != 1)
                    {
                        errline = 2;
                    }
                }

                /* Hack -- don't allow multiple unreadable lines */
                else errline = 2;
            }
            else errline = 0;

            if (errline == 2)
            {
                DeleteObject(hBitmap);
                hBitmap = NULL;
                noerror = FALSE;
                break;
            }
        }
    }
    SelectPalette(hDC, hOldPal, TRUE);
    RealizePalette(hDC);
    if (!hBitmap)
    {
        DeleteObject(hPalette);
        noerror = FALSE;
    }
    else
    {
        pInfo->hBitmap = hBitmap;
        pInfo->hPalette = hPalette;
        pInfo->hDIB = 0;
        pInfo->ImageWidth = width;
        pInfo->ImageHeight = height;
    }
    
    if (pMask && (color_type == PNG_COLOR_TYPE_RGB_ALPHA))
    {
        byte *pBits, v;
        int x;
        DWORD *srcrow;
        HBITMAP hBitmap2 = NULL;
        HPALETTE hPalette2 = GetStockObject(DEFAULT_PALETTE);
        BOOL have_alpha = FALSE;
        
        /* Need to realize palette for converting DIB to bitmap. */
        hOldPal = SelectPalette(hDC, hPalette2, TRUE);
        RealizePalette(hDC);
        
        /* Allocate the storage space */
        pBits = (byte*)malloc(sizeof(byte)*width*height*3);
        if (!pBits) noerror = FALSE;

        if (noerror)
        {
            for (y = 0; y < height; ++y)
            {
                srcrow = (DWORD*)row_pointers[y];
                for (x = 0; x < width; ++x)
                {
                    /* Get the alpha byte from the source */
                    v = (*((DWORD*)srcrow + x)>>24);
                    v = 255 - v;
                    if (v == 255) have_alpha = TRUE;

                    /* Write the alpha byte to the three colors of the storage space */
                    *(pBits + (y*width*3) + (x*3)) = v;
                    *(pBits + (y*width*3) + (x*3)+1) = v;
                    *(pBits + (y*width*3) + (x*3)+2) = v;
                }
            }

            /* Create the bitmap from the storage space */
            if (have_alpha)
                hBitmap2 = CreateDIBitmap(hDC, &(bi.bmiHeader), CBM_INIT, pBits, &bi, DIB_RGB_COLORS);
            free(pBits);
        }
        SelectPalette(hDC, hOldPal, TRUE);
        RealizePalette(hDC);
        if (!hBitmap2)
        {
            DeleteObject(hPalette2);
            noerror = FALSE;
        }
        else
        {
            pMask->hBitmap = hBitmap2;
            pMask->hPalette = hPalette2;
            pMask->hDIB = 0;
        }
    }
    
    /* Release the image memory */
    for (y = 0; y < height; ++y) free(row_pointers[y]);
    free(row_pointers);
    
    /* Release all the PNG Structures */
    if (info_ptr)
    {
        png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
        info_ptr = NULL;
        png_ptr = NULL;
    }
    else if (png_ptr)
    {
        png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
        png_ptr = NULL;
    }
    
    ReleaseDC(hWnd,hDC);
    
    return noerror;
}