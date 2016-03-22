/*
 * DICOM demuxer
 * Copyright (c) 2016 Patryk Balicki
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#include "avformat.h"
#include "internal.h"

#include "dicom.h"

typedef struct DICOMContext {
    int endian;
    int vr_explicit;
    int compression;
    DICOMTransferSyntax syntax;
} DICOMContext;

static uint32_t dicom_r16(AVIOContext *s, DICOMContext *d){
    return d->endian ? avio_rb16(s) : avio_rl16(s);}
//static uint32_t dicom_r24(AVIOContext *s, DICOMContext *d){
//    return d->endian ? avio_rb24(s) : avio_rl24(s);}
static uint32_t dicom_r32(AVIOContext *s, DICOMContext *d){
    return d->endian ? avio_rb32(s) : avio_rl32(s);}
//static uint64_t dicom_r64(AVIOContext *s, DICOMContext *d){
//    return d->endian ? avio_rb64(s) : avio_rl64(s);}

static int dicom_probe(AVProbeData *p)
{
    if(memcmp(p->buf+0x80, "DICM", 4))
        return 0;
    return AVPROBE_SCORE_MAX;
}

static int dicom_parse_syntax(DICOMContext *d)
{
    switch(d->syntax.type){
    case 0:
        d->vr_explicit = DICOM_VR_IMPLICIT;
        break;
    case 199:
        d->compression = DICOM_COMPRESSION_DEFLATE;
        return AVERROR_PATCHWELCOME;
//        break;
    case 2:
        d->endian = DICOM_ENDIAN_BE;
        break;
    case 5:
        d->compression = DICOM_COMPRESSION_RLE;
        break;
    }
    return 0;
}

static uint64_t dicom_get_next_element(AVFormatContext *s, DICOMContext *d);

static int dicom_read_transfer_syntax(AVFormatContext *s, DICOMContext *d)
{
    int length, i;
    char syntax[DICOM_TRANSFER_SYNTAX_MAXSIZE + 1];

    avio_skip(s->pb, 2);
    length = avio_rl16(s->pb);
    avio_read(s->pb, syntax, length);

    for(i = 0; strcmp(dicom_transfer_syntax[i].name, syntax); i++);
    if(i == sizeof(dicom_transfer_syntax))
        return AVERROR(EINVAL);
    d->syntax = dicom_transfer_syntax[i];

    av_log(s, AV_LOG_INFO, "TransferSyntax: %d %s\n", d->syntax.type, d->syntax.name);
    return 0;
}

static uint32_t dicom_read_element_length(AVFormatContext *s, DICOMContext *d)
{
    if(d->vr_explicit){
        char vr[2];
        avio_read(s-> pb, vr, 2);

        if((memcmp(vr, "OB", 2) && memcmp(vr, "OW", 2) &&
            memcmp(vr, "OF", 2) && memcmp(vr, "SQ", 2) &&
            memcmp(vr, "UT", 2) && memcmp(vr, "UN", 2))){

            return dicom_r16(s->pb, d);
        }
        avio_skip(s->pb, 2);
    }
    return dicom_r32(s->pb, d);
}

static uint64_t dicom_nested_data(AVFormatContext *s, DICOMContext *d)
{
    uint16_t group, element;

    while(!avio_feof(s->pb))
    {
        group = dicom_r16(s->pb, d);
        element = dicom_r16(s->pb, d);

        if(group == 0xFFFE && element == 0xE0DD)
            return avio_skip(s->pb, 4);

        dicom_get_next_element(s, d);
    }
    return 0;
}

static uint64_t dicom_get_next_element(AVFormatContext *s, DICOMContext *d)
{
    uint32_t vl = dicom_read_element_length(s, d);

    if(vl != 0xffffffff)
        return avio_skip(s->pb, vl);
    return dicom_nested_data(s, d);
}

static int dicom_read_metadata(AVFormatContext *s, DICOMContext *d, uint16_t element)
{
    char data[1024]; // ST max size
    uint32_t vl = dicom_read_element_length(s, d);
    switch(element){
    case 0x0002:
        av_log(s, AV_LOG_INFO, "Samples per Pixel: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x0003:
        av_log(s, AV_LOG_INFO, "Samples per Pixel Used: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x0004:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Photometric Interpretation: %s\n", data);
        break;
    case 0x0005:
        av_log(s, AV_LOG_INFO, "Image Dimensions: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x0006:
        av_log(s, AV_LOG_INFO, "Planar Configuration: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x0008:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Number of Frames: %s\n", data);
        break;
    case 0x0009:
        av_log(s, AV_LOG_INFO, "Frame Increment Pointer: (%d),(%d)\n", dicom_r16(s->pb, d), dicom_r16(s->pb, d));
        break;
    case 0x000A:
        av_log(s, AV_LOG_INFO, "Frame Dimension Pointer: (%d),(%d)\n", dicom_r16(s->pb, d), dicom_r16(s->pb, d));
        break;
    case 0x0010:
        av_log(s, AV_LOG_INFO, "Rows: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x0011:
        av_log(s, AV_LOG_INFO, "Columns: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x0012:
        av_log(s, AV_LOG_INFO, "Planes: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x0014:
        av_log(s, AV_LOG_INFO, "Ultrasound Color Data Present: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x0030:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Pixel Spacing: %s\n", data);
        break;
    case 0x0031:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Zoom Factor: %s\n", data);
        break;
    case 0x0032:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Zoom Center: %s\n", data);
        break;
    case 0x0034:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Pixel Aspect Ratio: %s\n", data);
        break;
    case 0x0040:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Image Format: %s\n", data);
        break;
    case 0x0050:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Manipulated Image: %s\n", data);
        break;
    case 0x0051:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Corrected Image: %s\n", data);
        break;
    case 0x005F:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Compression Recognition Code: %s\n", data);
        break;
    case 0x0060:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Compression Code: %s\n", data);
        break;
    case 0x0061:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Compression Originator: %s\n", data);
        break;
    case 0x0062:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Compression Label: %s\n", data);
        break;
    case 0x0063:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Compression Description: %s\n", data);
        break;
    case 0x0065:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Compression Sequence: %s\n", data);
        break;
    case 0x0066:
        av_log(s, AV_LOG_INFO, "Compression Step Pointers: (%d),(%d)\n", dicom_r16(s->pb, d), dicom_r16(s->pb, d));
        break;
    case 0x0068:
        av_log(s, AV_LOG_INFO, "Repeat Interval: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x0069:
        av_log(s, AV_LOG_INFO, "Bits Grouped: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x0070:
        av_log(s, AV_LOG_INFO, "Perimeter Table: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x0071:
        av_log(s, AV_LOG_INFO, "Perimeter Value: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x0080:
        av_log(s, AV_LOG_INFO, "Predictor Rows: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x0081:
        av_log(s, AV_LOG_INFO, "Predictor Columns: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x0082:
        av_log(s, AV_LOG_INFO, "Predictor Constants: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x0090:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Blocked Pixels: %s\n", data);
        break;
    case 0x0091:
        av_log(s, AV_LOG_INFO, "Block Rows: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x0092:
        av_log(s, AV_LOG_INFO, "Block Columns: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x0093:
        av_log(s, AV_LOG_INFO, "Row Overlap: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x0094:
        av_log(s, AV_LOG_INFO, "Column Overlap: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x0100:
        av_log(s, AV_LOG_INFO, "Bits Allocated: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x0101:
        av_log(s, AV_LOG_INFO, "Bits Stored: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x0102:
        av_log(s, AV_LOG_INFO, "High Bit: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x0103:
        av_log(s, AV_LOG_INFO, "Pixel Representation: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x0104:
        av_log(s, AV_LOG_INFO, "Smallest Valid Pixel Value: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x0105:
        av_log(s, AV_LOG_INFO, "Largest Valid Pixel Value: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x0106:
        av_log(s, AV_LOG_INFO, "Smallest Image Pixel Value: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x0107:
        av_log(s, AV_LOG_INFO, "Largest Image Pixel Value: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x0108:
        av_log(s, AV_LOG_INFO, "Smallest Pixel Value in Series: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x0109:
        av_log(s, AV_LOG_INFO, "Largest Pixel Value in Series: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x0110:
        av_log(s, AV_LOG_INFO, "Smallest Image Pixel Value in Plane: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x0111:
        av_log(s, AV_LOG_INFO, "Largest Image Pixel Value in Plane: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x0120:
        av_log(s, AV_LOG_INFO, "Pixel Padding Value: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x0121:
        av_log(s, AV_LOG_INFO, "Pixel Padding Range Limit: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x0200:
        av_log(s, AV_LOG_INFO, "Image Location: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x0300:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Quality Control Image: %s\n", data);
        break;
    case 0x0301:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Burned In Annotation: %s\n", data);
        break;
    case 0x0302:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Recognizable Visual Features: %s\n", data);
        break;
    case 0x0303:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Longitudinal Temporal Information Modified: %s\n", data);
        break;
    case 0x0304:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Referenced Color Palette Instance UID: %s\n", data);
        break;
    case 0x0400:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Transform Label: %s\n", data);
        break;
    case 0x0401:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Transform Version Number: %s\n", data);
        break;
    case 0x0402:
        av_log(s, AV_LOG_INFO, "Number of Transform Steps: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x0403:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Sequence of Compressed Data: %s\n", data);
        break;
    case 0x0404:
        av_log(s, AV_LOG_INFO, "Details of Coefficients: (%d),(%d)\n", dicom_r16(s->pb, d), dicom_r16(s->pb, d));
        break;
    case 0x0700:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "DCT Label: %s\n", data);
        break;
    case 0x0701:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Data Block Description: %s\n", data);
        break;
    case 0x0702:
        av_log(s, AV_LOG_INFO, "Data Block: (%d),(%d)\n", dicom_r16(s->pb, d), dicom_r16(s->pb, d));
        break;
    case 0x0710:
        av_log(s, AV_LOG_INFO, "Normalization Factor Format: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x0720:
        av_log(s, AV_LOG_INFO, "Zonal Map Number Format: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x0721:
        av_log(s, AV_LOG_INFO, "Zonal Map Location: (%d),(%d)\n", dicom_r16(s->pb, d), dicom_r16(s->pb, d));
        break;
    case 0x0722:
        av_log(s, AV_LOG_INFO, "Zonal Map Format: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x0730:
        av_log(s, AV_LOG_INFO, "Adaptive Map Format: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x0740:
        av_log(s, AV_LOG_INFO, "Code Number Format: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x0A02:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Pixel Spacing Calibration Type: %s\n", data);
        break;
    case 0x0A04:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Pixel Spacing Calibration Description: %s\n", data);
        break;
    case 0x1040:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Pixel Intensity Relationship: %s\n", data);
        break;
    case 0x1041:
        av_log(s, AV_LOG_INFO, "Pixel Intensity Relationship Sign: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x1050:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Window Center: %s\n", data);
        break;
    case 0x1051:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Window Width: %s\n", data);
        break;
    case 0x1052:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Rescale Intercept: %s\n", data);
        break;
    case 0x1053:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Rescale Slope: %s\n", data);
        break;
    case 0x1054:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Rescale Type: %s\n", data);
        break;
    case 0x1055:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Window Center & Width Explanation: %s\n", data);
        break;
    case 0x1056:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "VOI LUT Function: %s\n", data);
        break;
    case 0x1080:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Gray Scale: %s\n", data);
        break;
    case 0x1090:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Recommended Viewing Mode: %s\n", data);
        break;
    case 0x1100:
        av_log(s, AV_LOG_INFO, "Gray Lookup Table Descriptor: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x1101:
        av_log(s, AV_LOG_INFO, "Red Palette Color Lookup Table Descriptor: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x1102:
        av_log(s, AV_LOG_INFO, "Green Palette Color Lookup Table Descriptor: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x1103:
        av_log(s, AV_LOG_INFO, "Blue Palette Color Lookup Table Descriptor: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x1104:
        av_log(s, AV_LOG_INFO, "Alpha Palette Color Lookup Table Descriptor: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x1111:
        av_log(s, AV_LOG_INFO, "Large Red Palette Color Lookup Table Descriptor: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x1112:
        av_log(s, AV_LOG_INFO, "Large Green Palette Color Lookup Table Descriptor: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x1113:
        av_log(s, AV_LOG_INFO, "Large Blue Palette Color Lookup Table Descriptor: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x1199:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Palette Color Lookup Table UID: %s\n", data);
        break;
    case 0x1214:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Large Palette Color Lookup Table UID: %s\n", data);
        break;
    case 0x1300:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Breast Implant Present: %s\n", data);
        break;
    case 0x1350:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Partial View: %s\n", data);
        break;
    case 0x1351:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Partial View Description: %s\n", data);
        break;
    case 0x135A:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Spatial Locations Preserved: %s\n", data);
        break;
    case 0x1402:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Data Path Assignment: %s\n", data);
        break;
    case 0x1403:
        av_log(s, AV_LOG_INFO, "Bits Mapped to Color Lookup Table: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x1405:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Blending LUT 1 Transfer Function: %s\n", data);
        break;
    case 0x1407:
        av_log(s, AV_LOG_INFO, "Blending Lookup Table Descriptor: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x140D:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Blending LUT 2 Transfer Function: %s\n", data);
        break;
    case 0x140E:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Data Path ID: %s\n", data);
        break;
    case 0x140F:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "RGB LUT Transfer Function: %s\n", data);
        break;
    case 0x1410:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Alpha LUT Transfer Function: %s\n", data);
        break;
    case 0x2002:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Color Space: %s\n", data);
        break;
    case 0x2110:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Lossy Image Compression: %s\n", data);
        break;
    case 0x2112:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Lossy Image Compression Ratio: %s\n", data);
        break;
    case 0x2114:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Lossy Image Compression Method: %s\n", data);
        break;
    case 0x3002:
        av_log(s, AV_LOG_INFO, "LUT Descriptor: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x3003:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "LUT Explanation: %s\n", data);
        break;
    case 0x3004:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Modality LUT Type: %s\n", data);
        break;
    case 0x4000:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Image Presentation Comments: %s\n", data);
        break;
    case 0x6010:
        av_log(s, AV_LOG_INFO, "Representative Frame Number: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x6020:
        av_log(s, AV_LOG_INFO, "Frame Numbers of Interest (FOI): %d\n", dicom_r16(s->pb, d));
        break;
    case 0x6022:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Frame of Interest Description: %s\n", data);
        break;
    case 0x6023:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Frame of Interest Type: %s\n", data);
        break;
    case 0x6030:
        av_log(s, AV_LOG_INFO, "Mask Pointer(s): %d\n", dicom_r16(s->pb, d));
        break;
    case 0x6040:
        av_log(s, AV_LOG_INFO, "R Wave Pointer: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x6101:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Mask Operation: %s\n", data);
        break;
    case 0x6102:
        av_log(s, AV_LOG_INFO, "Applicable Frame Range: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x6110:
        av_log(s, AV_LOG_INFO, "Mask Frame Numbers: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x6112:
        av_log(s, AV_LOG_INFO, "Contrast Frame Averaging: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x6120:
        av_log(s, AV_LOG_INFO, "TID Offset: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x6190:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Mask Operation Explanation: %s\n", data);
        break;
    case 0x7001:
        av_log(s, AV_LOG_INFO, "Number of Display Subsystems: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x7002:
        av_log(s, AV_LOG_INFO, "Current Configuration ID: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x7003:
        av_log(s, AV_LOG_INFO, "Display Subsystem ID: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x7004:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Display Subsystem Name: %s\n", data);
        break;
    case 0x7005:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Display Subsystem Description: %s\n", data);
        break;
    case 0x7006:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "System Status: %s\n", data);
        break;
    case 0x7007:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "System Status Comment: %s\n", data);
        break;
    case 0x7009:
        av_log(s, AV_LOG_INFO, "Luminance Characteristics ID: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x700B:
        av_log(s, AV_LOG_INFO, "Configuration ID: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x700C:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Configuration Name: %s\n", data);
        break;
    case 0x700D:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Configuration Description: %s\n", data);
        break;
    case 0x700E:
        av_log(s, AV_LOG_INFO, "Referenced Target Luminance Characteristics ID: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x7013:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Measurement Functions: %s\n", data);
        break;
    case 0x7014:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Measurement Equipment Type: %s\n", data);
        break;
    case 0x7017:
        av_log(s, AV_LOG_INFO, "DDL Value: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x7019:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Display Function Type: %s\n", data);
        break;
    case 0x701B:
        av_log(s, AV_LOG_INFO, "Number of Luminance Points: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x7020:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Luminance Response Description: %s\n", data);
        break;
    case 0x7021:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "White Point Flag: %s\n", data);
        break;
    case 0x7025:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Ambient Light Value Source: %s\n", data);
        break;
    case 0x7026:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Measured Characteristics: %s\n", data);
        break;
    case 0x7029:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Test Result: %s\n", data);
        break;
    case 0x702A:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Test Result Comment: %s\n", data);
        break;
    case 0x702B:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Test Image Validation: %s\n", data);
        break;
    case 0x9001:
        av_log(s, AV_LOG_INFO, "Data Point Rows: %d\n", dicom_r32(s->pb, d));
        break;
    case 0x9002:
        av_log(s, AV_LOG_INFO, "Data Point Columns: %d\n", dicom_r32(s->pb, d));
        break;
    case 0x9003:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Signal Domain Columns: %s\n", data);
        break;
    case 0x9099:
        av_log(s, AV_LOG_INFO, "Largest Monochrome Pixel Value: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x9108:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Data Representation: %s\n", data);
        break;
    case 0x9235:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Signal Domain Rows: %s\n", data);
        break;
    case 0x9416:
        av_log(s, AV_LOG_INFO, "Subtraction Item ID: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x9444:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Geometrical Properties: %s\n", data);
        break;
    case 0x9446:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Image Processing Applied: %s\n", data);
        break;
    case 0x9454:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Mask Selection Mode: %s\n", data);
        break;
    case 0x9474:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "LUT Function: %s\n", data);
        break;
    case 0x9503:
        av_log(s, AV_LOG_INFO, "Vertices of the Region: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x9506:
        av_log(s, AV_LOG_INFO, "Pixel Shift Frame Range: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x9507:
        av_log(s, AV_LOG_INFO, "LUT Frame Range: %d\n", dicom_r16(s->pb, d));
        break;
    case 0x9520:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Image to Equipment Mapping Matrix: %s\n", data);
        break;
    case 0x9537:
        avio_read(s->pb, data, vl);
        av_log(s, AV_LOG_INFO, "Equipment Coordinate System Identification: %s\n", data);
        break;
    default:
        if(vl == 0xffffffff)
            dicom_nested_data(s, d);
        else
            avio_skip(s->pb, vl);
    }
    return 0;
}

static int dicom_read_header(AVFormatContext *s)
{
    int err;
    uint16_t group, element;
    DICOMContext *d = s->priv_data;

    d->endian = DICOM_ENDIAN_LE;
    d->vr_explicit = DICOM_VR_EXPLICIT;
    d->compression = DICOM_COMPRESSION_NONE;

    avio_skip(s->pb, 0x84);
    group = avio_rl16(s->pb);
    element = avio_rl16(s->pb);

    while(!avio_feof(s->pb) && group == 0x0002){

        av_log(s, AV_LOG_TRACE, "%x %x\n", group, element);

        if(element == 0x0010){
            if(err = dicom_read_transfer_syntax(s, d))
                return err;
        } else
            if(!dicom_get_next_element(s, d))
                break;

        group = avio_rl16(s->pb);
        element = avio_rl16(s->pb);
    }

    dicom_parse_syntax(d);
    if(!d->vr_explicit){
        group = (group << 8) & 0xff00 + (group >> 8);
        element = (element << 8) & 0xff00 + (element >> 8);
    }

    while(!avio_feof(s->pb))
    {
        av_log(s, AV_LOG_TRACE, "%x %x\n", group, element);

        if(group == 0x0028){
            if(err = dicom_read_metadata(s, d, element))
                return err;
        } else if(group == 0x7fe0 && element == 0x0010)
            return 0;
        else
            if(!dicom_get_next_element(s, d))
                break;

        group = dicom_r16(s->pb, d);
        element = dicom_r16(s->pb, d);
    }

    return AVERROR(EINVAL);
}


static int dicom_read_packet(AVFormatContext *s, AVPacket *pkt)
{
    return -1;
}

AVInputFormat ff_dicom_demuxer = {
    .name           = "dicom",
    .long_name      = NULL_IF_CONFIG_SMALL("DICOM"),
    .priv_data_size = sizeof(DICOMContext),
    .read_probe     = dicom_probe,
    .read_header    = dicom_read_header,
    .read_packet    = dicom_read_packet,
};
