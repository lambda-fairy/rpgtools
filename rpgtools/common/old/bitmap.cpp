#include "bitmap.h"

#include <stdexcept>
#include <iostream>
#include <algorithm>

#include <cstdlib>
#include <cstring>
#include <cassert>

#include <png.h>
#include <zlib.h>

#include "util.h"

#define ERROR_GENERIC "unknown read error"

static const char xyzMagicNumber[] = {'X', 'Y', 'Z', '1'};
static const char bmpMagicNumber[] = {'B', 'M'};

/* constructors and destructors */
Bitmap::Bitmap() :
    width(0), height(0)
{
}

Bitmap::Bitmap(const std::string &filename) :
    width(0), height(0)
{
    std::string ext = Util::getExtension(filename);
    if (ext == "xyz")
        readFromXyz(filename);
    else if (ext == "png")
        readFromPng(filename);
    else if (ext == "bmp")
        readFromBmp(filename);
    else
        throw std::runtime_error(filename + ": could not determine file type");
}

Bitmap::Bitmap(unsigned int width, unsigned int height) :
    width(width), height(height),
    pixels(width * height * 3),
    alpha(width * height, false)
{
}

void Bitmap::readFromXyz(const std::string &filename)
{
    try {
        char magicNumber[sizeof(xyzMagicNumber)];
        uLongf size;

        ifstream file(filename.c_str(), std::ios::in | std::ios::binary);
        file.read(magicNumber, 4);

        if (std::memcmp(magicNumber, xyzMagicNumber, sizeof(magicNumber)))
            throw std::runtime_error("not a valid XYZ file");

        //Read the width and height
        width = height = 0;
        file.read(reinterpret_cast<char*>(&width), 2);
        file.read(reinterpret_cast<char*>(&height), 2);

        //Read the rest into a buffer
        file.seekg(0, file.end);
        size = static_cast<uLongf>(file.tellg()) - 8; //8 bytes for header
        if (size == 0)
            throw std::runtime_error("not a valid XYZ file");
        std::vector<Bytef> dataCompressed(size);
        file.seekg(8, file.beg);
        file.read(reinterpret_cast<char*>(dataCompressed.data()), dataCompressed.size());

        //Uncompress!
        size = 256 * 3 + width * height;
        std::vector<Bytef> data(size);
        if (uncompress(data.data(), &size, dataCompressed.data(), dataCompressed.size()) != Z_OK)
            throw std::runtime_error("zlib error");
        if (size != data.size())
            throw std::runtime_error("uncompressed image data too small");

        //Copy data to the correct buffers
        pixels = std::vector<uint8_t>(width * height * 3);
        alpha = std::vector<bool>(width * height);
        for (unsigned int y = 0; y < height; ++y) {
            for (unsigned int x = 0; x < width; ++x) {
                int offset = 256 * 3 + y * width + x;
                alpha[y * width + x] = (data[offset] != 0);
                std::vector<Bytef>::iterator color = data.begin() + data[offset] * 3;
                std::copy(color, color + 3, pixels.begin() + (y * width + x) * 3);
            }
        }
    } catch (ifstream::failure &e) {
        throw std::runtime_error(ERROR_GENERIC);
    } catch (std::runtime_error &e) {
        throw std::runtime_error(filename + ": " + e.what());
    }
}

void Bitmap::readFromPng(const std::string &filename)
{
    std::string error;

    png_structp png = NULL;
    png_infop info = NULL;
    png_bytepp rows = NULL;

    FILE *file = Util::fopen(filename, U("rb"));
    if (!file) {
        error = "could not open file";
        goto end;
    }

    //Check the header
    png_byte header[8];
    if (fread(header, 8, 1, file) != 1) {
        error = "could not validate as PNG";
        goto end;
    }
    if(png_sig_cmp(header, 0, 8) != 0) {
        error = "not a valid PNG file";
        goto end;
    }

    //Create
    png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png == NULL) {
        error = ERROR_GENERIC;
        goto end;
    }
    info = png_create_info_struct(png);
    if (info == NULL) {
        error = ERROR_GENERIC;
        goto end;
    }

    if (setjmp(png_jmpbuf(png))) {
        error = ERROR_GENERIC;
        goto end;
    }

    png_init_io(png, file);
    png_set_sig_bytes(png, 8);
    png_read_png(png, info, PNG_TRANSFORM_STRIP_16 | PNG_TRANSFORM_PACKING, NULL);

    //Basic info
    width = png_get_image_width(png, info);
    height = png_get_image_height(png, info);
    if (width == 0 || height == 0) {
        error = "invalid image dimensions";
    }

    //Allocate image buffer, get rows
    pixels = std::vector<uint8_t>(width * height * 3);
    alpha = std::vector<bool>(width * height);
    rows = png_get_rows(png, info);

    //Check the colortype
    switch(png_get_color_type(png, info)) {
    case PNG_COLOR_TYPE_PALETTE: {
        //Get palette
        png_colorp palette;
        int nPalette;
        png_get_PLTE(png, info, &palette, &nPalette);

        //Populate pixels
        for (unsigned int y = 0; y < height; ++y) {
            for (unsigned int x = 0; x < width; ++x) {
                alpha[y * width + x] = (rows[y][x] != 0);
                uint8_t *color = reinterpret_cast<uint8_t*>(&palette[rows[y][x]]);
                std::copy(color, color + 3, pixels.begin() + (y * width + x) * 3);
            }
        }
    } break;
    case PNG_COLOR_TYPE_RGB:
        //Populate pixels
        std::fill(alpha.begin(), alpha.end(), true);
        for (unsigned int y = 0; y < height; ++y) {
            std::copy(rows[y], rows[y] + width * 3, pixels.begin() + (y * width) * 3);
        }
        break;
    case PNG_COLOR_TYPE_RGBA:
        //Populate pixels
        for (unsigned int y = 0; y < height; ++y) {
            for (unsigned int x = 0; x < width; ++x) {
                uint8_t *color = reinterpret_cast<uint8_t*>(&rows[y][x * 4]);
                alpha[y * width + x] = (color[3] != 0);
                std::copy(color, color + 3, pixels.begin() + (y * width + x) * 3);
            }
        }
        break;
    case PNG_COLOR_TYPE_GRAY:
        //Populate pixels
        std::fill(alpha.begin(), alpha.end(), true);
        for (unsigned int y = 0; y < height; ++y) {
            for (unsigned int x = 0; x < width; ++x) {
                int offset = (y * width + x) * 3;
                std::fill(pixels.begin() + offset, pixels.begin() + offset + 3, rows[y][x]);
            }
        }
        break;
    case PNG_COLOR_TYPE_GRAY_ALPHA:
        //Populate pixels
        for (unsigned int y = 0; y < height; ++y) {
            for (unsigned int x = 0; x < width; ++x) {
                int offset = y * width + x;
                alpha[offset] = (rows[y][x * 2 + 1] != 0);
                std::fill(pixels.begin() + offset * 3, pixels.begin() + offset * 3 + 3, rows[y][x * 2]);
            }
        }
    default:
        error = "unknown image type";
        goto end;
    }
end:
    if (png != NULL || info != NULL) {
        png_free_data(png, info, PNG_FREE_ALL, -1);
        png_destroy_read_struct(&png, &info, NULL);
    }
    if (file)
        fclose(file);

    if (!error.empty())
        throw std::runtime_error(filename + ": " + error);
}

void Bitmap::readFromBmp(const std::string &filename)
{
    try {
        char magicNumber[sizeof(bmpMagicNumber)];
        int32_t temp;

        ifstream file(filename.c_str(), std::ios::in | std::ios::binary);

        //Read and verify magic number
        file.read(magicNumber, sizeof(magicNumber));
        if (std::memcmp(magicNumber, bmpMagicNumber, sizeof(magicNumber)))
            throw std::runtime_error("not a valid BMP file");

        //Read: pixel data offset, palette offset
        file.seekg(10);
        uint32_t pixelOffset;
        file.read(reinterpret_cast<char*>(&pixelOffset), 4);
        uint32_t paletteOffset;
        file.read(reinterpret_cast<char*>(&paletteOffset), 4);
        paletteOffset += 14;

        //Read: width, height, pixel order; basic sanity checking
        width = 0;
        file.read(reinterpret_cast<char*>(&width), 4);
        file.read(reinterpret_cast<char*>(&temp), 4);
        if (width == 0 || temp == 0)
            throw std::runtime_error("invalid image dimensions");
        bool topDown = temp < 0;
        height = abs(temp);

        //More sanity checking
        temp = 0;
        file.read(reinterpret_cast<char*>(&temp), 2);
        if (temp != 1)
            throw std::runtime_error("number of BMP planes is not 1");
        file.read(reinterpret_cast<char*>(&temp), 2);
        if (temp != 8)
            throw std::runtime_error("BMP is not 8-bit");
        file.read(reinterpret_cast<char*>(&temp), 4);
        if (temp != 0)
            throw std::runtime_error("BMP is compressed");

        //Read palette info
        uint32_t paletteSize;
        file.read(reinterpret_cast<char*>(&paletteSize), 4);
        if (paletteSize > 256)
            throw std::runtime_error("BMP header specifies more than 256 colors");
        if (paletteSize == 0)
            paletteSize = 256;

        //Read palette
        std::vector<uint8_t> palette(paletteSize * 4);
        file.seekg(paletteOffset);
        file.read(reinterpret_cast<char*>(palette.data()), palette.size());

        //Read pixels
        std::vector<uint8_t> bmpPixels(width * height);
        file.seekg(pixelOffset);
        file.read(reinterpret_cast<char*>(bmpPixels.data()), bmpPixels.size());

        //Populate pixels
        pixels = std::vector<uint8_t>(width * height * 3);
        alpha = std::vector<bool>(width * height);
        for (unsigned int y = 0; y < height; ++y) {
            for (unsigned int x = 0; x < width; ++x) {
                int srcOffset = (topDown ? y : height - 1 - y) * width + x;
                int dstOffset = (y * width + x) * 3;

                alpha[y * width + x] = (bmpPixels[srcOffset] != 0);
                int color = bmpPixels[srcOffset] * 4;

                pixels[dstOffset+0] = palette[color+2];
                pixels[dstOffset+1] = palette[color+1];
                pixels[dstOffset+2] = palette[color+0];
            }
        }
    } catch (ifstream::failure &e) {
        throw std::runtime_error(filename + ": " + ERROR_GENERIC);
    } catch (std::runtime_error &e) {
        throw std::runtime_error(filename + ": " + e.what());
    }
}

void Bitmap::blit(int mX, int mY, const Bitmap &other, int oX, int oY, int oW, int oH)
{
    for (int y = 0; y < oH; ++y) {
        for (int x = 0; x < oW; ++x) {
            int oOffset = (oY + y) * other.width + oX + x;
            int mOffset = (mY + y) * width + mX + x;
            if (other.alpha[oOffset]) {
                alpha[mOffset] = true;
                std::copy(other.pixels.begin() + oOffset * 3, other.pixels.begin() + oOffset * 3 + 3,
                          pixels.begin() + ((mY + y) * width + mX + x) * 3);
            }
        }
    }
}

void Bitmap::writeToPng(const std::string &filename) const
{
    volatile bool failed = false;

    std::vector<png_byte> row(width * 4);

    png_structp png = 0;
    png_infop info = 0;

    //Open file for writing
    FILE *file = Util::fopen(filename, U("wb"));
    if (!file)
        goto error;

    //Initialize write structure
    png = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
    if (png == 0)
        goto error;

    //Initialize info structure
    info = png_create_info_struct(png);
    if (info == 0)
        goto error;

    //Setup Exception handling
    if (setjmp(png_jmpbuf(png)))
        goto error;

    png_init_io(png, file);

    //Write header
    png_set_IHDR(png, info, width, height,
                 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
    png_write_info(png, info);

    //Write image data
    for (unsigned int y = 0; y < height; ++y) {
        std::fill(row.begin(), row.end(), 0);
        //Populate & write row buffer
        for (unsigned int x = 0; x < width; ++x) {
            int offset = (y * width + x) * 3;
            std::copy(pixels.begin() + offset, pixels.begin() + offset + 3, row.begin() + x * 4);
            row[x * 4 + 3] = (alpha[y * width + x] ? 255 : 0);
        }
        png_write_row(png, row.data());
    }

    //End
    png_write_end(png, 0);
    goto end;

error:
    failed = true;
end:
    if (png || info) {
        png_free_data(png, info, PNG_FREE_ALL, -1);
        png_destroy_write_struct(&png, &info);
    }
    if (file)
        fclose(file);
    if (failed)
        throw std::runtime_error("unknown error while writing PNG");
}
