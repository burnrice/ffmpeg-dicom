/*
 * DICOM header
 *
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

/**
 * @file
 * DICOM header
 */

#ifndef AVFORMAT_DICOM_H
#define AVFORMAT_DICOM_H

#define DICOM_TRANSFER_SYNTAX_MAXSIZE 24 // must be even
#define DICOM_CODEC_MAXSIZE 5
#define DICOM_VR_CS_MAXSIZE 16

enum {
    DICOM_ENDIAN_LE = 0,
    DICOM_ENDIAN_BE = 1,
};

enum {
    DICOM_VR_IMPLICIT = 0,
    DICOM_VR_EXPLICIT = 1,
};

enum {
    DICOM_COMPRESSION_NONE = 0,
    DICOM_COMPRESSION_DEFLATE,
    DICOM_COMPRESSION_RLE,
};

typedef struct DICOMTransferSyntax {
    char name[DICOM_TRANSFER_SYNTAX_MAXSIZE + 1];
    int type;
    char codec[DICOM_CODEC_MAXSIZE + 1];
} DICOMTransferSyntax;

static const DICOMTransferSyntax dicom_transfer_syntax[] = {
    {"1.2.840.10008.1.2",       0,    "none"},
    {"1.2.840.10008.1.2.1",     1,    "none"},
    {"1.2.840.10008.1.2.1.99",  199,  "none"},
    {"1.2.840.10008.1.2.2",     2,    "none"},
    {"1.2.840.10008.1.2.4.50",  450,  "none"},
    {"1.2.840.10008.1.2.4.51",  451,  "none"},
    {"1.2.840.10008.1.2.4.52",  452,  "none"},
    {"1.2.840.10008.1.2.4.53",  453,  "none"},
    {"1.2.840.10008.1.2.4.54",  454,  "none"},
    {"1.2.840.10008.1.2.4.55",  455,  "none"},
    {"1.2.840.10008.1.2.4.56",  456,  "none"},
    {"1.2.840.10008.1.2.4.57",  457,  "none"},
    {"1.2.840.10008.1.2.4.58",  458,  "none"},
    {"1.2.840.10008.1.2.4.59",  459,  "none"},
    {"1.2.840.10008.1.2.4.60",  460,  "none"},
    {"1.2.840.10008.1.2.4.61",  461,  "none"},
    {"1.2.840.10008.1.2.4.62",  462,  "none"},
    {"1.2.840.10008.1.2.4.63",  463,  "none"},
    {"1.2.840.10008.1.2.4.64",  464,  "none"},
    {"1.2.840.10008.1.2.4.65",  465,  "none"},
    {"1.2.840.10008.1.2.4.66",  466,  "none"},
    {"1.2.840.10008.1.2.4.67",  467,  "none"},
    {"1.2.840.10008.1.2.4.68",  468,  "none"},
    {"1.2.840.10008.1.2.4.69",  469,  "none"},
    {"1.2.840.10008.1.2.4.70",  470,  "none"},
    {"1.2.840.10008.1.2.4.80",  480,  "none"},
    {"1.2.840.10008.1.2.4.81",  481,  "none"},
    {"1.2.840.10008.1.2.4.90",  490,  "none"},
    {"1.2.840.10008.1.2.4.91",  491,  "none"},
    {"1.2.840.10008.1.2.4.92",  492,  "none"},
    {"1.2.840.10008.1.2.4.93",  493,  "none"},
    {"1.2.840.10008.1.2.4.94",  494,  "none"},
    {"1.2.840.10008.1.2.4.95",  495,  "none"},
    {"1.2.840.10008.1.2.5",     5,    "none"},
    {"1.2.840.10008.1.2.6.1",   61,   "none"},
    {"1.2.840.10008.1.2.4.100", 4100, "none"},
    {"1.2.840.10008.1.2.4.102", 4102, "none"},
    {"1.2.840.10008.1.2.4.103", 4103, "none"}
};

#endif /* AVFORMAT_DICOM_H */
