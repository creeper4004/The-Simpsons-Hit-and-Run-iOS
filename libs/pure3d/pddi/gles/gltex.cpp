//=============================================================================
// Copyright (c) 2002 Radical Games Ltd.  All rights reserved.
//=============================================================================

#include <pddi/gles/gl.hpp>
#include <pddi/gles/gldisplay.hpp>
#include <pddi/gles/gltex.hpp>
#include <pddi/gles/glcon.hpp>
#include <pddi/gles/decompress.hpp>

#include <math.h>
#include <cstdlib>
#include <pddi/base/debug.hpp>
#include <radmemory.hpp>

#include <microprofile.h>

static inline GLenum PickPixelFormat(pddiPixelFormat format)
{
    switch (format)
    {
    // En GLES2 el `internalformat` de glTexImage2D debe ser un formato interno
    // valido (GL_RGBA/GL_RGB). GL_BGRA_EXT (0x80E1) NO es un internalformat
    // valido en ES2: solo se acepta como `format` (via GL_EXT_bgra). Usarlo como
    // internalformat funciona en el simulador (driver laxo) pero da texturas
    // negras en el dispositivo real (driver estricto). El `format` (BGRA/RGBA)
    // ya se elige aparte segun lock.native en el glTexImage2D de abajo.
    case PDDI_PIXEL_RGB888: return GL_RGBA;
    case PDDI_PIXEL_ARGB8888: return GL_RGBA;
#ifdef RAD_VITAGL
    case PDDI_PIXEL_DXT1: return GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
    case PDDI_PIXEL_DXT3: return GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
    case PDDI_PIXEL_DXT5: return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
#else
    case PDDI_PIXEL_DXT1: return GL_RGBA;
    case PDDI_PIXEL_DXT3: return GL_RGBA;
    case PDDI_PIXEL_DXT5: return GL_RGBA;
#endif
    }
    PDDIASSERT(false);
    return GL_INVALID_ENUM;
};

static inline pddiPixelFormat PickPixelFormat(pddiTextureType type, int bitDepth, int alphaDepth)
{
    switch (type)
    {
    case PDDI_TEXTYPE_RGB:
        switch (alphaDepth)
        {
        case 0:
            return (bitDepth <= 16) ? PDDI_PIXEL_RGB565 : PDDI_PIXEL_RGB888;
        case 1:
            return (bitDepth <= 16) ? PDDI_PIXEL_ARGB1555 : PDDI_PIXEL_ARGB8888;
        default:
            return (bitDepth <= 16) ? PDDI_PIXEL_ARGB4444 : PDDI_PIXEL_ARGB8888;
        }
        break;

    case PDDI_TEXTYPE_PALETTIZED:
        return PDDI_PIXEL_PAL8;

    case PDDI_TEXTYPE_LUMINANCE:
        return PDDI_PIXEL_LUM8;

    case PDDI_TEXTYPE_BUMPMAP:
        return PDDI_PIXEL_DUDV88;

    case PDDI_TEXTYPE_DXT1:
        return PDDI_PIXEL_DXT1;

    case PDDI_TEXTYPE_DXT2:
        return PDDI_PIXEL_DXT2;

    case PDDI_TEXTYPE_DXT3:
        return PDDI_PIXEL_DXT3;

    case PDDI_TEXTYPE_DXT4:
        return PDDI_PIXEL_DXT4;

    case PDDI_TEXTYPE_DXT5:
        return PDDI_PIXEL_DXT5;

    case PDDI_TEXTYPE_YUV:
        return PDDI_PIXEL_YUV;
    }
    PDDIASSERT(false);
    return PDDI_PIXEL_UNKNOWN;
};

void pglTexture::SetGLState(void)
{
    if(context->contextID != contextID)
    {
        contextID = context->contextID;
        gltexture = 0;
    }

    MICROPROFILE_SCOPEI("PDDI", "pglTexture::SetGLState", MP_RED);

    if(!valid)
    {
        glDeleteTextures(1, &gltexture);
        glGenTextures(1,&gltexture);
        glBindTexture(GL_TEXTURE_2D, gltexture);

//      if(nMipMap == 0)
        if (type == PDDI_TEXTYPE_DXT1 || type == PDDI_TEXTYPE_DXT3 || type == PDDI_TEXTYPE_DXT5)
        {
#ifdef RAD_VITAGL
            unsigned int blocksize = lock.format == PDDI_PIXEL_DXT1 ? 8 : 16;
            GLenum internalFormat = lock.format == PDDI_PIXEL_DXT5 ? GL_COMPRESSED_RGBA_S3TC_DXT5_EXT :
                lock.format == PDDI_PIXEL_DXT3 ? GL_COMPRESSED_RGBA_S3TC_DXT3_EXT : GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
            glCompressedTexImage2D(GL_TEXTURE_2D, 0, internalFormat, xSize,
                ySize, 0, ceil(xSize/4.0)*ceil(ySize/4.0)*blocksize, (GLvoid*)bits[0]);
#else
            unsigned char* image = new unsigned char[xSize * ySize * 4];
            unsigned int blocksize = lock.format == PDDI_PIXEL_DXT1 ? 8 : 16;
            if (type == PDDI_TEXTYPE_DXT1)
                BlockDecompressImageBC1(xSize, ySize, (const uint8_t*)bits[0], image);
            else if (type == PDDI_TEXTYPE_DXT3)
                BlockDecompressImageBC2(xSize, ySize, (const uint8_t*)bits[0], image);
            else if( type == PDDI_TEXTYPE_DXT5)
                BlockDecompressImageBC3(xSize, ySize, (const uint8_t*)bits[0], image);
            glTexImage2D(GL_TEXTURE_2D, 0, PickPixelFormat(lock.format), xSize,
                ySize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                image);
#if defined(RAD_IOS)
            {
                static int sTexProbes = 0;
                if (getenv("IOS_ATTRIB_PROBE") && sTexProbes < 20) {
                    // bits[0] is BC-compressed and cannot be scanned as RGBA.
                    // Inspect the decompressed allocation before releasing it.
                    const unsigned char* px = image;
                    unsigned int n = (unsigned int)xSize * (unsigned int)ySize;
                    if (n > 0) {
                        unsigned long long r=0,g=0,b=0,a=0; unsigned int cnt=0;
                        const unsigned char* q = px;
                        for (unsigned int i=0;i<n && cnt<40000;i++,q+=4,cnt++){ r+=q[0];g+=q[1];b+=q[2];a+=q[3]; }
                        fprintf(stderr,"IOS_TEXTURE_DECODE %dx%d fmt=%d native=%d type=%d avg(%llu,%llu,%llu,%llu) first=%02x%02x%02x%02x\n",
                            xSize,ySize,lock.format,lock.native?1:0,type,cnt?r/cnt:0,cnt?g/cnt:0,cnt?b/cnt:0,cnt?a/cnt:0,
                            px[0],px[1],px[2],px[3]);
                    }
                    sTexProbes++;
                }
            }
#endif
            delete [] image;

#endif
        }
#ifdef RAD_VITAGL
        else if (type == PDDI_TEXTYPE_YUV)
        {
            glCompressedTexImage2D(GL_TEXTURE_2D, 0, VGL_YUV420P_BT601, xSize,
                ySize, 0, (xSize * ySize * 3) / 2, (GLvoid *)bits[0]);
        }
#endif
        else
        {
            glTexImage2D(GL_TEXTURE_2D, 0, PickPixelFormat(lock.format), xSize,
                ySize, 0, lock.native ? GL_BGRA_EXT : GL_RGBA, GL_UNSIGNED_BYTE,
                (GLvoid *)bits[0]);
#if defined(RAD_IOS)
            {
                static int sTexProbes = 0;
                if (sTexProbes < 20) {
                    const unsigned char* px = (const unsigned char*)bits[0];
                    unsigned int n = (unsigned int)xSize * (unsigned int)ySize;
                    if (n > 0) {
                        unsigned long long r=0,g=0,b=0,a=0; unsigned int cnt=0;
                        const unsigned char* q = px;
                        for (unsigned int i=0;i<n && cnt<40000;i++,q+=4,cnt++){ r+=q[0];g+=q[1];b+=q[2];a+=q[3]; }
                        FILE*_f=fopen("/tmp/shar_tex.txt","a");
                        if(_f){ fprintf(_f,"TEX %dx%d fmt=%d native=%d type=%d avg(%llu,%llu,%llu,%llu) first=%02x%02x%02x%02x\n",
                            xSize,ySize,lock.format,lock.native?1:0,type,cnt?r/cnt:0,cnt?g/cnt:0,cnt?b/cnt:0,cnt?a/cnt:0,
                            px[0],px[1],px[2],px[3]); fclose(_f); }
                    }
                    sTexProbes++;
                }
            }
#endif
        }
        /*
        else
        {

            int tmpMipMap = nMipMap;
            tmpMipMap--;
            if(tmpMipMap < 0)
                tmpMipMap = 0;

            int i = 0;
            int width = xSize;
            int height = ySize;
            bool bottomed = false;
            bool done = false;

            while(!done)
            {
                char* data = bits[i];
                if(i > tmpMipMap)
                    data = bits[tmpMipMap];

                glTexImage2D(GL_TEXTURE_2D, i, GL_RGBA8, xSize >> i,
                    ySize >> i, 0, lock.native ? GL_BGRA_EXT : GL_RGBA, GL_UNSIGNED_BYTE,
                    (GLvoid *)data);

                done = (width == 1) || (height == 1);

                width >>= 1;
                if(width < 1) 
                {
                    width = 1;
                    bottomed = true;
                }

                height >>= 1;
                if(height < 1) 
                {
                    height = 1;
                    bottomed = true;
                }

                i++;
            }
        }
        */

        valid = true;
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, gltexture);
    }
}

int fastlog2(int x)
{
    int r = 0;
    int tmp = x;
    while(tmp > 1)
    {
        r++;
        tmp = tmp >> 1;

        if((tmp << r) != x)
            // not power of 2
            return -1;
    }
    return r;
}

bool pglTexture::Create(int x, int y, int bpp, int alphaDepth, int nMip, pddiTextureType textureType, pddiTextureUsageHint usageHint)
{
    xSize = x;
    ySize = y;
    nMipMap = nMip;
    type = textureType;

    log2X = fastlog2(xSize);
    log2Y = fastlog2(ySize);

#ifndef RAD_VITA
    if((log2X == -1) || (log2Y == -1))
    {
        lastError = PDDI_TEX_NOT_POW_2;
        return false;
    }
#endif

    if ((xSize > context->GetMaxTextureDimension()) ||
        (ySize > context->GetMaxTextureDimension()))
    {
        lastError = PDDI_TEX_TOO_BIG;
        return false;
    }

    // TODO palletized
    if (textureType == PDDI_TEXTYPE_PALETTIZED)
    {
        textureType = PDDI_TEXTYPE_RGB;
        bpp = 32;
    }

    bits = new char* [nMipMap + 1];
    if (type == PDDI_TEXTYPE_DXT1 || type == PDDI_TEXTYPE_DXT3 || type == PDDI_TEXTYPE_DXT5)
    {
        unsigned int blocksize = type == PDDI_TEXTYPE_DXT1 ? 8 : 16;
        for(int i = 0; i < nMipMap+1; i++)
            bits[i] = (char*)radMemoryAllocAligned(radMemoryGetCurrentAllocator(), size_t(ceil(double(xSize>>i)/4)*ceil(double(ySize>>i)/4)*blocksize), 16);
    }
    else
    {
        for(int i = 0; i < nMipMap+1; i++)
            bits[i] = (char*)radMemoryAllocAligned(radMemoryGetCurrentAllocator(), ((xSize>>i)*(ySize>>i)*bpp)/8, 16);
    }

    lock.depth = bpp;
    lock.format = PickPixelFormat(textureType, bpp, alphaDepth);

    if(context->GetDisplay()->ExtBGRA())
    {
        lock.native = true;
        lock.rgbaLShift[0] = lock.rgbaRShift[0] =
        lock.rgbaLShift[1] = lock.rgbaRShift[1] =
        lock.rgbaLShift[2] = lock.rgbaRShift[2] =
        lock.rgbaLShift[3] = lock.rgbaRShift[3] = 0;

        lock.rgbaMask[0] = 0x00ff0000;
        lock.rgbaMask[1] = 0x0000ff00;
        lock.rgbaMask[2] = 0x000000ff;
        lock.rgbaMask[3] = 0xff000000;
    }
    else
    {
        lock.native = false;
        lock.rgbaRShift[0] = 16;
        lock.rgbaLShift[2] = 16;

        lock.rgbaLShift[0] = 
        lock.rgbaLShift[1] = lock.rgbaRShift[1] =
        lock.rgbaRShift[2] =
        lock.rgbaLShift[3] = lock.rgbaRShift[3] = 0;

        lock.rgbaMask[0] = 0x000000ff;
        lock.rgbaMask[1] = 0x0000ff00;
        lock.rgbaMask[2] = 0x00ff0000;
        lock.rgbaMask[3] = 0xff000000;
    }

    context->ADD_STAT(PDDI_STAT_TEXTURE_ALLOC_32BIT, (float)((xSize * ySize * lock.depth) / 8192));
    context->ADD_STAT(PDDI_STAT_TEXTURE_COUNT_32BIT, 1);

    return true;
}

pglTexture::pglTexture(pglContext* c)
{
    context = c;
    contextID = c->contextID;
    bits = NULL;
    gltexture = 0;
    priority = 15;
    valid = false;
}

pglTexture::~pglTexture()
{
    if(gltexture) glDeleteTextures(1, &gltexture);

    if(bits)
    {
        for(int i = 0; i < nMipMap + 1; i++)
            radMemoryFreeAligned(bits[i]);
        delete [] bits;
    }

    context->ADD_STAT(PDDI_STAT_TEXTURE_ALLOC_32BIT, -(float)((xSize * ySize * lock.depth) / 8192));
    context->ADD_STAT(PDDI_STAT_TEXTURE_COUNT_32BIT, -1);
}

pddiPixelFormat pglTexture::GetPixelFormat()
{
    return PDDI_PIXEL_ARGB8888;
}

int   pglTexture::GetWidth()
{
    return xSize;
}

int   pglTexture::GetHeight()
{
    return ySize;
}

int   pglTexture::GetDepth()
{
    return 32;
}

int   pglTexture::GetNumMipMaps()
{
    return nMipMap;
}

int pglTexture::GetAlphaDepth()
{
    return 8;
}

pddiLockInfo* pglTexture::Lock(int mipMap, pddiRect* rect)
{
    PDDIASSERT(mipMap <= nMipMap);

    lock.width = 1 << (log2X-mipMap);
    lock.height = 1 << (log2Y-mipMap);
    if (lock.format == PDDI_PIXEL_DXT1 || lock.format == PDDI_PIXEL_DXT3 || lock.format == PDDI_PIXEL_DXT5)
    {
        unsigned int blocksize = lock.format == PDDI_PIXEL_DXT1 ? 8 : 16;
        lock.pitch = ceil( double( xSize >> mipMap ) / 4 ) * blocksize;
        lock.bits = bits[mipMap];
    }
    else if (lock.format == PDDI_PIXEL_YUV)
    {
        lock.pitch = (lock.width * lock.depth) / 8;
        lock.bits = bits[mipMap];
    }
    else
    {
        lock.pitch = -(lock.width * 4);
        lock.bits = bits[mipMap] + (lock.width * (lock.height - 1) * 4);
    }

    return &lock;
}

void pglTexture::Unlock(int mipLevel)
{
    valid = false;
}

void pglTexture::SetPriority(int p)
{
    priority = p;
}

int pglTexture::GetPriority(void)
{
    return priority;
}

// paging control
void pglTexture::Prefetch(void)
{
}

void pglTexture::Discard(void)
{
}

// palette managment
int pglTexture::GetNumPaletteEntries(void)
{
    return 0;
}

void pglTexture::SetPalette(int nEntries, pddiColour* palette)
{
}

int pglTexture::GetPalette(pddiColour* palette)
{
    return 0;
}
