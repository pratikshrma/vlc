/*****************************************************************************
 * bitmapinfoheader.h : BITMAPINFOHEADER handling
 *****************************************************************************
 * Copyright (C) 2001-2009 VLC authors and VideoLAN
 *               2018      VideoLabs, VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#include <vlc_common.h>
#include <vlc_es.h>
#include <vlc_codecs.h>
#include <limits.h>

/* biCompression / Others are FourCC */
#ifndef BI_RGB
 #define BI_RGB             0x0000
 #define BI_RLE8            0x0001
 #define BI_RLE4            0x0002
 #define BI_BITFIELDS       0x0003
 #define BI_JPEG            0x0004
 #define BI_PNG             0x0005
#endif
#define BI_ALPHAFIELDS      0x0006
#define BI_CMYK             0x000B
#define BI_CMYKRLE8         0x000C
#define BI_CMYKRLE4         0x000D

static const struct
{
    vlc_fourcc_t codec;
    uint32_t i_rmask, i_gmask, i_bmask;
} bitmap_rgb_masks[] = {
    { VLC_CODEC_RGB15,      0x7c00, /* FIXME it should be B5G5R5LE */
                            0x03e0,
                            0x001f, },
    { VLC_CODEC_RGB16,      0xf800, /* FIXME it should be B5G6R5LE */
                            0x07e0,
                            0x001f, },
};

static inline void SetBitmapRGBMasks( vlc_fourcc_t i_fourcc, video_format_t *fmt )
{
    for( size_t i=0; i<ARRAY_SIZE(bitmap_rgb_masks); i++ )
    {
        if( bitmap_rgb_masks[i].codec == i_fourcc )
        {
            fmt->i_rmask = bitmap_rgb_masks[i].i_rmask;
            fmt->i_gmask = bitmap_rgb_masks[i].i_gmask;
            fmt->i_bmask = bitmap_rgb_masks[i].i_bmask;
            fmt->i_chroma = i_fourcc;
            video_format_FixRgb( fmt );
            break;
        }
    }
}

struct bitmapinfoheader_properties
{
    bool b_flipped;
    unsigned i_stride;
};

static inline int ParseBitmapInfoHeader( const VLC_BITMAPINFOHEADER *p_bih, size_t i_bih,
                                         es_format_t *fmt,
                                         struct bitmapinfoheader_properties *p_props )
{
    /* Extradata is the remainder of the chunk less the BIH */
    const uint8_t *p_bihextra = (const uint8_t *) &p_bih[1];
    size_t i_bihextra;
    if( i_bih <= INT_MAX - sizeof(VLC_BITMAPINFOHEADER) &&
            i_bih >= sizeof(VLC_BITMAPINFOHEADER) )
        i_bihextra = i_bih - sizeof(VLC_BITMAPINFOHEADER);
    else
        i_bihextra = 0;

    if( p_bih->biCompression == BI_RGB ||
        p_bih->biCompression == BI_BITFIELDS )
    {
        uint32_t biCompression = p_bih->biCompression;
        switch( p_bih->biBitCount )
        {
            case 32:
                fmt->video.i_chroma = fmt->i_codec = VLC_CODEC_XBGR;
                break;
            case 24:
                fmt->video.i_chroma = fmt->i_codec = VLC_CODEC_BGR24;
                break;
            case 16:
                fmt->video.i_chroma = fmt->i_codec = VLC_CODEC_RGB16; /* RGB (5,6,5 bits) */
                break;
            case 15: /* RGB (B least 5 bits) */
                fmt->video.i_chroma = fmt->i_codec = VLC_CODEC_RGB15;
                break;
            case 9: /* <- TODO check that */
                fmt->video.i_chroma = fmt->i_codec = VLC_CODEC_I410;
                break;
            case 8:
                if ( p_bih->biClrUsed )
                    fmt->video.i_chroma = fmt->i_codec = VLC_CODEC_RGBP;
                else
                    fmt->video.i_chroma = fmt->i_codec = VLC_CODEC_GREY;
                break;
            default:
                if( p_bih->biClrUsed < 8 )
                    fmt->video.i_chroma = fmt->i_codec = VLC_CODEC_RGBP;
                break;
        }

        if( p_bih->biCompression == BI_BITFIELDS ) /* Only 16 & 32 */
        {
            if( i_bihextra >= 3 * sizeof(uint32_t) )
            {
                uint32_t rmask = GetDWLE( &p_bihextra[0] );
                uint32_t gmask = GetDWLE( &p_bihextra[4] );
                uint32_t bmask = GetDWLE( &p_bihextra[8] );
                uint32_t amask = i_bihextra >= 4 * sizeof(uint32_t) ? GetDWLE( &p_bihextra[12] ) : 0;

                vlc_fourcc_t known_chroma = 0;
                switch( p_bih->biBitCount )
                {
                    case 32:
                        if (rmask == 0x00ff0000 &&
                            gmask == 0x0000ff00 &&
                            bmask == 0x000000ff &&
                            amask == 0x00000000)
                        {
                            known_chroma = VLC_CODEC_XRGB;
                        }
                        else
                        if (rmask == 0x000000ff &&
                            gmask == 0x0000ff00 &&
                            bmask == 0x00ff0000 &&
                            amask == 0x00000000)
                        {
                            known_chroma = VLC_CODEC_XBGR;
                        }
                        else
                        if (rmask == 0xff000000 &&
                            gmask == 0x00ff0000 &&
                            bmask == 0x0000ff00 &&
                            amask == 0x00000000)
                        {
                            known_chroma = VLC_CODEC_RGBX;
                        }
                        else
                        if (rmask == 0x0000ff00 &&
                            gmask == 0x00ff0000 &&
                            bmask == 0xff000000 &&
                            amask == 0x00000000)
                        {
                            known_chroma = VLC_CODEC_BGRX;
                        }
                        if (rmask == 0x00ff0000 &&
                            gmask == 0x0000ff00 &&
                            bmask == 0x000000ff &&
                            amask == 0xff000000)
                        {
                            known_chroma = VLC_CODEC_ARGB;
                        }
                        else
                        if (rmask == 0x000000ff &&
                            gmask == 0x0000ff00 &&
                            bmask == 0x00ff0000 &&
                            amask == 0xff000000)
                        {
                            known_chroma = VLC_CODEC_ABGR;
                        }
                        else
                        if (rmask == 0xff000000 &&
                            gmask == 0x00ff0000 &&
                            bmask == 0x0000ff00 &&
                            amask == 0x000000ff)
                        {
                            known_chroma = VLC_CODEC_RGBA;
                        }
                        else
                        if (rmask == 0x0000ff00 &&
                            gmask == 0x00ff0000 &&
                            bmask == 0xff000000 &&
                            amask == 0x000000ff)
                        {
                            known_chroma = VLC_CODEC_BGRA;
                        }
                        else
                        {
                            // unknown mask
                            return VLC_ENOTSUP;
                        }
                        break;
                    default:
                        break;
                }

                if (known_chroma != 0)
                {
                    fmt->video.i_chroma = fmt->i_codec = known_chroma;
                    fmt->video.i_rmask = 0;
                    fmt->video.i_gmask = 0;
                    fmt->video.i_bmask = 0;
                }
                else if (amask == 0)
                {
                    fmt->video.i_rmask = rmask;
                    fmt->video.i_gmask = gmask;
                    fmt->video.i_bmask = bmask;
                    video_format_FixRgb( &fmt->video );
                }
                else
                {
                    // unsupported alpha mask
                    return VLC_ENOTSUP;
                }
            }
            else
            {
                // bogus mask size, assume BI_RGB positions
                biCompression = BI_RGB;
            }
        }
        else if( fmt->i_codec == VLC_CODEC_RGBP )
        {
            /* The palette should not be included in biSize, but come
             * directly after BITMAPINFORHEADER in the BITMAPINFO structure */
            fmt->video.p_palette = malloc( sizeof(video_palette_t) );
            if ( fmt->video.p_palette )
            {
                fmt->video.p_palette->i_entries = __MIN(i_bihextra/4, 256);
                for( int k = 0; k < fmt->video.p_palette->i_entries; k++ )
                {
                    for( int j = 0; j < 4; j++ )
                        fmt->video.p_palette->palette[k][j] = p_bihextra[4*k+j];
                }
            }
        }
        if (biCompression == BI_RGB)
        {
            vlc_fourcc_t bi_rgb_chroma;
            switch (p_bih->biBitCount)
            {
                case 32: bi_rgb_chroma = VLC_CODEC_XBGR; break;
                case 24: bi_rgb_chroma = VLC_CODEC_BGR24; break;
                case 16: bi_rgb_chroma = VLC_CODEC_BGR565LE; break;
                case 15: bi_rgb_chroma = VLC_CODEC_BGR555LE; break;
                default: return VLC_EINVAL;
            }
            fmt->video.i_chroma = fmt->i_codec = bi_rgb_chroma;
            fmt->video.i_rmask = 0;
            fmt->video.i_gmask = 0;
            fmt->video.i_bmask = 0;
        }

        p_props->i_stride = p_bih->biWidth * (p_bih->biBitCount >> 3);
        /* Unintuitively RGB DIB are always coded from bottom to top,
         * except when height is negative */
        if ( p_bih->biHeight <= INT32_MAX )
            p_props->b_flipped = true;
        /* else
         *     set below to positive value */
    }
    else /* Compressed codecs */
    {
        fmt->i_codec = vlc_fourcc_GetCodec( VIDEO_ES, p_bih->biCompression );

        /* Copy extradata if any */
        if( i_bihextra > 0 )
        {
            fmt->p_extra = malloc( i_bihextra );
            if( unlikely(fmt->p_extra == NULL) )
                return VLC_ENOMEM;
            fmt->i_extra = i_bihextra;
            memcpy( fmt->p_extra, p_bihextra, i_bihextra );
        }

        SetBitmapRGBMasks( fmt->i_codec, &fmt->video );
    }

    video_format_Setup( &fmt->video, fmt->i_codec,
                        p_bih->biWidth, p_bih->biHeight,
                        p_bih->biWidth, p_bih->biHeight,
                        fmt->video.i_sar_num, fmt->video.i_sar_den );

    /* Uncompressed Bitmap or YUV, YUV being always top to bottom whatever
     * height sign is, and compressed must also not use flip, so positive
     * values only here */
    if ( fmt->video.i_height > INT32_MAX )
    {
        fmt->video.i_visible_height =
        fmt->video.i_height = -1 * p_bih->biHeight;
    }

    return VLC_SUCCESS;
}

static inline int CreateBitmapInfoHeader( const es_format_t *fmt,
                                          VLC_BITMAPINFOHEADER *p_bih,
                                          uint8_t **p_bih_extra,
                                          size_t *pi_total )
{
    uint16_t biBitCount = 0;
    uint32_t biCompression = BI_RGB;
    bool b_has_alpha = false;
    switch( fmt->i_codec )
    {
        case VLC_CODEC_XRGB:
            biBitCount = 32;
            break;
        case VLC_CODEC_BGRX:
        case VLC_CODEC_XBGR:
        case VLC_CODEC_RGBX:
            biBitCount = 32;
            biCompression = BI_BITFIELDS;
            break;
        case VLC_CODEC_BGRA:
        case VLC_CODEC_RGBA:
        case VLC_CODEC_ARGB:
        case VLC_CODEC_ABGR:
            biBitCount = 32;
            biCompression = BI_BITFIELDS;
            b_has_alpha = true;
            break;
        case VLC_CODEC_BGR24:
            biBitCount = 24;
            break;
        case VLC_CODEC_RGB24:
            return VLC_EINVAL;
        case VLC_CODEC_BGR565LE:
            biBitCount = 16;
            break;
        case VLC_CODEC_BGR555LE:
            biBitCount = 16;
            break;
        case VLC_CODEC_RGB16:
        case VLC_CODEC_RGB15:
            biBitCount = 16;
            biCompression = BI_BITFIELDS;
            break;
        case VLC_CODEC_RGBP:
        case VLC_CODEC_GREY:
            biBitCount = 8;
            break;
        case VLC_CODEC_MP4V:
            biCompression = VLC_FOURCC( 'X', 'V', 'I', 'D' );
            break;
        default:
            biCompression = fmt->i_original_fourcc
                ? fmt->i_original_fourcc : fmt->i_codec;
            break;
    }

    size_t i_bih_extra = 0;
    size_t i_bmiColors = 0;
    if( biCompression == BI_BITFIELDS )
        i_bmiColors = (b_has_alpha) ? 16 : 12;
    else if ( fmt->i_codec == VLC_CODEC_RGBP )
        i_bmiColors = fmt->video.p_palette ? (fmt->video.p_palette->i_entries * 4) : 0;
    else
        i_bih_extra = fmt->i_extra;

    *p_bih_extra = malloc( i_bih_extra + i_bmiColors );
    if( *p_bih_extra == NULL && (i_bih_extra + i_bmiColors) != 0 )
        return VLC_ENOMEM;

    uint8_t *p_bmiColors = *p_bih_extra;
    p_bih->biClrUsed = 0;
    if( biCompression == BI_BITFIELDS )
    {
        uint32_t i_rmask,i_gmask,i_bmask, i_amask;
        int ret = vlc_RGBChromaToMask( fmt->i_codec, &i_rmask, &i_gmask,
                                       &i_bmask, &i_amask );
        if (ret != VLC_SUCCESS)
        {
            i_rmask = fmt->video.i_rmask;
            i_gmask = fmt->video.i_gmask;
            i_bmask = fmt->video.i_bmask;
        }
        SetDWLE( &p_bmiColors[0], i_rmask );
        SetDWLE( &p_bmiColors[4], i_gmask );
        SetDWLE( &p_bmiColors[8], i_bmask );
        if( b_has_alpha )
        {
            SetDWLE( &p_bmiColors[12], i_amask );
        }
    }
    else if( fmt->i_codec == VLC_CODEC_RGBP )
    {
        for( int i = 0; i < fmt->video.p_palette->i_entries; i++ )
            memcpy( &p_bmiColors[i * 4], fmt->video.p_palette->palette[i], 4 );
        p_bih->biClrUsed = fmt->video.p_palette->i_entries;
    }
    else if( fmt->i_extra )
    {
        memcpy( *p_bih_extra, fmt->p_extra, fmt->i_extra );
    }

    p_bih->biSize = sizeof(VLC_BITMAPINFOHEADER) + i_bih_extra;
    p_bih->biCompression = biCompression;
    p_bih->biBitCount = biBitCount;
    p_bih->biWidth = fmt->video.i_visible_width;
    p_bih->biHeight = fmt->video.i_visible_height;
    p_bih->biPlanes = 1;
    p_bih->biSizeImage = 0;
    p_bih->biXPelsPerMeter = 0;
    p_bih->biYPelsPerMeter = 0;
    p_bih->biClrImportant = 0;

    *pi_total = i_bih_extra + i_bmiColors;
    return VLC_SUCCESS;
}
