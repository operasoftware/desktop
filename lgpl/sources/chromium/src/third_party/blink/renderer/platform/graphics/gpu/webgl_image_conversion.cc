// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/webgl_image_conversion.h"

#include <limits>
#include <memory>

#include "base/compiler_specific.h"
#include "base/numerics/checked_math.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/graphics/cpu/arm/webgl_image_conversion_neon.h"
#include "third_party/blink/renderer/platform/graphics/cpu/mips/webgl_image_conversion_msa.h"
#include "third_party/blink/renderer/platform/graphics/cpu/x86/webgl_image_conversion_sse.h"
#include "third_party/blink/renderer/platform/graphics/image_observer.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/skia/include/core/SkImage.h"

namespace blink {

namespace {

const float kMaxInt8Value = INT8_MAX;
const float kMaxUInt8Value = UINT8_MAX;
const float kMaxInt16Value = INT16_MAX;
const float kMaxUInt16Value = UINT16_MAX;
const double kMaxInt32Value = INT32_MAX;
const double kMaxUInt32Value = UINT32_MAX;

int8_t ClampMin(int8_t value) {
  const static int8_t kMinInt8Value = INT8_MIN + 1;
  return value < kMinInt8Value ? kMinInt8Value : value;
}

int16_t ClampMin(int16_t value) {
  const static int16_t kMinInt16Value = INT16_MIN + 1;
  return value < kMinInt16Value ? kMinInt16Value : value;
}

int32_t ClampMin(int32_t value) {
  const static int32_t kMinInt32Value = INT32_MIN + 1;
  return value < kMinInt32Value ? kMinInt32Value : value;
}

template <class T>
T ClampImpl(const float& v, const T& lo, const T& hi) {
  return (v < lo) ? lo : ((hi < v) ? hi : static_cast<T>(v));
}

template <class T>
T ClampFloat(float value) {
  if (std::numeric_limits<T>::is_signed) {
    // Generate an equal number of positive and negative values. Two's
    // complement has one more negative number than positive number.
    return ClampImpl<T>(value, std::numeric_limits<T>::min() + 1,
                        std::numeric_limits<T>::max());
  } else {
    return ClampImpl<T>(value, std::numeric_limits<T>::min(),
                        std::numeric_limits<T>::max());
  }
}

template <class T>
T ClampAndScaleFloat(float value) {
  return ClampFloat<T>(value * std::numeric_limits<T>::max());
}

// Return kDataFormatNumFormats if format/type combination is invalid.
WebGLImageConversion::DataFormat GetDataFormat(GLenum destination_format,
                                               GLenum destination_type) {
  WebGLImageConversion::DataFormat dst_format =
      WebGLImageConversion::kDataFormatRGBA8;
  switch (destination_type) {
    case GL_BYTE:
      switch (destination_format) {
        case GL_RED:
        case GL_RED_INTEGER:
          dst_format = WebGLImageConversion::kDataFormatR8_S;
          break;
        case GL_RG:
        case GL_RG_INTEGER:
          dst_format = WebGLImageConversion::kDataFormatRG8_S;
          break;
        case GL_RGB:
        case GL_RGB_INTEGER:
          dst_format = WebGLImageConversion::kDataFormatRGB8_S;
          break;
        case GL_RGBA:
        case GL_RGBA_INTEGER:
          dst_format = WebGLImageConversion::kDataFormatRGBA8_S;
          break;
        default:
          return WebGLImageConversion::kDataFormatNumFormats;
      }
      break;
    case GL_UNSIGNED_BYTE:
      switch (destination_format) {
        case GL_RGB:
        case GL_RGB_INTEGER:
        case GL_SRGB_EXT:
          dst_format = WebGLImageConversion::kDataFormatRGB8;
          break;
        case GL_RGBA:
        case GL_RGBA_INTEGER:
        case GL_SRGB_ALPHA_EXT:
          dst_format = WebGLImageConversion::kDataFormatRGBA8;
          break;
        case GL_ALPHA:
          dst_format = WebGLImageConversion::kDataFormatA8;
          break;
        case GL_LUMINANCE:
        case GL_RED:
        case GL_RED_INTEGER:
          dst_format = WebGLImageConversion::kDataFormatR8;
          break;
        case GL_RG:
        case GL_RG_INTEGER:
          dst_format = WebGLImageConversion::kDataFormatRG8;
          break;
        case GL_LUMINANCE_ALPHA:
          dst_format = WebGLImageConversion::kDataFormatRA8;
          break;
        default:
          return WebGLImageConversion::kDataFormatNumFormats;
      }
      break;
    case GL_SHORT:
      switch (destination_format) {
        case GL_RED_INTEGER:
          dst_format = WebGLImageConversion::kDataFormatR16_S;
          break;
        case GL_RG_INTEGER:
          dst_format = WebGLImageConversion::kDataFormatRG16_S;
          break;
        case GL_RGB_INTEGER:
          dst_format = WebGLImageConversion::kDataFormatRGB16_S;
          break;
        case GL_RGBA_INTEGER:
          dst_format = WebGLImageConversion::kDataFormatRGBA16_S;
          break;
        default:
          return WebGLImageConversion::kDataFormatNumFormats;
      }
      break;
    case GL_UNSIGNED_SHORT:
      switch (destination_format) {
        case GL_RED_INTEGER:
          dst_format = WebGLImageConversion::kDataFormatR16;
          break;
        case GL_DEPTH_COMPONENT:
          dst_format = WebGLImageConversion::kDataFormatD16;
          break;
        case GL_RG_INTEGER:
          dst_format = WebGLImageConversion::kDataFormatRG16;
          break;
        case GL_RGB_INTEGER:
          dst_format = WebGLImageConversion::kDataFormatRGB16;
          break;
        case GL_RGBA_INTEGER:
          dst_format = WebGLImageConversion::kDataFormatRGBA16;
          break;
        default:
          return WebGLImageConversion::kDataFormatNumFormats;
      }
      break;
    case GL_INT:
      switch (destination_format) {
        case GL_RED_INTEGER:
          dst_format = WebGLImageConversion::kDataFormatR32_S;
          break;
        case GL_RG_INTEGER:
          dst_format = WebGLImageConversion::kDataFormatRG32_S;
          break;
        case GL_RGB_INTEGER:
          dst_format = WebGLImageConversion::kDataFormatRGB32_S;
          break;
        case GL_RGBA_INTEGER:
          dst_format = WebGLImageConversion::kDataFormatRGBA32_S;
          break;
        default:
          return WebGLImageConversion::kDataFormatNumFormats;
      }
      break;
    case GL_UNSIGNED_INT:
      switch (destination_format) {
        case GL_RED_INTEGER:
          dst_format = WebGLImageConversion::kDataFormatR32;
          break;
        case GL_DEPTH_COMPONENT:
          dst_format = WebGLImageConversion::kDataFormatD32;
          break;
        case GL_RG_INTEGER:
          dst_format = WebGLImageConversion::kDataFormatRG32;
          break;
        case GL_RGB_INTEGER:
          dst_format = WebGLImageConversion::kDataFormatRGB32;
          break;
        case GL_RGBA_INTEGER:
          dst_format = WebGLImageConversion::kDataFormatRGBA32;
          break;
        default:
          return WebGLImageConversion::kDataFormatNumFormats;
      }
      break;
    case GL_HALF_FLOAT_OES:  // OES_texture_half_float
    case GL_HALF_FLOAT:
      switch (destination_format) {
        case GL_RGBA:
          dst_format = WebGLImageConversion::kDataFormatRGBA16F;
          break;
        case GL_RGB:
          dst_format = WebGLImageConversion::kDataFormatRGB16F;
          break;
        case GL_RG:
          dst_format = WebGLImageConversion::kDataFormatRG16F;
          break;
        case GL_ALPHA:
          dst_format = WebGLImageConversion::kDataFormatA16F;
          break;
        case GL_LUMINANCE:
        case GL_RED:
          dst_format = WebGLImageConversion::kDataFormatR16F;
          break;
        case GL_LUMINANCE_ALPHA:
          dst_format = WebGLImageConversion::kDataFormatRA16F;
          break;
        default:
          return WebGLImageConversion::kDataFormatNumFormats;
      }
      break;
    case GL_FLOAT:  // OES_texture_float
      switch (destination_format) {
        case GL_RGBA:
          dst_format = WebGLImageConversion::kDataFormatRGBA32F;
          break;
        case GL_RGB:
          dst_format = WebGLImageConversion::kDataFormatRGB32F;
          break;
        case GL_RG:
          dst_format = WebGLImageConversion::kDataFormatRG32F;
          break;
        case GL_ALPHA:
          dst_format = WebGLImageConversion::kDataFormatA32F;
          break;
        case GL_LUMINANCE:
        case GL_RED:
          dst_format = WebGLImageConversion::kDataFormatR32F;
          break;
        case GL_DEPTH_COMPONENT:
          dst_format = WebGLImageConversion::kDataFormatD32F;
          break;
        case GL_LUMINANCE_ALPHA:
          dst_format = WebGLImageConversion::kDataFormatRA32F;
          break;
        default:
          return WebGLImageConversion::kDataFormatNumFormats;
      }
      break;
    case GL_UNSIGNED_SHORT_4_4_4_4:
      dst_format = WebGLImageConversion::kDataFormatRGBA4444;
      break;
    case GL_UNSIGNED_SHORT_5_5_5_1:
      dst_format = WebGLImageConversion::kDataFormatRGBA5551;
      break;
    case GL_UNSIGNED_SHORT_5_6_5:
      dst_format = WebGLImageConversion::kDataFormatRGB565;
      break;
    case GL_UNSIGNED_INT_5_9_9_9_REV:
      dst_format = WebGLImageConversion::kDataFormatRGB5999;
      break;
    case GL_UNSIGNED_INT_24_8:
      dst_format = WebGLImageConversion::kDataFormatDS24_8;
      break;
    case GL_UNSIGNED_INT_10F_11F_11F_REV:
      dst_format = WebGLImageConversion::kDataFormatRGB10F11F11F;
      break;
    case GL_UNSIGNED_INT_2_10_10_10_REV:
      dst_format = WebGLImageConversion::kDataFormatRGBA2_10_10_10;
      break;
    default:
      return WebGLImageConversion::kDataFormatNumFormats;
  }
  return dst_format;
}

// The following Float to Half-Float conversion code is from the implementation
// of http://www.fox-toolkit.org/ftp/fasthalffloatconversion.pdf , "Fast Half
// Float Conversions" by Jeroen van der Zijp, November 2008 (Revised September
// 2010). Specially, the basetable[512] and shifttable[512] are generated as
// follows:
/*
unsigned short basetable[512];
unsigned char shifttable[512];

void generatetables(){
    unsigned int i;
    int e;
    for (i = 0; i < 256; ++i){
        e = i - 127;
        if (e < -24){ // Very small numbers map to zero
            basetable[i | 0x000] = 0x0000;
            basetable[i | 0x100] = 0x8000;
            shifttable[i | 0x000] = 24;
            shifttable[i | 0x100] = 24;
        }
        else if (e < -14) { // Small numbers map to denorms
            basetable[i | 0x000] = (0x0400>>(-e-14));
            basetable[i | 0x100] = (0x0400>>(-e-14)) | 0x8000;
            shifttable[i | 0x000] = -e-1;
            shifttable[i | 0x100] = -e-1;
        }
        else if (e <= 15){ // Normal numbers just lose precision
            basetable[i | 0x000] = ((e+15)<<10);
            basetable[i| 0x100] = ((e+15)<<10) | 0x8000;
            shifttable[i|0x000] = 13;
            shifttable[i|0x100] = 13;
        }
        else if (e<128){ // Large numbers map to Infinity
            basetable[i|0x000] = 0x7C00;
            basetable[i|0x100] = 0xFC00;
            shifttable[i|0x000] = 24;
            shifttable[i|0x100] = 24;
        }
        else { // Infinity and NaN's stay Infinity and NaN's
            basetable[i|0x000] = 0x7C00;
            basetable[i|0x100] = 0xFC00;
            shifttable[i|0x000] = 13;
            shifttable[i|0x100] = 13;
       }
    }
}
*/

const uint16_t g_base_table[512] = {
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    0,     0,     0,     0,     1,     2,     4,     8,     16,    32,    64,
    128,   256,   512,   1024,  2048,  3072,  4096,  5120,  6144,  7168,  8192,
    9216,  10240, 11264, 12288, 13312, 14336, 15360, 16384, 17408, 18432, 19456,
    20480, 21504, 22528, 23552, 24576, 25600, 26624, 27648, 28672, 29696, 30720,
    31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744,
    31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744,
    31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744,
    31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744,
    31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744,
    31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744,
    31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744,
    31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744,
    31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744,
    31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744,
    31744, 31744, 31744, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768,
    32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768,
    32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768,
    32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768,
    32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768,
    32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768,
    32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768,
    32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768,
    32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768,
    32768, 32768, 32768, 32768, 32768, 32768, 32768, 32769, 32770, 32772, 32776,
    32784, 32800, 32832, 32896, 33024, 33280, 33792, 34816, 35840, 36864, 37888,
    38912, 39936, 40960, 41984, 43008, 44032, 45056, 46080, 47104, 48128, 49152,
    50176, 51200, 52224, 53248, 54272, 55296, 56320, 57344, 58368, 59392, 60416,
    61440, 62464, 63488, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512,
    64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512,
    64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512,
    64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512,
    64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512,
    64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512,
    64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512,
    64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512,
    64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512,
    64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512,
    64512, 64512, 64512, 64512, 64512, 64512};

const unsigned char g_shift_table[512] = {
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13,
    13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
    13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 13, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 23, 22,
    21, 20, 19, 18, 17, 16, 15, 14, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
    13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 13};

uint16_t ConvertFloatToHalfFloat(float f) {
  unsigned temp = *(reinterpret_cast<unsigned*>(&f));
  uint16_t signexp = (temp >> 23) & 0x1ff;
  return g_base_table[signexp] +
         ((temp & 0x007fffff) >> g_shift_table[signexp]);
}

// The mantissatable[2048], offsettable[64] and exponenttable[64] are
// generated as follows:
/*
unsigned int mantissatable[2048];
unsigned short offsettable[64];
unsigned int exponenttable[64];

unsigned int convertmantissa(unsigned int i) {
  unsigned int m=i<<13; // Zero pad mantissa bits
  unsigned int e=0; // Zero exponent
  while(!(m&0x00800000)){ // While not normalized
    e-=0x00800000; // Decrement exponent (1<<23)
    m<<=1; // Shift mantissa
  }
  m&=~0x00800000; // Clear leading 1 bit
  e+=0x38800000; // Adjust bias ((127-14)<<23)
  return m | e; // Return combined number
}

void generatef16tof32tables() {
  int i;
  mantissatable[0] = 0;
  for (i = 1; i <= 1023; ++i)
    mantissatable[i] = convertmantissa(i);
  for (i = 1024; i <= 2047; ++i)
    mantissatable[i] = 0x38000000 + ((i-1024)<<13);

  exponenttable[0] = 0;
  exponenttable[32]= 0x80000000;
  for (int i = 1; i <= 30; ++i)
    exponenttable[i] = i<<23;
  for (int i = 33; i <= 62; ++i)
    exponenttable[i] = 0x80000000 + ((i-32)<<23);
  exponenttable[31]= 0x47800000;
  exponenttable[63]= 0xC7800000;

  for (i = 0; i <= 63; ++i)
    offsettable[i] = 1024;
  offsettable[0] = 0;
  offsettable[32] = 0;
}
*/

const uint32_t g_mantissa_table[2048] = {
    0x0,        0x33800000, 0x34000000, 0x34400000, 0x34800000, 0x34a00000,
    0x34c00000, 0x34e00000, 0x35000000, 0x35100000, 0x35200000, 0x35300000,
    0x35400000, 0x35500000, 0x35600000, 0x35700000, 0x35800000, 0x35880000,
    0x35900000, 0x35980000, 0x35a00000, 0x35a80000, 0x35b00000, 0x35b80000,
    0x35c00000, 0x35c80000, 0x35d00000, 0x35d80000, 0x35e00000, 0x35e80000,
    0x35f00000, 0x35f80000, 0x36000000, 0x36040000, 0x36080000, 0x360c0000,
    0x36100000, 0x36140000, 0x36180000, 0x361c0000, 0x36200000, 0x36240000,
    0x36280000, 0x362c0000, 0x36300000, 0x36340000, 0x36380000, 0x363c0000,
    0x36400000, 0x36440000, 0x36480000, 0x364c0000, 0x36500000, 0x36540000,
    0x36580000, 0x365c0000, 0x36600000, 0x36640000, 0x36680000, 0x366c0000,
    0x36700000, 0x36740000, 0x36780000, 0x367c0000, 0x36800000, 0x36820000,
    0x36840000, 0x36860000, 0x36880000, 0x368a0000, 0x368c0000, 0x368e0000,
    0x36900000, 0x36920000, 0x36940000, 0x36960000, 0x36980000, 0x369a0000,
    0x369c0000, 0x369e0000, 0x36a00000, 0x36a20000, 0x36a40000, 0x36a60000,
    0x36a80000, 0x36aa0000, 0x36ac0000, 0x36ae0000, 0x36b00000, 0x36b20000,
    0x36b40000, 0x36b60000, 0x36b80000, 0x36ba0000, 0x36bc0000, 0x36be0000,
    0x36c00000, 0x36c20000, 0x36c40000, 0x36c60000, 0x36c80000, 0x36ca0000,
    0x36cc0000, 0x36ce0000, 0x36d00000, 0x36d20000, 0x36d40000, 0x36d60000,
    0x36d80000, 0x36da0000, 0x36dc0000, 0x36de0000, 0x36e00000, 0x36e20000,
    0x36e40000, 0x36e60000, 0x36e80000, 0x36ea0000, 0x36ec0000, 0x36ee0000,
    0x36f00000, 0x36f20000, 0x36f40000, 0x36f60000, 0x36f80000, 0x36fa0000,
    0x36fc0000, 0x36fe0000, 0x37000000, 0x37010000, 0x37020000, 0x37030000,
    0x37040000, 0x37050000, 0x37060000, 0x37070000, 0x37080000, 0x37090000,
    0x370a0000, 0x370b0000, 0x370c0000, 0x370d0000, 0x370e0000, 0x370f0000,
    0x37100000, 0x37110000, 0x37120000, 0x37130000, 0x37140000, 0x37150000,
    0x37160000, 0x37170000, 0x37180000, 0x37190000, 0x371a0000, 0x371b0000,
    0x371c0000, 0x371d0000, 0x371e0000, 0x371f0000, 0x37200000, 0x37210000,
    0x37220000, 0x37230000, 0x37240000, 0x37250000, 0x37260000, 0x37270000,
    0x37280000, 0x37290000, 0x372a0000, 0x372b0000, 0x372c0000, 0x372d0000,
    0x372e0000, 0x372f0000, 0x37300000, 0x37310000, 0x37320000, 0x37330000,
    0x37340000, 0x37350000, 0x37360000, 0x37370000, 0x37380000, 0x37390000,
    0x373a0000, 0x373b0000, 0x373c0000, 0x373d0000, 0x373e0000, 0x373f0000,
    0x37400000, 0x37410000, 0x37420000, 0x37430000, 0x37440000, 0x37450000,
    0x37460000, 0x37470000, 0x37480000, 0x37490000, 0x374a0000, 0x374b0000,
    0x374c0000, 0x374d0000, 0x374e0000, 0x374f0000, 0x37500000, 0x37510000,
    0x37520000, 0x37530000, 0x37540000, 0x37550000, 0x37560000, 0x37570000,
    0x37580000, 0x37590000, 0x375a0000, 0x375b0000, 0x375c0000, 0x375d0000,
    0x375e0000, 0x375f0000, 0x37600000, 0x37610000, 0x37620000, 0x37630000,
    0x37640000, 0x37650000, 0x37660000, 0x37670000, 0x37680000, 0x37690000,
    0x376a0000, 0x376b0000, 0x376c0000, 0x376d0000, 0x376e0000, 0x376f0000,
    0x37700000, 0x37710000, 0x37720000, 0x37730000, 0x37740000, 0x37750000,
    0x37760000, 0x37770000, 0x37780000, 0x37790000, 0x377a0000, 0x377b0000,
    0x377c0000, 0x377d0000, 0x377e0000, 0x377f0000, 0x37800000, 0x37808000,
    0x37810000, 0x37818000, 0x37820000, 0x37828000, 0x37830000, 0x37838000,
    0x37840000, 0x37848000, 0x37850000, 0x37858000, 0x37860000, 0x37868000,
    0x37870000, 0x37878000, 0x37880000, 0x37888000, 0x37890000, 0x37898000,
    0x378a0000, 0x378a8000, 0x378b0000, 0x378b8000, 0x378c0000, 0x378c8000,
    0x378d0000, 0x378d8000, 0x378e0000, 0x378e8000, 0x378f0000, 0x378f8000,
    0x37900000, 0x37908000, 0x37910000, 0x37918000, 0x37920000, 0x37928000,
    0x37930000, 0x37938000, 0x37940000, 0x37948000, 0x37950000, 0x37958000,
    0x37960000, 0x37968000, 0x37970000, 0x37978000, 0x37980000, 0x37988000,
    0x37990000, 0x37998000, 0x379a0000, 0x379a8000, 0x379b0000, 0x379b8000,
    0x379c0000, 0x379c8000, 0x379d0000, 0x379d8000, 0x379e0000, 0x379e8000,
    0x379f0000, 0x379f8000, 0x37a00000, 0x37a08000, 0x37a10000, 0x37a18000,
    0x37a20000, 0x37a28000, 0x37a30000, 0x37a38000, 0x37a40000, 0x37a48000,
    0x37a50000, 0x37a58000, 0x37a60000, 0x37a68000, 0x37a70000, 0x37a78000,
    0x37a80000, 0x37a88000, 0x37a90000, 0x37a98000, 0x37aa0000, 0x37aa8000,
    0x37ab0000, 0x37ab8000, 0x37ac0000, 0x37ac8000, 0x37ad0000, 0x37ad8000,
    0x37ae0000, 0x37ae8000, 0x37af0000, 0x37af8000, 0x37b00000, 0x37b08000,
    0x37b10000, 0x37b18000, 0x37b20000, 0x37b28000, 0x37b30000, 0x37b38000,
    0x37b40000, 0x37b48000, 0x37b50000, 0x37b58000, 0x37b60000, 0x37b68000,
    0x37b70000, 0x37b78000, 0x37b80000, 0x37b88000, 0x37b90000, 0x37b98000,
    0x37ba0000, 0x37ba8000, 0x37bb0000, 0x37bb8000, 0x37bc0000, 0x37bc8000,
    0x37bd0000, 0x37bd8000, 0x37be0000, 0x37be8000, 0x37bf0000, 0x37bf8000,
    0x37c00000, 0x37c08000, 0x37c10000, 0x37c18000, 0x37c20000, 0x37c28000,
    0x37c30000, 0x37c38000, 0x37c40000, 0x37c48000, 0x37c50000, 0x37c58000,
    0x37c60000, 0x37c68000, 0x37c70000, 0x37c78000, 0x37c80000, 0x37c88000,
    0x37c90000, 0x37c98000, 0x37ca0000, 0x37ca8000, 0x37cb0000, 0x37cb8000,
    0x37cc0000, 0x37cc8000, 0x37cd0000, 0x37cd8000, 0x37ce0000, 0x37ce8000,
    0x37cf0000, 0x37cf8000, 0x37d00000, 0x37d08000, 0x37d10000, 0x37d18000,
    0x37d20000, 0x37d28000, 0x37d30000, 0x37d38000, 0x37d40000, 0x37d48000,
    0x37d50000, 0x37d58000, 0x37d60000, 0x37d68000, 0x37d70000, 0x37d78000,
    0x37d80000, 0x37d88000, 0x37d90000, 0x37d98000, 0x37da0000, 0x37da8000,
    0x37db0000, 0x37db8000, 0x37dc0000, 0x37dc8000, 0x37dd0000, 0x37dd8000,
    0x37de0000, 0x37de8000, 0x37df0000, 0x37df8000, 0x37e00000, 0x37e08000,
    0x37e10000, 0x37e18000, 0x37e20000, 0x37e28000, 0x37e30000, 0x37e38000,
    0x37e40000, 0x37e48000, 0x37e50000, 0x37e58000, 0x37e60000, 0x37e68000,
    0x37e70000, 0x37e78000, 0x37e80000, 0x37e88000, 0x37e90000, 0x37e98000,
    0x37ea0000, 0x37ea8000, 0x37eb0000, 0x37eb8000, 0x37ec0000, 0x37ec8000,
    0x37ed0000, 0x37ed8000, 0x37ee0000, 0x37ee8000, 0x37ef0000, 0x37ef8000,
    0x37f00000, 0x37f08000, 0x37f10000, 0x37f18000, 0x37f20000, 0x37f28000,
    0x37f30000, 0x37f38000, 0x37f40000, 0x37f48000, 0x37f50000, 0x37f58000,
    0x37f60000, 0x37f68000, 0x37f70000, 0x37f78000, 0x37f80000, 0x37f88000,
    0x37f90000, 0x37f98000, 0x37fa0000, 0x37fa8000, 0x37fb0000, 0x37fb8000,
    0x37fc0000, 0x37fc8000, 0x37fd0000, 0x37fd8000, 0x37fe0000, 0x37fe8000,
    0x37ff0000, 0x37ff8000, 0x38000000, 0x38004000, 0x38008000, 0x3800c000,
    0x38010000, 0x38014000, 0x38018000, 0x3801c000, 0x38020000, 0x38024000,
    0x38028000, 0x3802c000, 0x38030000, 0x38034000, 0x38038000, 0x3803c000,
    0x38040000, 0x38044000, 0x38048000, 0x3804c000, 0x38050000, 0x38054000,
    0x38058000, 0x3805c000, 0x38060000, 0x38064000, 0x38068000, 0x3806c000,
    0x38070000, 0x38074000, 0x38078000, 0x3807c000, 0x38080000, 0x38084000,
    0x38088000, 0x3808c000, 0x38090000, 0x38094000, 0x38098000, 0x3809c000,
    0x380a0000, 0x380a4000, 0x380a8000, 0x380ac000, 0x380b0000, 0x380b4000,
    0x380b8000, 0x380bc000, 0x380c0000, 0x380c4000, 0x380c8000, 0x380cc000,
    0x380d0000, 0x380d4000, 0x380d8000, 0x380dc000, 0x380e0000, 0x380e4000,
    0x380e8000, 0x380ec000, 0x380f0000, 0x380f4000, 0x380f8000, 0x380fc000,
    0x38100000, 0x38104000, 0x38108000, 0x3810c000, 0x38110000, 0x38114000,
    0x38118000, 0x3811c000, 0x38120000, 0x38124000, 0x38128000, 0x3812c000,
    0x38130000, 0x38134000, 0x38138000, 0x3813c000, 0x38140000, 0x38144000,
    0x38148000, 0x3814c000, 0x38150000, 0x38154000, 0x38158000, 0x3815c000,
    0x38160000, 0x38164000, 0x38168000, 0x3816c000, 0x38170000, 0x38174000,
    0x38178000, 0x3817c000, 0x38180000, 0x38184000, 0x38188000, 0x3818c000,
    0x38190000, 0x38194000, 0x38198000, 0x3819c000, 0x381a0000, 0x381a4000,
    0x381a8000, 0x381ac000, 0x381b0000, 0x381b4000, 0x381b8000, 0x381bc000,
    0x381c0000, 0x381c4000, 0x381c8000, 0x381cc000, 0x381d0000, 0x381d4000,
    0x381d8000, 0x381dc000, 0x381e0000, 0x381e4000, 0x381e8000, 0x381ec000,
    0x381f0000, 0x381f4000, 0x381f8000, 0x381fc000, 0x38200000, 0x38204000,
    0x38208000, 0x3820c000, 0x38210000, 0x38214000, 0x38218000, 0x3821c000,
    0x38220000, 0x38224000, 0x38228000, 0x3822c000, 0x38230000, 0x38234000,
    0x38238000, 0x3823c000, 0x38240000, 0x38244000, 0x38248000, 0x3824c000,
    0x38250000, 0x38254000, 0x38258000, 0x3825c000, 0x38260000, 0x38264000,
    0x38268000, 0x3826c000, 0x38270000, 0x38274000, 0x38278000, 0x3827c000,
    0x38280000, 0x38284000, 0x38288000, 0x3828c000, 0x38290000, 0x38294000,
    0x38298000, 0x3829c000, 0x382a0000, 0x382a4000, 0x382a8000, 0x382ac000,
    0x382b0000, 0x382b4000, 0x382b8000, 0x382bc000, 0x382c0000, 0x382c4000,
    0x382c8000, 0x382cc000, 0x382d0000, 0x382d4000, 0x382d8000, 0x382dc000,
    0x382e0000, 0x382e4000, 0x382e8000, 0x382ec000, 0x382f0000, 0x382f4000,
    0x382f8000, 0x382fc000, 0x38300000, 0x38304000, 0x38308000, 0x3830c000,
    0x38310000, 0x38314000, 0x38318000, 0x3831c000, 0x38320000, 0x38324000,
    0x38328000, 0x3832c000, 0x38330000, 0x38334000, 0x38338000, 0x3833c000,
    0x38340000, 0x38344000, 0x38348000, 0x3834c000, 0x38350000, 0x38354000,
    0x38358000, 0x3835c000, 0x38360000, 0x38364000, 0x38368000, 0x3836c000,
    0x38370000, 0x38374000, 0x38378000, 0x3837c000, 0x38380000, 0x38384000,
    0x38388000, 0x3838c000, 0x38390000, 0x38394000, 0x38398000, 0x3839c000,
    0x383a0000, 0x383a4000, 0x383a8000, 0x383ac000, 0x383b0000, 0x383b4000,
    0x383b8000, 0x383bc000, 0x383c0000, 0x383c4000, 0x383c8000, 0x383cc000,
    0x383d0000, 0x383d4000, 0x383d8000, 0x383dc000, 0x383e0000, 0x383e4000,
    0x383e8000, 0x383ec000, 0x383f0000, 0x383f4000, 0x383f8000, 0x383fc000,
    0x38400000, 0x38404000, 0x38408000, 0x3840c000, 0x38410000, 0x38414000,
    0x38418000, 0x3841c000, 0x38420000, 0x38424000, 0x38428000, 0x3842c000,
    0x38430000, 0x38434000, 0x38438000, 0x3843c000, 0x38440000, 0x38444000,
    0x38448000, 0x3844c000, 0x38450000, 0x38454000, 0x38458000, 0x3845c000,
    0x38460000, 0x38464000, 0x38468000, 0x3846c000, 0x38470000, 0x38474000,
    0x38478000, 0x3847c000, 0x38480000, 0x38484000, 0x38488000, 0x3848c000,
    0x38490000, 0x38494000, 0x38498000, 0x3849c000, 0x384a0000, 0x384a4000,
    0x384a8000, 0x384ac000, 0x384b0000, 0x384b4000, 0x384b8000, 0x384bc000,
    0x384c0000, 0x384c4000, 0x384c8000, 0x384cc000, 0x384d0000, 0x384d4000,
    0x384d8000, 0x384dc000, 0x384e0000, 0x384e4000, 0x384e8000, 0x384ec000,
    0x384f0000, 0x384f4000, 0x384f8000, 0x384fc000, 0x38500000, 0x38504000,
    0x38508000, 0x3850c000, 0x38510000, 0x38514000, 0x38518000, 0x3851c000,
    0x38520000, 0x38524000, 0x38528000, 0x3852c000, 0x38530000, 0x38534000,
    0x38538000, 0x3853c000, 0x38540000, 0x38544000, 0x38548000, 0x3854c000,
    0x38550000, 0x38554000, 0x38558000, 0x3855c000, 0x38560000, 0x38564000,
    0x38568000, 0x3856c000, 0x38570000, 0x38574000, 0x38578000, 0x3857c000,
    0x38580000, 0x38584000, 0x38588000, 0x3858c000, 0x38590000, 0x38594000,
    0x38598000, 0x3859c000, 0x385a0000, 0x385a4000, 0x385a8000, 0x385ac000,
    0x385b0000, 0x385b4000, 0x385b8000, 0x385bc000, 0x385c0000, 0x385c4000,
    0x385c8000, 0x385cc000, 0x385d0000, 0x385d4000, 0x385d8000, 0x385dc000,
    0x385e0000, 0x385e4000, 0x385e8000, 0x385ec000, 0x385f0000, 0x385f4000,
    0x385f8000, 0x385fc000, 0x38600000, 0x38604000, 0x38608000, 0x3860c000,
    0x38610000, 0x38614000, 0x38618000, 0x3861c000, 0x38620000, 0x38624000,
    0x38628000, 0x3862c000, 0x38630000, 0x38634000, 0x38638000, 0x3863c000,
    0x38640000, 0x38644000, 0x38648000, 0x3864c000, 0x38650000, 0x38654000,
    0x38658000, 0x3865c000, 0x38660000, 0x38664000, 0x38668000, 0x3866c000,
    0x38670000, 0x38674000, 0x38678000, 0x3867c000, 0x38680000, 0x38684000,
    0x38688000, 0x3868c000, 0x38690000, 0x38694000, 0x38698000, 0x3869c000,
    0x386a0000, 0x386a4000, 0x386a8000, 0x386ac000, 0x386b0000, 0x386b4000,
    0x386b8000, 0x386bc000, 0x386c0000, 0x386c4000, 0x386c8000, 0x386cc000,
    0x386d0000, 0x386d4000, 0x386d8000, 0x386dc000, 0x386e0000, 0x386e4000,
    0x386e8000, 0x386ec000, 0x386f0000, 0x386f4000, 0x386f8000, 0x386fc000,
    0x38700000, 0x38704000, 0x38708000, 0x3870c000, 0x38710000, 0x38714000,
    0x38718000, 0x3871c000, 0x38720000, 0x38724000, 0x38728000, 0x3872c000,
    0x38730000, 0x38734000, 0x38738000, 0x3873c000, 0x38740000, 0x38744000,
    0x38748000, 0x3874c000, 0x38750000, 0x38754000, 0x38758000, 0x3875c000,
    0x38760000, 0x38764000, 0x38768000, 0x3876c000, 0x38770000, 0x38774000,
    0x38778000, 0x3877c000, 0x38780000, 0x38784000, 0x38788000, 0x3878c000,
    0x38790000, 0x38794000, 0x38798000, 0x3879c000, 0x387a0000, 0x387a4000,
    0x387a8000, 0x387ac000, 0x387b0000, 0x387b4000, 0x387b8000, 0x387bc000,
    0x387c0000, 0x387c4000, 0x387c8000, 0x387cc000, 0x387d0000, 0x387d4000,
    0x387d8000, 0x387dc000, 0x387e0000, 0x387e4000, 0x387e8000, 0x387ec000,
    0x387f0000, 0x387f4000, 0x387f8000, 0x387fc000, 0x38000000, 0x38002000,
    0x38004000, 0x38006000, 0x38008000, 0x3800a000, 0x3800c000, 0x3800e000,
    0x38010000, 0x38012000, 0x38014000, 0x38016000, 0x38018000, 0x3801a000,
    0x3801c000, 0x3801e000, 0x38020000, 0x38022000, 0x38024000, 0x38026000,
    0x38028000, 0x3802a000, 0x3802c000, 0x3802e000, 0x38030000, 0x38032000,
    0x38034000, 0x38036000, 0x38038000, 0x3803a000, 0x3803c000, 0x3803e000,
    0x38040000, 0x38042000, 0x38044000, 0x38046000, 0x38048000, 0x3804a000,
    0x3804c000, 0x3804e000, 0x38050000, 0x38052000, 0x38054000, 0x38056000,
    0x38058000, 0x3805a000, 0x3805c000, 0x3805e000, 0x38060000, 0x38062000,
    0x38064000, 0x38066000, 0x38068000, 0x3806a000, 0x3806c000, 0x3806e000,
    0x38070000, 0x38072000, 0x38074000, 0x38076000, 0x38078000, 0x3807a000,
    0x3807c000, 0x3807e000, 0x38080000, 0x38082000, 0x38084000, 0x38086000,
    0x38088000, 0x3808a000, 0x3808c000, 0x3808e000, 0x38090000, 0x38092000,
    0x38094000, 0x38096000, 0x38098000, 0x3809a000, 0x3809c000, 0x3809e000,
    0x380a0000, 0x380a2000, 0x380a4000, 0x380a6000, 0x380a8000, 0x380aa000,
    0x380ac000, 0x380ae000, 0x380b0000, 0x380b2000, 0x380b4000, 0x380b6000,
    0x380b8000, 0x380ba000, 0x380bc000, 0x380be000, 0x380c0000, 0x380c2000,
    0x380c4000, 0x380c6000, 0x380c8000, 0x380ca000, 0x380cc000, 0x380ce000,
    0x380d0000, 0x380d2000, 0x380d4000, 0x380d6000, 0x380d8000, 0x380da000,
    0x380dc000, 0x380de000, 0x380e0000, 0x380e2000, 0x380e4000, 0x380e6000,
    0x380e8000, 0x380ea000, 0x380ec000, 0x380ee000, 0x380f0000, 0x380f2000,
    0x380f4000, 0x380f6000, 0x380f8000, 0x380fa000, 0x380fc000, 0x380fe000,
    0x38100000, 0x38102000, 0x38104000, 0x38106000, 0x38108000, 0x3810a000,
    0x3810c000, 0x3810e000, 0x38110000, 0x38112000, 0x38114000, 0x38116000,
    0x38118000, 0x3811a000, 0x3811c000, 0x3811e000, 0x38120000, 0x38122000,
    0x38124000, 0x38126000, 0x38128000, 0x3812a000, 0x3812c000, 0x3812e000,
    0x38130000, 0x38132000, 0x38134000, 0x38136000, 0x38138000, 0x3813a000,
    0x3813c000, 0x3813e000, 0x38140000, 0x38142000, 0x38144000, 0x38146000,
    0x38148000, 0x3814a000, 0x3814c000, 0x3814e000, 0x38150000, 0x38152000,
    0x38154000, 0x38156000, 0x38158000, 0x3815a000, 0x3815c000, 0x3815e000,
    0x38160000, 0x38162000, 0x38164000, 0x38166000, 0x38168000, 0x3816a000,
    0x3816c000, 0x3816e000, 0x38170000, 0x38172000, 0x38174000, 0x38176000,
    0x38178000, 0x3817a000, 0x3817c000, 0x3817e000, 0x38180000, 0x38182000,
    0x38184000, 0x38186000, 0x38188000, 0x3818a000, 0x3818c000, 0x3818e000,
    0x38190000, 0x38192000, 0x38194000, 0x38196000, 0x38198000, 0x3819a000,
    0x3819c000, 0x3819e000, 0x381a0000, 0x381a2000, 0x381a4000, 0x381a6000,
    0x381a8000, 0x381aa000, 0x381ac000, 0x381ae000, 0x381b0000, 0x381b2000,
    0x381b4000, 0x381b6000, 0x381b8000, 0x381ba000, 0x381bc000, 0x381be000,
    0x381c0000, 0x381c2000, 0x381c4000, 0x381c6000, 0x381c8000, 0x381ca000,
    0x381cc000, 0x381ce000, 0x381d0000, 0x381d2000, 0x381d4000, 0x381d6000,
    0x381d8000, 0x381da000, 0x381dc000, 0x381de000, 0x381e0000, 0x381e2000,
    0x381e4000, 0x381e6000, 0x381e8000, 0x381ea000, 0x381ec000, 0x381ee000,
    0x381f0000, 0x381f2000, 0x381f4000, 0x381f6000, 0x381f8000, 0x381fa000,
    0x381fc000, 0x381fe000, 0x38200000, 0x38202000, 0x38204000, 0x38206000,
    0x38208000, 0x3820a000, 0x3820c000, 0x3820e000, 0x38210000, 0x38212000,
    0x38214000, 0x38216000, 0x38218000, 0x3821a000, 0x3821c000, 0x3821e000,
    0x38220000, 0x38222000, 0x38224000, 0x38226000, 0x38228000, 0x3822a000,
    0x3822c000, 0x3822e000, 0x38230000, 0x38232000, 0x38234000, 0x38236000,
    0x38238000, 0x3823a000, 0x3823c000, 0x3823e000, 0x38240000, 0x38242000,
    0x38244000, 0x38246000, 0x38248000, 0x3824a000, 0x3824c000, 0x3824e000,
    0x38250000, 0x38252000, 0x38254000, 0x38256000, 0x38258000, 0x3825a000,
    0x3825c000, 0x3825e000, 0x38260000, 0x38262000, 0x38264000, 0x38266000,
    0x38268000, 0x3826a000, 0x3826c000, 0x3826e000, 0x38270000, 0x38272000,
    0x38274000, 0x38276000, 0x38278000, 0x3827a000, 0x3827c000, 0x3827e000,
    0x38280000, 0x38282000, 0x38284000, 0x38286000, 0x38288000, 0x3828a000,
    0x3828c000, 0x3828e000, 0x38290000, 0x38292000, 0x38294000, 0x38296000,
    0x38298000, 0x3829a000, 0x3829c000, 0x3829e000, 0x382a0000, 0x382a2000,
    0x382a4000, 0x382a6000, 0x382a8000, 0x382aa000, 0x382ac000, 0x382ae000,
    0x382b0000, 0x382b2000, 0x382b4000, 0x382b6000, 0x382b8000, 0x382ba000,
    0x382bc000, 0x382be000, 0x382c0000, 0x382c2000, 0x382c4000, 0x382c6000,
    0x382c8000, 0x382ca000, 0x382cc000, 0x382ce000, 0x382d0000, 0x382d2000,
    0x382d4000, 0x382d6000, 0x382d8000, 0x382da000, 0x382dc000, 0x382de000,
    0x382e0000, 0x382e2000, 0x382e4000, 0x382e6000, 0x382e8000, 0x382ea000,
    0x382ec000, 0x382ee000, 0x382f0000, 0x382f2000, 0x382f4000, 0x382f6000,
    0x382f8000, 0x382fa000, 0x382fc000, 0x382fe000, 0x38300000, 0x38302000,
    0x38304000, 0x38306000, 0x38308000, 0x3830a000, 0x3830c000, 0x3830e000,
    0x38310000, 0x38312000, 0x38314000, 0x38316000, 0x38318000, 0x3831a000,
    0x3831c000, 0x3831e000, 0x38320000, 0x38322000, 0x38324000, 0x38326000,
    0x38328000, 0x3832a000, 0x3832c000, 0x3832e000, 0x38330000, 0x38332000,
    0x38334000, 0x38336000, 0x38338000, 0x3833a000, 0x3833c000, 0x3833e000,
    0x38340000, 0x38342000, 0x38344000, 0x38346000, 0x38348000, 0x3834a000,
    0x3834c000, 0x3834e000, 0x38350000, 0x38352000, 0x38354000, 0x38356000,
    0x38358000, 0x3835a000, 0x3835c000, 0x3835e000, 0x38360000, 0x38362000,
    0x38364000, 0x38366000, 0x38368000, 0x3836a000, 0x3836c000, 0x3836e000,
    0x38370000, 0x38372000, 0x38374000, 0x38376000, 0x38378000, 0x3837a000,
    0x3837c000, 0x3837e000, 0x38380000, 0x38382000, 0x38384000, 0x38386000,
    0x38388000, 0x3838a000, 0x3838c000, 0x3838e000, 0x38390000, 0x38392000,
    0x38394000, 0x38396000, 0x38398000, 0x3839a000, 0x3839c000, 0x3839e000,
    0x383a0000, 0x383a2000, 0x383a4000, 0x383a6000, 0x383a8000, 0x383aa000,
    0x383ac000, 0x383ae000, 0x383b0000, 0x383b2000, 0x383b4000, 0x383b6000,
    0x383b8000, 0x383ba000, 0x383bc000, 0x383be000, 0x383c0000, 0x383c2000,
    0x383c4000, 0x383c6000, 0x383c8000, 0x383ca000, 0x383cc000, 0x383ce000,
    0x383d0000, 0x383d2000, 0x383d4000, 0x383d6000, 0x383d8000, 0x383da000,
    0x383dc000, 0x383de000, 0x383e0000, 0x383e2000, 0x383e4000, 0x383e6000,
    0x383e8000, 0x383ea000, 0x383ec000, 0x383ee000, 0x383f0000, 0x383f2000,
    0x383f4000, 0x383f6000, 0x383f8000, 0x383fa000, 0x383fc000, 0x383fe000,
    0x38400000, 0x38402000, 0x38404000, 0x38406000, 0x38408000, 0x3840a000,
    0x3840c000, 0x3840e000, 0x38410000, 0x38412000, 0x38414000, 0x38416000,
    0x38418000, 0x3841a000, 0x3841c000, 0x3841e000, 0x38420000, 0x38422000,
    0x38424000, 0x38426000, 0x38428000, 0x3842a000, 0x3842c000, 0x3842e000,
    0x38430000, 0x38432000, 0x38434000, 0x38436000, 0x38438000, 0x3843a000,
    0x3843c000, 0x3843e000, 0x38440000, 0x38442000, 0x38444000, 0x38446000,
    0x38448000, 0x3844a000, 0x3844c000, 0x3844e000, 0x38450000, 0x38452000,
    0x38454000, 0x38456000, 0x38458000, 0x3845a000, 0x3845c000, 0x3845e000,
    0x38460000, 0x38462000, 0x38464000, 0x38466000, 0x38468000, 0x3846a000,
    0x3846c000, 0x3846e000, 0x38470000, 0x38472000, 0x38474000, 0x38476000,
    0x38478000, 0x3847a000, 0x3847c000, 0x3847e000, 0x38480000, 0x38482000,
    0x38484000, 0x38486000, 0x38488000, 0x3848a000, 0x3848c000, 0x3848e000,
    0x38490000, 0x38492000, 0x38494000, 0x38496000, 0x38498000, 0x3849a000,
    0x3849c000, 0x3849e000, 0x384a0000, 0x384a2000, 0x384a4000, 0x384a6000,
    0x384a8000, 0x384aa000, 0x384ac000, 0x384ae000, 0x384b0000, 0x384b2000,
    0x384b4000, 0x384b6000, 0x384b8000, 0x384ba000, 0x384bc000, 0x384be000,
    0x384c0000, 0x384c2000, 0x384c4000, 0x384c6000, 0x384c8000, 0x384ca000,
    0x384cc000, 0x384ce000, 0x384d0000, 0x384d2000, 0x384d4000, 0x384d6000,
    0x384d8000, 0x384da000, 0x384dc000, 0x384de000, 0x384e0000, 0x384e2000,
    0x384e4000, 0x384e6000, 0x384e8000, 0x384ea000, 0x384ec000, 0x384ee000,
    0x384f0000, 0x384f2000, 0x384f4000, 0x384f6000, 0x384f8000, 0x384fa000,
    0x384fc000, 0x384fe000, 0x38500000, 0x38502000, 0x38504000, 0x38506000,
    0x38508000, 0x3850a000, 0x3850c000, 0x3850e000, 0x38510000, 0x38512000,
    0x38514000, 0x38516000, 0x38518000, 0x3851a000, 0x3851c000, 0x3851e000,
    0x38520000, 0x38522000, 0x38524000, 0x38526000, 0x38528000, 0x3852a000,
    0x3852c000, 0x3852e000, 0x38530000, 0x38532000, 0x38534000, 0x38536000,
    0x38538000, 0x3853a000, 0x3853c000, 0x3853e000, 0x38540000, 0x38542000,
    0x38544000, 0x38546000, 0x38548000, 0x3854a000, 0x3854c000, 0x3854e000,
    0x38550000, 0x38552000, 0x38554000, 0x38556000, 0x38558000, 0x3855a000,
    0x3855c000, 0x3855e000, 0x38560000, 0x38562000, 0x38564000, 0x38566000,
    0x38568000, 0x3856a000, 0x3856c000, 0x3856e000, 0x38570000, 0x38572000,
    0x38574000, 0x38576000, 0x38578000, 0x3857a000, 0x3857c000, 0x3857e000,
    0x38580000, 0x38582000, 0x38584000, 0x38586000, 0x38588000, 0x3858a000,
    0x3858c000, 0x3858e000, 0x38590000, 0x38592000, 0x38594000, 0x38596000,
    0x38598000, 0x3859a000, 0x3859c000, 0x3859e000, 0x385a0000, 0x385a2000,
    0x385a4000, 0x385a6000, 0x385a8000, 0x385aa000, 0x385ac000, 0x385ae000,
    0x385b0000, 0x385b2000, 0x385b4000, 0x385b6000, 0x385b8000, 0x385ba000,
    0x385bc000, 0x385be000, 0x385c0000, 0x385c2000, 0x385c4000, 0x385c6000,
    0x385c8000, 0x385ca000, 0x385cc000, 0x385ce000, 0x385d0000, 0x385d2000,
    0x385d4000, 0x385d6000, 0x385d8000, 0x385da000, 0x385dc000, 0x385de000,
    0x385e0000, 0x385e2000, 0x385e4000, 0x385e6000, 0x385e8000, 0x385ea000,
    0x385ec000, 0x385ee000, 0x385f0000, 0x385f2000, 0x385f4000, 0x385f6000,
    0x385f8000, 0x385fa000, 0x385fc000, 0x385fe000, 0x38600000, 0x38602000,
    0x38604000, 0x38606000, 0x38608000, 0x3860a000, 0x3860c000, 0x3860e000,
    0x38610000, 0x38612000, 0x38614000, 0x38616000, 0x38618000, 0x3861a000,
    0x3861c000, 0x3861e000, 0x38620000, 0x38622000, 0x38624000, 0x38626000,
    0x38628000, 0x3862a000, 0x3862c000, 0x3862e000, 0x38630000, 0x38632000,
    0x38634000, 0x38636000, 0x38638000, 0x3863a000, 0x3863c000, 0x3863e000,
    0x38640000, 0x38642000, 0x38644000, 0x38646000, 0x38648000, 0x3864a000,
    0x3864c000, 0x3864e000, 0x38650000, 0x38652000, 0x38654000, 0x38656000,
    0x38658000, 0x3865a000, 0x3865c000, 0x3865e000, 0x38660000, 0x38662000,
    0x38664000, 0x38666000, 0x38668000, 0x3866a000, 0x3866c000, 0x3866e000,
    0x38670000, 0x38672000, 0x38674000, 0x38676000, 0x38678000, 0x3867a000,
    0x3867c000, 0x3867e000, 0x38680000, 0x38682000, 0x38684000, 0x38686000,
    0x38688000, 0x3868a000, 0x3868c000, 0x3868e000, 0x38690000, 0x38692000,
    0x38694000, 0x38696000, 0x38698000, 0x3869a000, 0x3869c000, 0x3869e000,
    0x386a0000, 0x386a2000, 0x386a4000, 0x386a6000, 0x386a8000, 0x386aa000,
    0x386ac000, 0x386ae000, 0x386b0000, 0x386b2000, 0x386b4000, 0x386b6000,
    0x386b8000, 0x386ba000, 0x386bc000, 0x386be000, 0x386c0000, 0x386c2000,
    0x386c4000, 0x386c6000, 0x386c8000, 0x386ca000, 0x386cc000, 0x386ce000,
    0x386d0000, 0x386d2000, 0x386d4000, 0x386d6000, 0x386d8000, 0x386da000,
    0x386dc000, 0x386de000, 0x386e0000, 0x386e2000, 0x386e4000, 0x386e6000,
    0x386e8000, 0x386ea000, 0x386ec000, 0x386ee000, 0x386f0000, 0x386f2000,
    0x386f4000, 0x386f6000, 0x386f8000, 0x386fa000, 0x386fc000, 0x386fe000,
    0x38700000, 0x38702000, 0x38704000, 0x38706000, 0x38708000, 0x3870a000,
    0x3870c000, 0x3870e000, 0x38710000, 0x38712000, 0x38714000, 0x38716000,
    0x38718000, 0x3871a000, 0x3871c000, 0x3871e000, 0x38720000, 0x38722000,
    0x38724000, 0x38726000, 0x38728000, 0x3872a000, 0x3872c000, 0x3872e000,
    0x38730000, 0x38732000, 0x38734000, 0x38736000, 0x38738000, 0x3873a000,
    0x3873c000, 0x3873e000, 0x38740000, 0x38742000, 0x38744000, 0x38746000,
    0x38748000, 0x3874a000, 0x3874c000, 0x3874e000, 0x38750000, 0x38752000,
    0x38754000, 0x38756000, 0x38758000, 0x3875a000, 0x3875c000, 0x3875e000,
    0x38760000, 0x38762000, 0x38764000, 0x38766000, 0x38768000, 0x3876a000,
    0x3876c000, 0x3876e000, 0x38770000, 0x38772000, 0x38774000, 0x38776000,
    0x38778000, 0x3877a000, 0x3877c000, 0x3877e000, 0x38780000, 0x38782000,
    0x38784000, 0x38786000, 0x38788000, 0x3878a000, 0x3878c000, 0x3878e000,
    0x38790000, 0x38792000, 0x38794000, 0x38796000, 0x38798000, 0x3879a000,
    0x3879c000, 0x3879e000, 0x387a0000, 0x387a2000, 0x387a4000, 0x387a6000,
    0x387a8000, 0x387aa000, 0x387ac000, 0x387ae000, 0x387b0000, 0x387b2000,
    0x387b4000, 0x387b6000, 0x387b8000, 0x387ba000, 0x387bc000, 0x387be000,
    0x387c0000, 0x387c2000, 0x387c4000, 0x387c6000, 0x387c8000, 0x387ca000,
    0x387cc000, 0x387ce000, 0x387d0000, 0x387d2000, 0x387d4000, 0x387d6000,
    0x387d8000, 0x387da000, 0x387dc000, 0x387de000, 0x387e0000, 0x387e2000,
    0x387e4000, 0x387e6000, 0x387e8000, 0x387ea000, 0x387ec000, 0x387ee000,
    0x387f0000, 0x387f2000, 0x387f4000, 0x387f6000, 0x387f8000, 0x387fa000,
    0x387fc000, 0x387fe000};

const uint16_t g_offset_table[64] = {
    0,    1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024,
    1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024,
    1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 0,
    1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024,
    1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024,
    1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024};

const uint32_t g_exponent_table[64] = {
    0x0,        0x800000,   0x1000000,  0x1800000,  0x2000000,  0x2800000,
    0x3000000,  0x3800000,  0x4000000,  0x4800000,  0x5000000,  0x5800000,
    0x6000000,  0x6800000,  0x7000000,  0x7800000,  0x8000000,  0x8800000,
    0x9000000,  0x9800000,  0xa000000,  0xa800000,  0xb000000,  0xb800000,
    0xc000000,  0xc800000,  0xd000000,  0xd800000,  0xe000000,  0xe800000,
    0xf000000,  0x47800000, 0x80000000, 0x80800000, 0x81000000, 0x81800000,
    0x82000000, 0x82800000, 0x83000000, 0x83800000, 0x84000000, 0x84800000,
    0x85000000, 0x85800000, 0x86000000, 0x86800000, 0x87000000, 0x87800000,
    0x88000000, 0x88800000, 0x89000000, 0x89800000, 0x8a000000, 0x8a800000,
    0x8b000000, 0x8b800000, 0x8c000000, 0x8c800000, 0x8d000000, 0x8d800000,
    0x8e000000, 0x8e800000, 0x8f000000, 0xc7800000};

float ConvertHalfFloatToFloat(uint16_t half) {
  uint32_t temp =
      g_mantissa_table[g_offset_table[half >> 10] + (half & 0x3ff)] +
      g_exponent_table[half >> 10];
  return *(reinterpret_cast<float*>(&temp));
}

/* BEGIN CODE SHARED WITH MOZILLA FIREFOX */

// The following packing and unpacking routines are expressed in terms of
// function templates and inline functions to achieve generality and speedup.
// Explicit template specializations correspond to the cases that would occur.
// Some code are merged back from Mozilla code in
// http://mxr.mozilla.org/mozilla-central/source/content/canvas/src/WebGLTexelConversions.h

//----------------------------------------------------------------------
// Pixel unpacking routines.
template <int format, typename SourceType, typename DstType>
void Unpack(const SourceType*, DstType*, unsigned) {
  NOTREACHED();
}

template <>
void Unpack<WebGLImageConversion::kDataFormatARGB8, uint8_t, uint8_t>(
    const uint8_t* source,
    uint8_t* destination,
    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = source[1];
    destination[1] = source[2];
    destination[2] = source[3];
    destination[3] = source[0];
    source += 4;
    destination += 4;
  }
}

template <>
void Unpack<WebGLImageConversion::kDataFormatABGR8, uint8_t, uint8_t>(
    const uint8_t* source,
    uint8_t* destination,
    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = source[3];
    destination[1] = source[2];
    destination[2] = source[1];
    destination[3] = source[0];
    source += 4;
    destination += 4;
  }
}

template <>
void Unpack<WebGLImageConversion::kDataFormatBGRA8, uint8_t, uint8_t>(
    const uint8_t* source,
    uint8_t* destination,
    unsigned pixels_per_row) {
  const uint32_t* source32 = reinterpret_cast_ptr<const uint32_t*>(source);
  uint32_t* destination32 = reinterpret_cast_ptr<uint32_t*>(destination);

#if defined(ARCH_CPU_X86_FAMILY)
  simd::UnpackOneRowOfBGRA8LittleToRGBA8(source32, destination32,
                                         pixels_per_row);
#endif
#if defined(HAVE_MIPS_MSA_INTRINSICS)
  simd::unpackOneRowOfBGRA8LittleToRGBA8MSA(source32, destination32,
                                            pixels_per_row);
#endif
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    uint32_t bgra = source32[i];
#if defined(ARCH_CPU_BIG_ENDIAN)
    uint32_t brMask = 0xff00ff00;
    uint32_t gaMask = 0x00ff00ff;
#else
    uint32_t br_mask = 0x00ff00ff;
    uint32_t ga_mask = 0xff00ff00;
#endif
    uint32_t rgba =
        (((bgra >> 16) | (bgra << 16)) & br_mask) | (bgra & ga_mask);
    destination32[i] = rgba;
  }
}

template <>
void Unpack<WebGLImageConversion::kDataFormatRGBA5551, uint16_t, uint8_t>(
    const uint16_t* source,
    uint8_t* destination,
    unsigned pixels_per_row) {
#if defined(ARCH_CPU_X86_FAMILY)
  simd::UnpackOneRowOfRGBA5551LittleToRGBA8(source, destination,
                                            pixels_per_row);
#endif
#if defined(CPU_ARM_NEON)
  simd::UnpackOneRowOfRGBA5551ToRGBA8(source, destination, pixels_per_row);
#endif
#if defined(HAVE_MIPS_MSA_INTRINSICS)
  simd::unpackOneRowOfRGBA5551ToRGBA8MSA(source, destination, pixels_per_row);
#endif

  for (unsigned i = 0; i < pixels_per_row; ++i) {
    uint16_t packed_value = source[0];
    uint8_t r = packed_value >> 11;
    uint8_t g = (packed_value >> 6) & 0x1F;
    uint8_t b = (packed_value >> 1) & 0x1F;
    destination[0] = (r << 3) | (r & 0x7);
    destination[1] = (g << 3) | (g & 0x7);
    destination[2] = (b << 3) | (b & 0x7);
    destination[3] = (packed_value & 0x1) ? 0xFF : 0x0;
    source += 1;
    destination += 4;
  }
}

template <>
void Unpack<WebGLImageConversion::kDataFormatRGBA4444, uint16_t, uint8_t>(
    const uint16_t* source,
    uint8_t* destination,
    unsigned pixels_per_row) {
#if defined(ARCH_CPU_X86_FAMILY)
  simd::UnpackOneRowOfRGBA4444LittleToRGBA8(source, destination,
                                            pixels_per_row);
#endif
#if defined(CPU_ARM_NEON)
  simd::UnpackOneRowOfRGBA4444ToRGBA8(source, destination, pixels_per_row);
#endif
#if defined(HAVE_MIPS_MSA_INTRINSICS)
  simd::unpackOneRowOfRGBA4444ToRGBA8MSA(source, destination, pixels_per_row);
#endif
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    uint16_t packed_value = source[0];
    uint8_t r = packed_value >> 12;
    uint8_t g = (packed_value >> 8) & 0x0F;
    uint8_t b = (packed_value >> 4) & 0x0F;
    uint8_t a = packed_value & 0x0F;
    destination[0] = r << 4 | r;
    destination[1] = g << 4 | g;
    destination[2] = b << 4 | b;
    destination[3] = a << 4 | a;
    source += 1;
    destination += 4;
  }
}

template <>
void Unpack<WebGLImageConversion::kDataFormatRA8, uint8_t, uint8_t>(
    const uint8_t* source,
    uint8_t* destination,
    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = source[0];
    destination[1] = source[0];
    destination[2] = source[0];
    destination[3] = source[1];
    source += 2;
    destination += 4;
  }
}

template <>
void Unpack<WebGLImageConversion::kDataFormatAR8, uint8_t, uint8_t>(
    const uint8_t* source,
    uint8_t* destination,
    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = source[1];
    destination[1] = source[1];
    destination[2] = source[1];
    destination[3] = source[0];
    source += 2;
    destination += 4;
  }
}

template <>
void Unpack<WebGLImageConversion::kDataFormatRGBA8, uint8_t, float>(
    const uint8_t* source,
    float* destination,
    unsigned pixels_per_row) {
  const float kScaleFactor = 1.0f / 255.0f;
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = source[0] * kScaleFactor;
    destination[1] = source[1] * kScaleFactor;
    destination[2] = source[2] * kScaleFactor;
    destination[3] = source[3] * kScaleFactor;
    source += 4;
    destination += 4;
  }
}

template <>
void Unpack<WebGLImageConversion::kDataFormatBGRA8, uint8_t, float>(
    const uint8_t* source,
    float* destination,
    unsigned pixels_per_row) {
  const float kScaleFactor = 1.0f / 255.0f;
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = source[2] * kScaleFactor;
    destination[1] = source[1] * kScaleFactor;
    destination[2] = source[0] * kScaleFactor;
    destination[3] = source[3] * kScaleFactor;
    source += 4;
    destination += 4;
  }
}

template <>
void Unpack<WebGLImageConversion::kDataFormatABGR8, uint8_t, float>(
    const uint8_t* source,
    float* destination,
    unsigned pixels_per_row) {
  const float kScaleFactor = 1.0f / 255.0f;
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = source[3] * kScaleFactor;
    destination[1] = source[2] * kScaleFactor;
    destination[2] = source[1] * kScaleFactor;
    destination[3] = source[0] * kScaleFactor;
    source += 4;
    destination += 4;
  }
}

template <>
void Unpack<WebGLImageConversion::kDataFormatARGB8, uint8_t, float>(
    const uint8_t* source,
    float* destination,
    unsigned pixels_per_row) {
  const float kScaleFactor = 1.0f / 255.0f;
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = source[1] * kScaleFactor;
    destination[1] = source[2] * kScaleFactor;
    destination[2] = source[3] * kScaleFactor;
    destination[3] = source[0] * kScaleFactor;
    source += 4;
    destination += 4;
  }
}

template <>
void Unpack<WebGLImageConversion::kDataFormatRA32F, float, float>(
    const float* source,
    float* destination,
    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = source[0];
    destination[1] = source[0];
    destination[2] = source[0];
    destination[3] = source[1];
    source += 2;
    destination += 4;
  }
}

template <>
void Unpack<WebGLImageConversion::kDataFormatRGBA2_10_10_10, uint32_t, float>(
    const uint32_t* source,
    float* destination,
    unsigned pixels_per_row) {
  static const float kRgbScaleFactor = 1.0f / 1023.0f;
  static const float kAlphaScaleFactor = 1.0f / 3.0f;
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    uint32_t packed_value = source[0];
    destination[0] = static_cast<float>(packed_value & 0x3FF) * kRgbScaleFactor;
    destination[1] =
        static_cast<float>((packed_value >> 10) & 0x3FF) * kRgbScaleFactor;
    destination[2] =
        static_cast<float>((packed_value >> 20) & 0x3FF) * kRgbScaleFactor;
    destination[3] = static_cast<float>(packed_value >> 30) * kAlphaScaleFactor;
    source += 1;
    destination += 4;
  }
}

// Used for non-trivial conversions of RGBA16F data.
template <>
void Unpack<WebGLImageConversion::kDataFormatRGBA16F, uint16_t, float>(
    const uint16_t* source,
    float* destination,
    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = ConvertHalfFloatToFloat(source[0]);
    destination[1] = ConvertHalfFloatToFloat(source[1]);
    destination[2] = ConvertHalfFloatToFloat(source[2]);
    destination[3] = ConvertHalfFloatToFloat(source[3]);
    source += 4;
    destination += 4;
  }
}

// Used for the trivial conversion of RGBA16F data to RGBA8.
template <>
void Unpack<WebGLImageConversion::kDataFormatRGBA16F, uint16_t, uint8_t>(
    const uint16_t* source,
    uint8_t* destination,
    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] =
        ClampAndScaleFloat<uint8_t>(ConvertHalfFloatToFloat(source[0]));
    destination[1] =
        ClampAndScaleFloat<uint8_t>(ConvertHalfFloatToFloat(source[1]));
    destination[2] =
        ClampAndScaleFloat<uint8_t>(ConvertHalfFloatToFloat(source[2]));
    destination[3] =
        ClampAndScaleFloat<uint8_t>(ConvertHalfFloatToFloat(source[3]));
    source += 4;
    destination += 4;
  }
}

//----------------------------------------------------------------------
// Pixel packing routines.
//

// All of the formats below refer to the format of the texture being
// uploaded. Only the formats that accept DOM sources (images, videos,
// ImageBitmap, etc.) need to:
//
//  (a) support conversions from "other" formats than the destination
//      format, since the other cases are simply handling Y-flips or alpha
//      premultiplication of data supplied via ArrayBufferView
//
//  (b) support the kAlphaDoUnmultiply operation, which is needed because
//      there are some DOM-related data sources (like 2D canvas) which are
//      stored in premultiplied form. Note that the alpha-only formats
//      inherently don't need to support the kAlphaDoUnmultiply operation.
//
// The formats that accept DOM-related inputs are in the table for
// texImage2D taking TexImageSource in the WebGL 2.0 specification, plus
// all of the formats in the WebGL 1.0 specification, including legacy
// formats like luminance, alpha and luminance-alpha formats (which are
// renamed in the DataFormat enum to things like "red-alpha"). Extensions
// like EXT_texture_norm16 add to the supported formats
//
// Currently, those texture formats to which DOM-related inputs can be
// uploaded have to support two basic input formats coming from the rest of
// the browser: uint8_t, for RGBA8, and float, for RGBA16F.

template <int format, int alphaOp, typename SourceType, typename DstType>
void Pack(const SourceType*, DstType*, unsigned) {
  NOTREACHED();
}

template <>
void Pack<WebGLImageConversion::kDataFormatA8,
          WebGLImageConversion::kAlphaDoNothing,
          uint8_t,
          uint8_t>(const uint8_t* source,
                   uint8_t* destination,
                   unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = source[3];
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatA8,
          WebGLImageConversion::kAlphaDoNothing,
          float,
          uint8_t>(const float* source,
                   uint8_t* destination,
                   unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = ClampAndScaleFloat<uint8_t>(source[3]);
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatR8,
          WebGLImageConversion::kAlphaDoNothing,
          uint8_t,
          uint8_t>(const uint8_t* source,
                   uint8_t* destination,
                   unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = source[0];
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatR8,
          WebGLImageConversion::kAlphaDoNothing,
          float,
          uint8_t>(const float* source,
                   uint8_t* destination,
                   unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = ClampAndScaleFloat<uint8_t>(source[0]);
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatR8,
          WebGLImageConversion::kAlphaDoPremultiply,
          uint8_t,
          uint8_t>(const uint8_t* source,
                   uint8_t* destination,
                   unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] / 255.0f;
    uint8_t source_r =
        static_cast<uint8_t>(static_cast<float>(source[0]) * scale_factor);
    destination[0] = source_r;
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatR8,
          WebGLImageConversion::kAlphaDoPremultiply,
          float,
          uint8_t>(const float* source,
                   uint8_t* destination,
                   unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    uint8_t source_r = ClampAndScaleFloat<uint8_t>(source[0] * source[3]);
    destination[0] = source_r;
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatR8,
          WebGLImageConversion::kAlphaDoUnmultiply,
          uint8_t,
          uint8_t>(const uint8_t* source,
                   uint8_t* destination,
                   unsigned pixels_per_row) {
#if defined(ARCH_CPU_X86_FAMILY)
  simd::PackOneRowOfRGBA8LittleToR8(source, destination, pixels_per_row);
#endif
#if defined(HAVE_MIPS_MSA_INTRINSICS)
  simd::packOneRowOfRGBA8LittleToR8MSA(source, destination, pixels_per_row);
#endif
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] ? 255.0f / source[3] : 1.0f;
    uint8_t source_r =
        static_cast<uint8_t>(static_cast<float>(source[0]) * scale_factor);
    destination[0] = source_r;
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatR8,
          WebGLImageConversion::kAlphaDoUnmultiply,
          float,
          uint8_t>(const float* source,
                   uint8_t* destination,
                   unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] ? 1.0f / source[3] : 1.0f;
    uint8_t source_r = ClampAndScaleFloat<uint8_t>(source[0] * scale_factor);
    destination[0] = source_r;
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRA8,
          WebGLImageConversion::kAlphaDoNothing,
          uint8_t,
          uint8_t>(const uint8_t* source,
                   uint8_t* destination,
                   unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = source[0];
    destination[1] = source[3];
    source += 4;
    destination += 2;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRA8,
          WebGLImageConversion::kAlphaDoNothing,
          float,
          uint8_t>(const float* source,
                   uint8_t* destination,
                   unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = ClampAndScaleFloat<uint8_t>(source[0]);
    destination[1] = ClampAndScaleFloat<uint8_t>(source[3]);
    source += 4;
    destination += 2;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRA8,
          WebGLImageConversion::kAlphaDoPremultiply,
          uint8_t,
          uint8_t>(const uint8_t* source,
                   uint8_t* destination,
                   unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] / 255.0f;
    uint8_t source_r =
        static_cast<uint8_t>(static_cast<float>(source[0]) * scale_factor);
    destination[0] = source_r;
    destination[1] = source[3];
    source += 4;
    destination += 2;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRA8,
          WebGLImageConversion::kAlphaDoPremultiply,
          float,
          uint8_t>(const float* source,
                   uint8_t* destination,
                   unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = ClampAndScaleFloat<uint8_t>(source[0] * source[3]);
    destination[1] = ClampAndScaleFloat<uint8_t>(source[3]);
    source += 4;
    destination += 2;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRA8,
          WebGLImageConversion::kAlphaDoUnmultiply,
          uint8_t,
          uint8_t>(const uint8_t* source,
                   uint8_t* destination,
                   unsigned pixels_per_row) {
#if defined(ARCH_CPU_X86_FAMILY)
  simd::PackOneRowOfRGBA8LittleToRA8(source, destination, pixels_per_row);
#endif
#if defined(HAVE_MIPS_MSA_INTRINSICS)
  simd::packOneRowOfRGBA8LittleToRA8MSA(source, destination, pixels_per_row);
#endif
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] ? 255.0f / source[3] : 1.0f;
    uint8_t source_r =
        static_cast<uint8_t>(static_cast<float>(source[0]) * scale_factor);
    destination[0] = source_r;
    destination[1] = source[3];
    source += 4;
    destination += 2;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRA8,
          WebGLImageConversion::kAlphaDoUnmultiply,
          float,
          uint8_t>(const float* source,
                   uint8_t* destination,
                   unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] ? 1.0f / source[3] : 1.0f;
    uint8_t source_r = ClampAndScaleFloat<uint8_t>(source[0] * scale_factor);
    destination[0] = source_r;
    destination[1] = ClampAndScaleFloat<uint8_t>(source[3]);
    source += 4;
    destination += 2;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGB8,
          WebGLImageConversion::kAlphaDoNothing,
          uint8_t,
          uint8_t>(const uint8_t* source,
                   uint8_t* destination,
                   unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = source[0];
    destination[1] = source[1];
    destination[2] = source[2];
    source += 4;
    destination += 3;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGB8,
          WebGLImageConversion::kAlphaDoNothing,
          float,
          uint8_t>(const float* source,
                   uint8_t* destination,
                   unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = ClampAndScaleFloat<uint8_t>(source[0]);
    destination[1] = ClampAndScaleFloat<uint8_t>(source[1]);
    destination[2] = ClampAndScaleFloat<uint8_t>(source[2]);
    source += 4;
    destination += 3;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGB8,
          WebGLImageConversion::kAlphaDoPremultiply,
          uint8_t,
          uint8_t>(const uint8_t* source,
                   uint8_t* destination,
                   unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] / 255.0f;
    uint8_t source_r =
        static_cast<uint8_t>(static_cast<float>(source[0]) * scale_factor);
    uint8_t source_g =
        static_cast<uint8_t>(static_cast<float>(source[1]) * scale_factor);
    uint8_t source_b =
        static_cast<uint8_t>(static_cast<float>(source[2]) * scale_factor);
    destination[0] = source_r;
    destination[1] = source_g;
    destination[2] = source_b;
    source += 4;
    destination += 3;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGB8,
          WebGLImageConversion::kAlphaDoPremultiply,
          float,
          uint8_t>(const float* source,
                   uint8_t* destination,
                   unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = ClampAndScaleFloat<uint8_t>(source[0] * source[3]);
    destination[1] = ClampAndScaleFloat<uint8_t>(source[1] * source[3]);
    destination[2] = ClampAndScaleFloat<uint8_t>(source[2] * source[3]);
    source += 4;
    destination += 3;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGB8,
          WebGLImageConversion::kAlphaDoUnmultiply,
          uint8_t,
          uint8_t>(const uint8_t* source,
                   uint8_t* destination,
                   unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] ? 255.0f / source[3] : 1.0f;
    uint8_t source_r =
        static_cast<uint8_t>(static_cast<float>(source[0]) * scale_factor);
    uint8_t source_g =
        static_cast<uint8_t>(static_cast<float>(source[1]) * scale_factor);
    uint8_t source_b =
        static_cast<uint8_t>(static_cast<float>(source[2]) * scale_factor);
    destination[0] = source_r;
    destination[1] = source_g;
    destination[2] = source_b;
    source += 4;
    destination += 3;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGB8,
          WebGLImageConversion::kAlphaDoUnmultiply,
          float,
          uint8_t>(const float* source,
                   uint8_t* destination,
                   unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] ? 1.0f / source[3] : 1.0f;
    destination[0] = ClampAndScaleFloat<uint8_t>(source[0] * scale_factor);
    destination[1] = ClampAndScaleFloat<uint8_t>(source[1] * scale_factor);
    destination[2] = ClampAndScaleFloat<uint8_t>(source[2] * scale_factor);
    source += 4;
    destination += 3;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA8,
          WebGLImageConversion::kAlphaDoPremultiply,
          uint8_t,
          uint8_t>(const uint8_t* source,
                   uint8_t* destination,
                   unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] / 255.0f;
    uint8_t source_r =
        static_cast<uint8_t>(static_cast<float>(source[0]) * scale_factor);
    uint8_t source_g =
        static_cast<uint8_t>(static_cast<float>(source[1]) * scale_factor);
    uint8_t source_b =
        static_cast<uint8_t>(static_cast<float>(source[2]) * scale_factor);
    destination[0] = source_r;
    destination[1] = source_g;
    destination[2] = source_b;
    destination[3] = source[3];
    source += 4;
    destination += 4;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA8,
          WebGLImageConversion::kAlphaDoPremultiply,
          float,
          uint8_t>(const float* source,
                   uint8_t* destination,
                   unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = ClampAndScaleFloat<uint8_t>(source[0] * source[3]);
    destination[1] = ClampAndScaleFloat<uint8_t>(source[1] * source[3]);
    destination[2] = ClampAndScaleFloat<uint8_t>(source[2] * source[3]);
    destination[3] = ClampAndScaleFloat<uint8_t>(source[3]);
    source += 4;
    destination += 4;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA8,
          WebGLImageConversion::kAlphaDoUnmultiply,
          uint8_t,
          uint8_t>(const uint8_t* source,
                   uint8_t* destination,
                   unsigned pixels_per_row) {
#if defined(ARCH_CPU_X86_FAMILY)
  simd::PackOneRowOfRGBA8LittleToRGBA8(source, destination, pixels_per_row);
#endif
#if defined(HAVE_MIPS_MSA_INTRINSICS)
  simd::packOneRowOfRGBA8LittleToRGBA8MSA(source, destination, pixels_per_row);
#endif
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] ? 255.0f / source[3] : 1.0f;
    uint8_t source_r =
        static_cast<uint8_t>(static_cast<float>(source[0]) * scale_factor);
    uint8_t source_g =
        static_cast<uint8_t>(static_cast<float>(source[1]) * scale_factor);
    uint8_t source_b =
        static_cast<uint8_t>(static_cast<float>(source[2]) * scale_factor);
    destination[0] = source_r;
    destination[1] = source_g;
    destination[2] = source_b;
    destination[3] = source[3];
    source += 4;
    destination += 4;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA8,
          WebGLImageConversion::kAlphaDoUnmultiply,
          float,
          uint8_t>(const float* source,
                   uint8_t* destination,
                   unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] ? 1.0f / source[3] : 1.0f;
    destination[0] = ClampAndScaleFloat<uint8_t>(source[0] * scale_factor);
    destination[1] = ClampAndScaleFloat<uint8_t>(source[1] * scale_factor);
    destination[2] = ClampAndScaleFloat<uint8_t>(source[2] * scale_factor);
    destination[3] = ClampAndScaleFloat<uint8_t>(source[3]);
    source += 4;
    destination += 4;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA4444,
          WebGLImageConversion::kAlphaDoNothing,
          uint8_t,
          uint16_t>(const uint8_t* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
#if defined(CPU_ARM_NEON)
  simd::PackOneRowOfRGBA8ToUnsignedShort4444(source, destination,
                                             pixels_per_row);
#endif
#if defined(HAVE_MIPS_MSA_INTRINSICS)
  simd::packOneRowOfRGBA8ToUnsignedShort4444MSA(source, destination,
                                                pixels_per_row);
#endif
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    *destination = (((source[0] & 0xF0) << 8) | ((source[1] & 0xF0) << 4) |
                    (source[2] & 0xF0) | (source[3] >> 4));
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA4444,
          WebGLImageConversion::kAlphaDoNothing,
          float,
          uint16_t>(const float* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    uint8_t r = ClampAndScaleFloat<uint8_t>(source[0]);
    uint8_t g = ClampAndScaleFloat<uint8_t>(source[1]);
    uint8_t b = ClampAndScaleFloat<uint8_t>(source[2]);
    uint8_t a = ClampAndScaleFloat<uint8_t>(source[3]);
    *destination =
        (((r & 0xF0) << 8) | ((g & 0xF0) << 4) | (b & 0xF0) | (a >> 4));
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA4444,
          WebGLImageConversion::kAlphaDoPremultiply,
          uint8_t,
          uint16_t>(const uint8_t* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] / 255.0f;
    uint8_t source_r =
        static_cast<uint8_t>(static_cast<float>(source[0]) * scale_factor);
    uint8_t source_g =
        static_cast<uint8_t>(static_cast<float>(source[1]) * scale_factor);
    uint8_t source_b =
        static_cast<uint8_t>(static_cast<float>(source[2]) * scale_factor);
    *destination = (((source_r & 0xF0) << 8) | ((source_g & 0xF0) << 4) |
                    (source_b & 0xF0) | (source[3] >> 4));
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA4444,
          WebGLImageConversion::kAlphaDoPremultiply,
          float,
          uint16_t>(const float* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    uint8_t r = ClampAndScaleFloat<uint8_t>(source[0] * source[3]);
    uint8_t g = ClampAndScaleFloat<uint8_t>(source[1] * source[3]);
    uint8_t b = ClampAndScaleFloat<uint8_t>(source[2] * source[3]);
    uint8_t a = ClampAndScaleFloat<uint8_t>(source[3]);
    *destination =
        (((r & 0xF0) << 8) | ((g & 0xF0) << 4) | (b & 0xF0) | (a >> 4));
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA4444,
          WebGLImageConversion::kAlphaDoUnmultiply,
          uint8_t,
          uint16_t>(const uint8_t* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] ? 255.0f / source[3] : 1.0f;
    uint8_t source_r =
        static_cast<uint8_t>(static_cast<float>(source[0]) * scale_factor);
    uint8_t source_g =
        static_cast<uint8_t>(static_cast<float>(source[1]) * scale_factor);
    uint8_t source_b =
        static_cast<uint8_t>(static_cast<float>(source[2]) * scale_factor);
    *destination = (((source_r & 0xF0) << 8) | ((source_g & 0xF0) << 4) |
                    (source_b & 0xF0) | (source[3] >> 4));
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA4444,
          WebGLImageConversion::kAlphaDoUnmultiply,
          float,
          uint16_t>(const float* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] ? 1.0f / source[3] : 1.0f;
    uint8_t r = ClampAndScaleFloat<uint8_t>(source[0] * scale_factor);
    uint8_t g = ClampAndScaleFloat<uint8_t>(source[1] * scale_factor);
    uint8_t b = ClampAndScaleFloat<uint8_t>(source[2] * scale_factor);
    uint8_t a = ClampAndScaleFloat<uint8_t>(source[3]);
    *destination =
        (((r & 0xF0) << 8) | ((g & 0xF0) << 4) | (b & 0xF0) | (a >> 4));
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA5551,
          WebGLImageConversion::kAlphaDoNothing,
          uint8_t,
          uint16_t>(const uint8_t* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
#if defined(CPU_ARM_NEON)
  simd::PackOneRowOfRGBA8ToUnsignedShort5551(source, destination,
                                             pixels_per_row);
#endif
#if defined(HAVE_MIPS_MSA_INTRINSICS)
  simd::packOneRowOfRGBA8ToUnsignedShort5551MSA(source, destination,
                                                pixels_per_row);
#endif
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    *destination = (((source[0] & 0xF8) << 8) | ((source[1] & 0xF8) << 3) |
                    ((source[2] & 0xF8) >> 2) | (source[3] >> 7));
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA5551,
          WebGLImageConversion::kAlphaDoNothing,
          float,
          uint16_t>(const float* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    uint8_t r = ClampAndScaleFloat<uint8_t>(source[0]);
    uint8_t g = ClampAndScaleFloat<uint8_t>(source[1]);
    uint8_t b = ClampAndScaleFloat<uint8_t>(source[2]);
    uint8_t a = ClampAndScaleFloat<uint8_t>(source[3]);
    *destination =
        (((r & 0xF8) << 8) | ((g & 0xF8) << 3) | ((b & 0xF8) >> 2) | (a >> 7));
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA5551,
          WebGLImageConversion::kAlphaDoPremultiply,
          uint8_t,
          uint16_t>(const uint8_t* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] / 255.0f;
    uint8_t source_r =
        static_cast<uint8_t>(static_cast<float>(source[0]) * scale_factor);
    uint8_t source_g =
        static_cast<uint8_t>(static_cast<float>(source[1]) * scale_factor);
    uint8_t source_b =
        static_cast<uint8_t>(static_cast<float>(source[2]) * scale_factor);
    *destination = (((source_r & 0xF8) << 8) | ((source_g & 0xF8) << 3) |
                    ((source_b & 0xF8) >> 2) | (source[3] >> 7));
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA5551,
          WebGLImageConversion::kAlphaDoPremultiply,
          float,
          uint16_t>(const float* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    uint8_t r = ClampAndScaleFloat<uint8_t>(source[0] * source[3]);
    uint8_t g = ClampAndScaleFloat<uint8_t>(source[1] * source[3]);
    uint8_t b = ClampAndScaleFloat<uint8_t>(source[2] * source[3]);
    uint8_t a = ClampAndScaleFloat<uint8_t>(source[3]);
    *destination =
        (((r & 0xF8) << 8) | ((g & 0xF8) << 3) | ((b & 0xF8) >> 2) | (a >> 7));
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA5551,
          WebGLImageConversion::kAlphaDoUnmultiply,
          uint8_t,
          uint16_t>(const uint8_t* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] ? 255.0f / source[3] : 1.0f;
    uint8_t source_r =
        static_cast<uint8_t>(static_cast<float>(source[0]) * scale_factor);
    uint8_t source_g =
        static_cast<uint8_t>(static_cast<float>(source[1]) * scale_factor);
    uint8_t source_b =
        static_cast<uint8_t>(static_cast<float>(source[2]) * scale_factor);
    *destination = (((source_r & 0xF8) << 8) | ((source_g & 0xF8) << 3) |
                    ((source_b & 0xF8) >> 2) | (source[3] >> 7));
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA5551,
          WebGLImageConversion::kAlphaDoUnmultiply,
          float,
          uint16_t>(const float* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] ? 1.0f / source[3] : 1.0f;
    uint8_t r = ClampAndScaleFloat<uint8_t>(source[0] * scale_factor);
    uint8_t g = ClampAndScaleFloat<uint8_t>(source[1] * scale_factor);
    uint8_t b = ClampAndScaleFloat<uint8_t>(source[2] * scale_factor);
    uint8_t a = ClampAndScaleFloat<uint8_t>(source[3]);
    *destination =
        (((r & 0xF8) << 8) | ((g & 0xF8) << 3) | ((b & 0xF8) >> 2) | (a >> 7));
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGB565,
          WebGLImageConversion::kAlphaDoNothing,
          uint8_t,
          uint16_t>(const uint8_t* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
#if defined(CPU_ARM_NEON)
  simd::PackOneRowOfRGBA8ToUnsignedShort565(source, destination,
                                            pixels_per_row);
#endif
#if defined(HAVE_MIPS_MSA_INTRINSICS)
  simd::packOneRowOfRGBA8ToUnsignedShort565MSA(source, destination,
                                               pixels_per_row);
#endif
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    *destination = (((source[0] & 0xF8) << 8) | ((source[1] & 0xFC) << 3) |
                    ((source[2] & 0xF8) >> 3));
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGB565,
          WebGLImageConversion::kAlphaDoNothing,
          float,
          uint16_t>(const float* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    uint8_t r = ClampAndScaleFloat<uint8_t>(source[0]);
    uint8_t g = ClampAndScaleFloat<uint8_t>(source[1]);
    uint8_t b = ClampAndScaleFloat<uint8_t>(source[2]);
    *destination = (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3));
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGB565,
          WebGLImageConversion::kAlphaDoPremultiply,
          uint8_t,
          uint16_t>(const uint8_t* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] / 255.0f;
    uint8_t source_r =
        static_cast<uint8_t>(static_cast<float>(source[0]) * scale_factor);
    uint8_t source_g =
        static_cast<uint8_t>(static_cast<float>(source[1]) * scale_factor);
    uint8_t source_b =
        static_cast<uint8_t>(static_cast<float>(source[2]) * scale_factor);
    *destination = (((source_r & 0xF8) << 8) | ((source_g & 0xFC) << 3) |
                    ((source_b & 0xF8) >> 3));
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGB565,
          WebGLImageConversion::kAlphaDoPremultiply,
          float,
          uint16_t>(const float* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    uint8_t r = ClampAndScaleFloat<uint8_t>(source[0] * source[3]);
    uint8_t g = ClampAndScaleFloat<uint8_t>(source[1] * source[3]);
    uint8_t b = ClampAndScaleFloat<uint8_t>(source[2] * source[3]);
    *destination = (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3));
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGB565,
          WebGLImageConversion::kAlphaDoUnmultiply,
          uint8_t,
          uint16_t>(const uint8_t* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] ? 255.0f / source[3] : 1.0f;
    uint8_t source_r =
        static_cast<uint8_t>(static_cast<float>(source[0]) * scale_factor);
    uint8_t source_g =
        static_cast<uint8_t>(static_cast<float>(source[1]) * scale_factor);
    uint8_t source_b =
        static_cast<uint8_t>(static_cast<float>(source[2]) * scale_factor);
    *destination = (((source_r & 0xF8) << 8) | ((source_g & 0xFC) << 3) |
                    ((source_b & 0xF8) >> 3));
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGB565,
          WebGLImageConversion::kAlphaDoUnmultiply,
          float,
          uint16_t>(const float* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] ? 1.0f / source[3] : 1.0f;
    uint8_t r = ClampAndScaleFloat<uint8_t>(source[0] * scale_factor);
    uint8_t g = ClampAndScaleFloat<uint8_t>(source[1] * scale_factor);
    uint8_t b = ClampAndScaleFloat<uint8_t>(source[2] * scale_factor);
    *destination = (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3));
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGB32F,
          WebGLImageConversion::kAlphaDoNothing,
          float,
          float>(const float* source,
                 float* destination,
                 unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = source[0];
    destination[1] = source[1];
    destination[2] = source[2];
    source += 4;
    destination += 3;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGB32F,
          WebGLImageConversion::kAlphaDoPremultiply,
          float,
          float>(const float* source,
                 float* destination,
                 unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3];
    destination[0] = source[0] * scale_factor;
    destination[1] = source[1] * scale_factor;
    destination[2] = source[2] * scale_factor;
    source += 4;
    destination += 3;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGB32F,
          WebGLImageConversion::kAlphaDoUnmultiply,
          float,
          float>(const float* source,
                 float* destination,
                 unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] ? 1.0f / source[3] : 1.0f;
    destination[0] = source[0] * scale_factor;
    destination[1] = source[1] * scale_factor;
    destination[2] = source[2] * scale_factor;
    source += 4;
    destination += 3;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA32F,
          WebGLImageConversion::kAlphaDoPremultiply,
          float,
          float>(const float* source,
                 float* destination,
                 unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3];
    destination[0] = source[0] * scale_factor;
    destination[1] = source[1] * scale_factor;
    destination[2] = source[2] * scale_factor;
    destination[3] = source[3];
    source += 4;
    destination += 4;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA32F,
          WebGLImageConversion::kAlphaDoUnmultiply,
          float,
          float>(const float* source,
                 float* destination,
                 unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] ? 1.0f / source[3] : 1.0f;
    destination[0] = source[0] * scale_factor;
    destination[1] = source[1] * scale_factor;
    destination[2] = source[2] * scale_factor;
    destination[3] = source[3];
    source += 4;
    destination += 4;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatA32F,
          WebGLImageConversion::kAlphaDoNothing,
          float,
          float>(const float* source,
                 float* destination,
                 unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = source[3];
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatR32F,
          WebGLImageConversion::kAlphaDoNothing,
          float,
          float>(const float* source,
                 float* destination,
                 unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = source[0];
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatR32F,
          WebGLImageConversion::kAlphaDoPremultiply,
          float,
          float>(const float* source,
                 float* destination,
                 unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3];
    destination[0] = source[0] * scale_factor;
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatR32F,
          WebGLImageConversion::kAlphaDoUnmultiply,
          float,
          float>(const float* source,
                 float* destination,
                 unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] ? 1.0f / source[3] : 1.0f;
    destination[0] = source[0] * scale_factor;
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRA32F,
          WebGLImageConversion::kAlphaDoNothing,
          float,
          float>(const float* source,
                 float* destination,
                 unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = source[0];
    destination[1] = source[3];
    source += 4;
    destination += 2;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRA32F,
          WebGLImageConversion::kAlphaDoPremultiply,
          float,
          float>(const float* source,
                 float* destination,
                 unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3];
    destination[0] = source[0] * scale_factor;
    destination[1] = source[3];
    source += 4;
    destination += 2;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRA32F,
          WebGLImageConversion::kAlphaDoUnmultiply,
          float,
          float>(const float* source,
                 float* destination,
                 unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] ? 1.0f / source[3] : 1.0f;
    destination[0] = source[0] * scale_factor;
    destination[1] = source[3];
    source += 4;
    destination += 2;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA16F,
          WebGLImageConversion::kAlphaDoNothing,
          float,
          uint16_t>(const float* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = ConvertFloatToHalfFloat(source[0]);
    destination[1] = ConvertFloatToHalfFloat(source[1]);
    destination[2] = ConvertFloatToHalfFloat(source[2]);
    destination[3] = ConvertFloatToHalfFloat(source[3]);
    source += 4;
    destination += 4;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA16F,
          WebGLImageConversion::kAlphaDoPremultiply,
          float,
          uint16_t>(const float* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3];
    destination[0] = ConvertFloatToHalfFloat(source[0] * scale_factor);
    destination[1] = ConvertFloatToHalfFloat(source[1] * scale_factor);
    destination[2] = ConvertFloatToHalfFloat(source[2] * scale_factor);
    destination[3] = ConvertFloatToHalfFloat(source[3]);
    source += 4;
    destination += 4;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA16F,
          WebGLImageConversion::kAlphaDoUnmultiply,
          float,
          uint16_t>(const float* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] ? 1.0f / source[3] : 1.0f;
    destination[0] = ConvertFloatToHalfFloat(source[0] * scale_factor);
    destination[1] = ConvertFloatToHalfFloat(source[1] * scale_factor);
    destination[2] = ConvertFloatToHalfFloat(source[2] * scale_factor);
    destination[3] = ConvertFloatToHalfFloat(source[3]);
    source += 4;
    destination += 4;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGB16F,
          WebGLImageConversion::kAlphaDoNothing,
          float,
          uint16_t>(const float* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = ConvertFloatToHalfFloat(source[0]);
    destination[1] = ConvertFloatToHalfFloat(source[1]);
    destination[2] = ConvertFloatToHalfFloat(source[2]);
    source += 4;
    destination += 3;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGB16F,
          WebGLImageConversion::kAlphaDoPremultiply,
          float,
          uint16_t>(const float* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3];
    destination[0] = ConvertFloatToHalfFloat(source[0] * scale_factor);
    destination[1] = ConvertFloatToHalfFloat(source[1] * scale_factor);
    destination[2] = ConvertFloatToHalfFloat(source[2] * scale_factor);
    source += 4;
    destination += 3;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGB16F,
          WebGLImageConversion::kAlphaDoUnmultiply,
          float,
          uint16_t>(const float* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] ? 1.0f / source[3] : 1.0f;
    destination[0] = ConvertFloatToHalfFloat(source[0] * scale_factor);
    destination[1] = ConvertFloatToHalfFloat(source[1] * scale_factor);
    destination[2] = ConvertFloatToHalfFloat(source[2] * scale_factor);
    source += 4;
    destination += 3;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRA16F,
          WebGLImageConversion::kAlphaDoNothing,
          float,
          uint16_t>(const float* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = ConvertFloatToHalfFloat(source[0]);
    destination[1] = ConvertFloatToHalfFloat(source[3]);
    source += 4;
    destination += 2;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRA16F,
          WebGLImageConversion::kAlphaDoPremultiply,
          float,
          uint16_t>(const float* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3];
    destination[0] = ConvertFloatToHalfFloat(source[0] * scale_factor);
    destination[1] = ConvertFloatToHalfFloat(source[3]);
    source += 4;
    destination += 2;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRA16F,
          WebGLImageConversion::kAlphaDoUnmultiply,
          float,
          uint16_t>(const float* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] ? 1.0f / source[3] : 1.0f;
    destination[0] = ConvertFloatToHalfFloat(source[0] * scale_factor);
    destination[1] = ConvertFloatToHalfFloat(source[3]);
    source += 4;
    destination += 2;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatR16F,
          WebGLImageConversion::kAlphaDoNothing,
          float,
          uint16_t>(const float* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = ConvertFloatToHalfFloat(source[0]);
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatR16F,
          WebGLImageConversion::kAlphaDoPremultiply,
          float,
          uint16_t>(const float* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3];
    destination[0] = ConvertFloatToHalfFloat(source[0] * scale_factor);
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatR16F,
          WebGLImageConversion::kAlphaDoUnmultiply,
          float,
          uint16_t>(const float* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] ? 1.0f / source[3] : 1.0f;
    destination[0] = ConvertFloatToHalfFloat(source[0] * scale_factor);
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatA16F,
          WebGLImageConversion::kAlphaDoNothing,
          float,
          uint16_t>(const float* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = ConvertFloatToHalfFloat(source[3]);
    source += 4;
    destination += 1;
  }
}

// Can not be targeted by DOM uploads, so does not need to support float
// input data.

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA8_S,
          WebGLImageConversion::kAlphaDoPremultiply,
          int8_t,
          int8_t>(const int8_t* source,
                  int8_t* destination,
                  unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[3] = ClampMin(source[3]);
    float scale_factor = static_cast<float>(destination[3]) / kMaxInt8Value;
    destination[0] = static_cast<int8_t>(
        static_cast<float>(ClampMin(source[0])) * scale_factor);
    destination[1] = static_cast<int8_t>(
        static_cast<float>(ClampMin(source[1])) * scale_factor);
    destination[2] = static_cast<int8_t>(
        static_cast<float>(ClampMin(source[2])) * scale_factor);
    source += 4;
    destination += 4;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA16,
          WebGLImageConversion::kAlphaDoPremultiply,
          uint16_t,
          uint16_t>(const uint16_t* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = static_cast<float>(source[3]) / kMaxUInt16Value;
    destination[0] =
        static_cast<uint16_t>(static_cast<float>(source[0]) * scale_factor);
    destination[1] =
        static_cast<uint16_t>(static_cast<float>(source[1]) * scale_factor);
    destination[2] =
        static_cast<uint16_t>(static_cast<float>(source[2]) * scale_factor);
    destination[3] = source[3];
    source += 4;
    destination += 4;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA16,
          WebGLImageConversion::kAlphaDoPremultiply,
          float,
          uint16_t>(const float* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = ClampAndScaleFloat<uint16_t>(source[0] * source[3]);
    destination[1] = ClampAndScaleFloat<uint16_t>(source[1] * source[3]);
    destination[2] = ClampAndScaleFloat<uint16_t>(source[2] * source[3]);
    destination[3] = ClampAndScaleFloat<uint16_t>(source[3]);
    source += 4;
    destination += 4;
  }
}

// Can not be targeted by DOM uploads, so does not need to support float
// input data.

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA16_S,
          WebGLImageConversion::kAlphaDoPremultiply,
          int16_t,
          int16_t>(const int16_t* source,
                   int16_t* destination,
                   unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[3] = ClampMin(source[3]);
    float scale_factor = static_cast<float>(destination[3]) / kMaxInt16Value;
    destination[0] = static_cast<int16_t>(
        static_cast<float>(ClampMin(source[0])) * scale_factor);
    destination[1] = static_cast<int16_t>(
        static_cast<float>(ClampMin(source[1])) * scale_factor);
    destination[2] = static_cast<int16_t>(
        static_cast<float>(ClampMin(source[2])) * scale_factor);
    source += 4;
    destination += 4;
  }
}

// Can not be targeted by DOM uploads, so does not need to support float
// input data.

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA32,
          WebGLImageConversion::kAlphaDoPremultiply,
          uint32_t,
          uint32_t>(const uint32_t* source,
                    uint32_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    double scale_factor = static_cast<double>(source[3]) / kMaxUInt32Value;
    destination[0] =
        static_cast<uint32_t>(static_cast<double>(source[0]) * scale_factor);
    destination[1] =
        static_cast<uint32_t>(static_cast<double>(source[1]) * scale_factor);
    destination[2] =
        static_cast<uint32_t>(static_cast<double>(source[2]) * scale_factor);
    destination[3] = source[3];
    source += 4;
    destination += 4;
  }
}

// Can not be targeted by DOM uploads, so does not need to support float
// input data.

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA32_S,
          WebGLImageConversion::kAlphaDoPremultiply,
          int32_t,
          int32_t>(const int32_t* source,
                   int32_t* destination,
                   unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[3] = ClampMin(source[3]);
    double scale_factor = static_cast<double>(destination[3]) / kMaxInt32Value;
    destination[0] = static_cast<int32_t>(
        static_cast<double>(ClampMin(source[0])) * scale_factor);
    destination[1] = static_cast<int32_t>(
        static_cast<double>(ClampMin(source[1])) * scale_factor);
    destination[2] = static_cast<int32_t>(
        static_cast<double>(ClampMin(source[2])) * scale_factor);
    source += 4;
    destination += 4;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA2_10_10_10,
          WebGLImageConversion::kAlphaDoNothing,
          float,
          uint32_t>(const float* source,
                    uint32_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    uint32_t r = static_cast<uint32_t>(source[0] * 1023.0f);
    uint32_t g = static_cast<uint32_t>(source[1] * 1023.0f);
    uint32_t b = static_cast<uint32_t>(source[2] * 1023.0f);
    uint32_t a = static_cast<uint32_t>(source[3] * 3.0f);
    destination[0] = (a << 30) | (b << 20) | (g << 10) | r;
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA2_10_10_10,
          WebGLImageConversion::kAlphaDoPremultiply,
          float,
          uint32_t>(const float* source,
                    uint32_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    uint32_t r = static_cast<uint32_t>(source[0] * source[3] * 1023.0f);
    uint32_t g = static_cast<uint32_t>(source[1] * source[3] * 1023.0f);
    uint32_t b = static_cast<uint32_t>(source[2] * source[3] * 1023.0f);
    uint32_t a = static_cast<uint32_t>(source[3] * 3.0f);
    destination[0] = (a << 30) | (b << 20) | (g << 10) | r;
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA2_10_10_10,
          WebGLImageConversion::kAlphaDoUnmultiply,
          float,
          uint32_t>(const float* source,
                    uint32_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] ? 1023.0f / source[3] : 1023.0f;
    uint32_t r = static_cast<uint32_t>(source[0] * scale_factor);
    uint32_t g = static_cast<uint32_t>(source[1] * scale_factor);
    uint32_t b = static_cast<uint32_t>(source[2] * scale_factor);
    uint32_t a = static_cast<uint32_t>(source[3] * 3.0f);
    destination[0] = (a << 30) | (b << 20) | (g << 10) | r;
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRG8,
          WebGLImageConversion::kAlphaDoNothing,
          uint8_t,
          uint8_t>(const uint8_t* source,
                   uint8_t* destination,
                   unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = source[0];
    destination[1] = source[1];
    source += 4;
    destination += 2;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRG8,
          WebGLImageConversion::kAlphaDoNothing,
          float,
          uint8_t>(const float* source,
                   uint8_t* destination,
                   unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = ClampAndScaleFloat<uint8_t>(source[0]);
    destination[1] = ClampAndScaleFloat<uint8_t>(source[1]);
    source += 4;
    destination += 2;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRG8,
          WebGLImageConversion::kAlphaDoPremultiply,
          uint8_t,
          uint8_t>(const uint8_t* source,
                   uint8_t* destination,
                   unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = static_cast<float>(source[3]) / kMaxUInt8Value;
    destination[0] =
        static_cast<uint8_t>(static_cast<float>(source[0]) * scale_factor);
    destination[1] =
        static_cast<uint8_t>(static_cast<float>(source[1]) * scale_factor);
    source += 4;
    destination += 2;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRG8,
          WebGLImageConversion::kAlphaDoPremultiply,
          float,
          uint8_t>(const float* source,
                   uint8_t* destination,
                   unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = ClampAndScaleFloat<uint8_t>(source[0] * source[3]);
    destination[1] = ClampAndScaleFloat<uint8_t>(source[1] * source[3]);
    source += 4;
    destination += 2;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRG8,
          WebGLImageConversion::kAlphaDoUnmultiply,
          uint8_t,
          uint8_t>(const uint8_t* source,
                   uint8_t* destination,
                   unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor =
        source[3] ? kMaxUInt8Value / static_cast<float>(source[3]) : 1.0f;
    destination[0] =
        static_cast<uint8_t>(static_cast<float>(source[0]) * scale_factor);
    destination[1] =
        static_cast<uint8_t>(static_cast<float>(source[1]) * scale_factor);
    source += 4;
    destination += 2;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRG8,
          WebGLImageConversion::kAlphaDoUnmultiply,
          float,
          uint8_t>(const float* source,
                   uint8_t* destination,
                   unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] ? 1.0f / source[3] : 1.0f;
    destination[0] = ClampAndScaleFloat<uint8_t>(source[0] * scale_factor);
    destination[1] = ClampAndScaleFloat<uint8_t>(source[1] * scale_factor);
    source += 4;
    destination += 2;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRG16F,
          WebGLImageConversion::kAlphaDoNothing,
          float,
          uint16_t>(const float* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = ConvertFloatToHalfFloat(source[0]);
    destination[1] = ConvertFloatToHalfFloat(source[1]);
    source += 4;
    destination += 2;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRG16F,
          WebGLImageConversion::kAlphaDoPremultiply,
          float,
          uint16_t>(const float* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3];
    destination[0] = ConvertFloatToHalfFloat(source[0] * scale_factor);
    destination[1] = ConvertFloatToHalfFloat(source[1] * scale_factor);
    source += 4;
    destination += 2;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRG16F,
          WebGLImageConversion::kAlphaDoUnmultiply,
          float,
          uint16_t>(const float* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] ? 1.0f / source[3] : 1.0f;
    destination[0] = ConvertFloatToHalfFloat(source[0] * scale_factor);
    destination[1] = ConvertFloatToHalfFloat(source[1] * scale_factor);
    source += 4;
    destination += 2;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRG32F,
          WebGLImageConversion::kAlphaDoNothing,
          float,
          float>(const float* source,
                 float* destination,
                 unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = source[0];
    destination[1] = source[1];
    source += 4;
    destination += 2;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRG32F,
          WebGLImageConversion::kAlphaDoPremultiply,
          float,
          float>(const float* source,
                 float* destination,
                 unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3];
    destination[0] = source[0] * scale_factor;
    destination[1] = source[1] * scale_factor;
    source += 4;
    destination += 2;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRG32F,
          WebGLImageConversion::kAlphaDoUnmultiply,
          float,
          float>(const float* source,
                 float* destination,
                 unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] ? 1.0f / source[3] : 1.0f;
    destination[0] = source[0] * scale_factor;
    destination[1] = source[1] * scale_factor;
    source += 4;
    destination += 2;
  }
}

bool HasAlpha(int format) {
  return format == WebGLImageConversion::kDataFormatA8 ||
         format == WebGLImageConversion::kDataFormatA16F ||
         format == WebGLImageConversion::kDataFormatA32F ||
         format == WebGLImageConversion::kDataFormatRA8 ||
         format == WebGLImageConversion::kDataFormatAR8 ||
         format == WebGLImageConversion::kDataFormatRA16F ||
         format == WebGLImageConversion::kDataFormatRA32F ||
         format == WebGLImageConversion::kDataFormatRGBA8 ||
         format == WebGLImageConversion::kDataFormatBGRA8 ||
         format == WebGLImageConversion::kDataFormatARGB8 ||
         format == WebGLImageConversion::kDataFormatABGR8 ||
         format == WebGLImageConversion::kDataFormatRGBA16F ||
         format == WebGLImageConversion::kDataFormatRGBA32F ||
         format == WebGLImageConversion::kDataFormatRGBA4444 ||
         format == WebGLImageConversion::kDataFormatRGBA5551 ||
         format == WebGLImageConversion::kDataFormatRGBA8_S ||
         format == WebGLImageConversion::kDataFormatRGBA16 ||
         format == WebGLImageConversion::kDataFormatRGBA16_S ||
         format == WebGLImageConversion::kDataFormatRGBA32 ||
         format == WebGLImageConversion::kDataFormatRGBA32_S ||
         format == WebGLImageConversion::kDataFormatRGBA2_10_10_10;
}

bool HasColor(int format) {
  return format == WebGLImageConversion::kDataFormatRGBA8 ||
         format == WebGLImageConversion::kDataFormatRGBA16F ||
         format == WebGLImageConversion::kDataFormatRGBA32F ||
         format == WebGLImageConversion::kDataFormatRGB8 ||
         format == WebGLImageConversion::kDataFormatRGB16F ||
         format == WebGLImageConversion::kDataFormatRGB32F ||
         format == WebGLImageConversion::kDataFormatBGR8 ||
         format == WebGLImageConversion::kDataFormatBGRA8 ||
         format == WebGLImageConversion::kDataFormatARGB8 ||
         format == WebGLImageConversion::kDataFormatABGR8 ||
         format == WebGLImageConversion::kDataFormatRGBA5551 ||
         format == WebGLImageConversion::kDataFormatRGBA4444 ||
         format == WebGLImageConversion::kDataFormatRGB565 ||
         format == WebGLImageConversion::kDataFormatR8 ||
         format == WebGLImageConversion::kDataFormatR16F ||
         format == WebGLImageConversion::kDataFormatR32F ||
         format == WebGLImageConversion::kDataFormatRA8 ||
         format == WebGLImageConversion::kDataFormatRA16F ||
         format == WebGLImageConversion::kDataFormatRA32F ||
         format == WebGLImageConversion::kDataFormatAR8 ||
         format == WebGLImageConversion::kDataFormatRGBA8_S ||
         format == WebGLImageConversion::kDataFormatRGBA16 ||
         format == WebGLImageConversion::kDataFormatRGBA16_S ||
         format == WebGLImageConversion::kDataFormatRGBA32 ||
         format == WebGLImageConversion::kDataFormatRGBA32_S ||
         format == WebGLImageConversion::kDataFormatRGBA2_10_10_10 ||
         format == WebGLImageConversion::kDataFormatRGB8_S ||
         format == WebGLImageConversion::kDataFormatRGB16 ||
         format == WebGLImageConversion::kDataFormatRGB16_S ||
         format == WebGLImageConversion::kDataFormatRGB32 ||
         format == WebGLImageConversion::kDataFormatRGB32_S ||
         format == WebGLImageConversion::kDataFormatRGB10F11F11F ||
         format == WebGLImageConversion::kDataFormatRGB5999 ||
         format == WebGLImageConversion::kDataFormatRG8 ||
         format == WebGLImageConversion::kDataFormatRG8_S ||
         format == WebGLImageConversion::kDataFormatRG16 ||
         format == WebGLImageConversion::kDataFormatRG16_S ||
         format == WebGLImageConversion::kDataFormatRG32 ||
         format == WebGLImageConversion::kDataFormatRG32_S ||
         format == WebGLImageConversion::kDataFormatRG16F ||
         format == WebGLImageConversion::kDataFormatRG32F ||
         format == WebGLImageConversion::kDataFormatR8_S ||
         format == WebGLImageConversion::kDataFormatR16 ||
         format == WebGLImageConversion::kDataFormatR16_S ||
         format == WebGLImageConversion::kDataFormatR32 ||
         format == WebGLImageConversion::kDataFormatR32_S;
}

template <int Format>
struct IsInt8Format {
  STATIC_ONLY(IsInt8Format);
  static const bool value =
      Format == WebGLImageConversion::kDataFormatRGBA8_S ||
      Format == WebGLImageConversion::kDataFormatRGB8_S ||
      Format == WebGLImageConversion::kDataFormatRG8_S ||
      Format == WebGLImageConversion::kDataFormatR8_S;
};

template <int Format>
struct IsInt16Format {
  STATIC_ONLY(IsInt16Format);
  static const bool value =
      Format == WebGLImageConversion::kDataFormatRGBA16_S ||
      Format == WebGLImageConversion::kDataFormatRGB16_S ||
      Format == WebGLImageConversion::kDataFormatRG16_S ||
      Format == WebGLImageConversion::kDataFormatR16_S;
};

template <int Format>
struct IsInt32Format {
  STATIC_ONLY(IsInt32Format);
  static const bool value =
      Format == WebGLImageConversion::kDataFormatRGBA32_S ||
      Format == WebGLImageConversion::kDataFormatRGB32_S ||
      Format == WebGLImageConversion::kDataFormatRG32_S ||
      Format == WebGLImageConversion::kDataFormatR32_S;
};

template <int Format>
struct IsUInt8Format {
  STATIC_ONLY(IsUInt8Format);
  static const bool value = Format == WebGLImageConversion::kDataFormatRGBA8 ||
                            Format == WebGLImageConversion::kDataFormatRGB8 ||
                            Format == WebGLImageConversion::kDataFormatRG8 ||
                            Format == WebGLImageConversion::kDataFormatR8 ||
                            Format == WebGLImageConversion::kDataFormatBGRA8 ||
                            Format == WebGLImageConversion::kDataFormatBGR8 ||
                            Format == WebGLImageConversion::kDataFormatARGB8 ||
                            Format == WebGLImageConversion::kDataFormatABGR8 ||
                            Format == WebGLImageConversion::kDataFormatRA8 ||
                            Format == WebGLImageConversion::kDataFormatAR8 ||
                            Format == WebGLImageConversion::kDataFormatA8;
};

template <int Format>
struct IsUInt16Format {
  STATIC_ONLY(IsUInt16Format);
  static const bool value = Format == WebGLImageConversion::kDataFormatRGBA16 ||
                            Format == WebGLImageConversion::kDataFormatRGB16 ||
                            Format == WebGLImageConversion::kDataFormatRG16 ||
                            Format == WebGLImageConversion::kDataFormatR16;
};

template <int Format>
struct IsUInt32Format {
  STATIC_ONLY(IsUInt32Format);
  static const bool value = Format == WebGLImageConversion::kDataFormatRGBA32 ||
                            Format == WebGLImageConversion::kDataFormatRGB32 ||
                            Format == WebGLImageConversion::kDataFormatRG32 ||
                            Format == WebGLImageConversion::kDataFormatR32;
};

template <int Format>
struct IsFloatFormat {
  STATIC_ONLY(IsFloatFormat);
  static const bool value =
      Format == WebGLImageConversion::kDataFormatRGBA32F ||
      Format == WebGLImageConversion::kDataFormatRGB32F ||
      Format == WebGLImageConversion::kDataFormatRA32F ||
      Format == WebGLImageConversion::kDataFormatR32F ||
      Format == WebGLImageConversion::kDataFormatA32F ||
      Format == WebGLImageConversion::kDataFormatRG32F;
};

template <int Format>
struct IsHalfFloatFormat {
  STATIC_ONLY(IsHalfFloatFormat);
  static const bool value =
      Format == WebGLImageConversion::kDataFormatRGBA16F ||
      Format == WebGLImageConversion::kDataFormatRGB16F ||
      Format == WebGLImageConversion::kDataFormatRA16F ||
      Format == WebGLImageConversion::kDataFormatR16F ||
      Format == WebGLImageConversion::kDataFormatA16F ||
      Format == WebGLImageConversion::kDataFormatRG16F;
};

template <int Format>
struct Is32bppFormat {
  STATIC_ONLY(Is32bppFormat);
  static const bool value =
      Format == WebGLImageConversion::kDataFormatRGBA2_10_10_10 ||
      Format == WebGLImageConversion::kDataFormatRGB5999 ||
      Format == WebGLImageConversion::kDataFormatRGB10F11F11F;
};

template <int Format>
struct Is16bppFormat {
  STATIC_ONLY(Is16bppFormat);
  static const bool value =
      Format == WebGLImageConversion::kDataFormatRGBA5551 ||
      Format == WebGLImageConversion::kDataFormatRGBA4444 ||
      Format == WebGLImageConversion::kDataFormatRGB565;
};

template <int Format,
          bool IsInt8Format = IsInt8Format<Format>::value,
          bool IsUInt8Format = IsUInt8Format<Format>::value,
          bool IsInt16Format = IsInt16Format<Format>::value,
          bool IsUInt16Format = IsUInt16Format<Format>::value,
          bool IsInt32Format = IsInt32Format<Format>::value,
          bool IsUInt32Format = IsUInt32Format<Format>::value,
          bool IsFloat = IsFloatFormat<Format>::value,
          bool IsHalfFloat = IsHalfFloatFormat<Format>::value,
          bool Is16bpp = Is16bppFormat<Format>::value,
          bool Is32bpp = Is32bppFormat<Format>::value>
struct DataTypeForFormat {
  STATIC_ONLY(DataTypeForFormat);
  typedef double Type;  // Use a type that's not used in unpack/pack.
};

template <int Format>
struct DataTypeForFormat<Format,
                         true,
                         false,
                         false,
                         false,
                         false,
                         false,
                         false,
                         false,
                         false,
                         false> {
  STATIC_ONLY(DataTypeForFormat);
  typedef int8_t Type;
};

template <int Format>
struct DataTypeForFormat<Format,
                         false,
                         true,
                         false,
                         false,
                         false,
                         false,
                         false,
                         false,
                         false,
                         false> {
  STATIC_ONLY(DataTypeForFormat);
  typedef uint8_t Type;
};

template <int Format>
struct DataTypeForFormat<Format,
                         false,
                         false,
                         true,
                         false,
                         false,
                         false,
                         false,
                         false,
                         false,
                         false> {
  STATIC_ONLY(DataTypeForFormat);
  typedef int16_t Type;
};

template <int Format>
struct DataTypeForFormat<Format,
                         false,
                         false,
                         false,
                         true,
                         false,
                         false,
                         false,
                         false,
                         false,
                         false> {
  STATIC_ONLY(DataTypeForFormat);
  typedef uint16_t Type;
};

template <int Format>
struct DataTypeForFormat<Format,
                         false,
                         false,
                         false,
                         false,
                         true,
                         false,
                         false,
                         false,
                         false,
                         false> {
  STATIC_ONLY(DataTypeForFormat);
  typedef int32_t Type;
};

template <int Format>
struct DataTypeForFormat<Format,
                         false,
                         false,
                         false,
                         false,
                         false,
                         true,
                         false,
                         false,
                         false,
                         false> {
  STATIC_ONLY(DataTypeForFormat);
  typedef uint32_t Type;
};

template <int Format>
struct DataTypeForFormat<Format,
                         false,
                         false,
                         false,
                         false,
                         false,
                         false,
                         true,
                         false,
                         false,
                         false> {
  STATIC_ONLY(DataTypeForFormat);
  typedef float Type;
};

template <int Format>
struct DataTypeForFormat<Format,
                         false,
                         false,
                         false,
                         false,
                         false,
                         false,
                         false,
                         true,
                         false,
                         false> {
  STATIC_ONLY(DataTypeForFormat);
  typedef uint16_t Type;
};

template <int Format>
struct DataTypeForFormat<Format,
                         false,
                         false,
                         false,
                         false,
                         false,
                         false,
                         false,
                         false,
                         true,
                         false> {
  STATIC_ONLY(DataTypeForFormat);
  typedef uint16_t Type;
};

template <int Format>
struct DataTypeForFormat<Format,
                         false,
                         false,
                         false,
                         false,
                         false,
                         false,
                         false,
                         false,
                         false,
                         true> {
  STATIC_ONLY(DataTypeForFormat);
  typedef uint32_t Type;
};

template <int Format>
struct UsesFloatIntermediateFormat {
  STATIC_ONLY(UsesFloatIntermediateFormat);
  static const bool value =
      IsFloatFormat<Format>::value || IsHalfFloatFormat<Format>::value ||
      Format == WebGLImageConversion::kDataFormatRGBA2_10_10_10 ||
      Format == WebGLImageConversion::kDataFormatRGB10F11F11F ||
      Format == WebGLImageConversion::kDataFormatRGB5999;
};

template <int Format>
struct IntermediateFormat {
  STATIC_ONLY(IntermediateFormat);
  static const int value =
      UsesFloatIntermediateFormat<Format>::value
          ? WebGLImageConversion::kDataFormatRGBA32F
          : IsInt32Format<Format>::value
                ? WebGLImageConversion::kDataFormatRGBA32_S
                : IsUInt32Format<Format>::value
                      ? WebGLImageConversion::kDataFormatRGBA32
                      : IsInt16Format<Format>::value
                            ? WebGLImageConversion::kDataFormatRGBA16_S
                            : (IsUInt16Format<Format>::value ||
                               Is32bppFormat<Format>::value)
                                  ? WebGLImageConversion::kDataFormatRGBA16
                                  : IsInt8Format<Format>::value
                                        ? WebGLImageConversion::
                                              kDataFormatRGBA8_S
                                        : WebGLImageConversion::
                                              kDataFormatRGBA8;
};

unsigned TexelBytesForFormat(WebGLImageConversion::DataFormat format) {
  switch (format) {
    case WebGLImageConversion::kDataFormatR8:
    case WebGLImageConversion::kDataFormatR8_S:
    case WebGLImageConversion::kDataFormatA8:
      return 1;
    case WebGLImageConversion::kDataFormatRG8:
    case WebGLImageConversion::kDataFormatRG8_S:
    case WebGLImageConversion::kDataFormatRA8:
    case WebGLImageConversion::kDataFormatAR8:
    case WebGLImageConversion::kDataFormatRGBA5551:
    case WebGLImageConversion::kDataFormatRGBA4444:
    case WebGLImageConversion::kDataFormatRGB565:
    case WebGLImageConversion::kDataFormatA16F:
    case WebGLImageConversion::kDataFormatR16:
    case WebGLImageConversion::kDataFormatR16_S:
    case WebGLImageConversion::kDataFormatR16F:
    case WebGLImageConversion::kDataFormatD16:
      return 2;
    case WebGLImageConversion::kDataFormatRGB8:
    case WebGLImageConversion::kDataFormatRGB8_S:
    case WebGLImageConversion::kDataFormatBGR8:
      return 3;
    case WebGLImageConversion::kDataFormatRGBA8:
    case WebGLImageConversion::kDataFormatRGBA8_S:
    case WebGLImageConversion::kDataFormatARGB8:
    case WebGLImageConversion::kDataFormatABGR8:
    case WebGLImageConversion::kDataFormatBGRA8:
    case WebGLImageConversion::kDataFormatR32:
    case WebGLImageConversion::kDataFormatR32_S:
    case WebGLImageConversion::kDataFormatR32F:
    case WebGLImageConversion::kDataFormatA32F:
    case WebGLImageConversion::kDataFormatRA16F:
    case WebGLImageConversion::kDataFormatRGBA2_10_10_10:
    case WebGLImageConversion::kDataFormatRGB10F11F11F:
    case WebGLImageConversion::kDataFormatRGB5999:
    case WebGLImageConversion::kDataFormatRG16:
    case WebGLImageConversion::kDataFormatRG16_S:
    case WebGLImageConversion::kDataFormatRG16F:
    case WebGLImageConversion::kDataFormatD32:
    case WebGLImageConversion::kDataFormatD32F:
    case WebGLImageConversion::kDataFormatDS24_8:
      return 4;
    case WebGLImageConversion::kDataFormatRGB16:
    case WebGLImageConversion::kDataFormatRGB16_S:
    case WebGLImageConversion::kDataFormatRGB16F:
      return 6;
    case WebGLImageConversion::kDataFormatRGBA16:
    case WebGLImageConversion::kDataFormatRGBA16_S:
    case WebGLImageConversion::kDataFormatRA32F:
    case WebGLImageConversion::kDataFormatRGBA16F:
    case WebGLImageConversion::kDataFormatRG32:
    case WebGLImageConversion::kDataFormatRG32_S:
    case WebGLImageConversion::kDataFormatRG32F:
      return 8;
    case WebGLImageConversion::kDataFormatRGB32:
    case WebGLImageConversion::kDataFormatRGB32_S:
    case WebGLImageConversion::kDataFormatRGB32F:
      return 12;
    case WebGLImageConversion::kDataFormatRGBA32:
    case WebGLImageConversion::kDataFormatRGBA32_S:
    case WebGLImageConversion::kDataFormatRGBA32F:
      return 16;
    default:
      return 0;
  }
}

/* END CODE SHARED WITH MOZILLA FIREFOX */

class FormatConverter {
  STACK_ALLOCATED();

 public:
  FormatConverter(const IntRect& source_data_sub_rectangle,
                  int depth,
                  int unpack_image_height,
                  const void* src_start,
                  void* dst_start,
                  int src_stride,
                  int src_row_offset,
                  int dst_stride)
      : src_sub_rectangle_(source_data_sub_rectangle),
        depth_(depth),
        unpack_image_height_(unpack_image_height),
        src_start_(src_start),
        dst_start_(dst_start),
        src_stride_(src_stride),
        src_row_offset_(src_row_offset),
        dst_stride_(dst_stride),
        success_(false) {
    const unsigned kMaxNumberOfComponents = 4;
    const unsigned kMaxBytesPerComponent = 4;
    unpacked_intermediate_src_data_ = std::make_unique<uint8_t[]>(
        src_sub_rectangle_.Width() * kMaxNumberOfComponents *
        kMaxBytesPerComponent);
    DCHECK(unpacked_intermediate_src_data_.get());
  }

  void Convert(WebGLImageConversion::DataFormat src_format,
               WebGLImageConversion::DataFormat dst_format,
               WebGLImageConversion::AlphaOp);
  bool Success() const { return success_; }

 private:
  template <WebGLImageConversion::DataFormat SrcFormat>
  void Convert(WebGLImageConversion::DataFormat dst_format,
               WebGLImageConversion::AlphaOp);

  template <WebGLImageConversion::DataFormat SrcFormat,
            WebGLImageConversion::DataFormat DstFormat>
  void Convert(WebGLImageConversion::AlphaOp);

  template <WebGLImageConversion::DataFormat SrcFormat,
            WebGLImageConversion::DataFormat DstFormat,
            WebGLImageConversion::AlphaOp alphaOp>
  void Convert();

  const IntRect& src_sub_rectangle_;
  const int depth_;
  const int unpack_image_height_;
  const void* const src_start_;
  void* const dst_start_;
  const int src_stride_, src_row_offset_, dst_stride_;
  bool success_;
  std::unique_ptr<uint8_t[]> unpacked_intermediate_src_data_;
};

void FormatConverter::Convert(WebGLImageConversion::DataFormat src_format,
                              WebGLImageConversion::DataFormat dst_format,
                              WebGLImageConversion::AlphaOp alpha_op) {
#define FORMATCONVERTER_CASE_SRCFORMAT(SrcFormat) \
  case SrcFormat:                                 \
    return Convert<SrcFormat>(dst_format, alpha_op);

  switch (src_format) {
    FORMATCONVERTER_CASE_SRCFORMAT(WebGLImageConversion::kDataFormatRA8)
    FORMATCONVERTER_CASE_SRCFORMAT(WebGLImageConversion::kDataFormatRA32F)
    FORMATCONVERTER_CASE_SRCFORMAT(WebGLImageConversion::kDataFormatRGBA8)
    FORMATCONVERTER_CASE_SRCFORMAT(WebGLImageConversion::kDataFormatARGB8)
    FORMATCONVERTER_CASE_SRCFORMAT(WebGLImageConversion::kDataFormatABGR8)
    FORMATCONVERTER_CASE_SRCFORMAT(WebGLImageConversion::kDataFormatAR8)
    FORMATCONVERTER_CASE_SRCFORMAT(WebGLImageConversion::kDataFormatBGRA8)
    FORMATCONVERTER_CASE_SRCFORMAT(WebGLImageConversion::kDataFormatRGBA5551)
    FORMATCONVERTER_CASE_SRCFORMAT(WebGLImageConversion::kDataFormatRGBA4444)
    FORMATCONVERTER_CASE_SRCFORMAT(WebGLImageConversion::kDataFormatRGBA32F)
    FORMATCONVERTER_CASE_SRCFORMAT(
        WebGLImageConversion::kDataFormatRGBA2_10_10_10)
    // Only used by ImageBitmap, when colorspace conversion is needed.
    FORMATCONVERTER_CASE_SRCFORMAT(WebGLImageConversion::kDataFormatRGBA16F)
    default:
      NOTREACHED();
  }
#undef FORMATCONVERTER_CASE_SRCFORMAT
}

template <WebGLImageConversion::DataFormat SrcFormat>
void FormatConverter::Convert(WebGLImageConversion::DataFormat dst_format,
                              WebGLImageConversion::AlphaOp alpha_op) {
#define FORMATCONVERTER_CASE_DSTFORMAT(DstFormat) \
  case DstFormat:                                 \
    return Convert<SrcFormat, DstFormat>(alpha_op);

  switch (dst_format) {
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatR8)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatR16F)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatR32F)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatA8)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatA16F)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatA32F)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatRA8)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatRA16F)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatRA32F)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatRGB8)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatRGB565)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatRGB16F)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatRGB32F)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatRGBA8)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatRGBA5551)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatRGBA4444)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatRGBA16F)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatRGBA32F)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatRGBA8_S)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatRGBA16)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatRGBA16_S)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatRGBA32)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatRGBA32_S)
    FORMATCONVERTER_CASE_DSTFORMAT(
        WebGLImageConversion::kDataFormatRGBA2_10_10_10)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatRG8)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatRG16F)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatRG32F)
    default:
      NOTREACHED();
  }

#undef FORMATCONVERTER_CASE_DSTFORMAT
}

template <WebGLImageConversion::DataFormat SrcFormat,
          WebGLImageConversion::DataFormat DstFormat>
void FormatConverter::Convert(WebGLImageConversion::AlphaOp alpha_op) {
#define FORMATCONVERTER_CASE_ALPHAOP(alphaOp) \
  case alphaOp:                               \
    return Convert<SrcFormat, DstFormat, alphaOp>();

  switch (alpha_op) {
    FORMATCONVERTER_CASE_ALPHAOP(WebGLImageConversion::kAlphaDoNothing)
    FORMATCONVERTER_CASE_ALPHAOP(WebGLImageConversion::kAlphaDoPremultiply)
    FORMATCONVERTER_CASE_ALPHAOP(WebGLImageConversion::kAlphaDoUnmultiply)
    default:
      NOTREACHED();
  }
#undef FORMATCONVERTER_CASE_ALPHAOP
}

template <int Format>
struct SupportsConversionFromDomElements {
  STATIC_ONLY(SupportsConversionFromDomElements);
  static const bool value =
      Format == WebGLImageConversion::kDataFormatRGBA8 ||
      Format == WebGLImageConversion::kDataFormatRGB8 ||
      Format == WebGLImageConversion::kDataFormatRG8 ||
      Format == WebGLImageConversion::kDataFormatRA8 ||
      Format == WebGLImageConversion::kDataFormatR8 ||
      Format == WebGLImageConversion::kDataFormatRGBA32F ||
      Format == WebGLImageConversion::kDataFormatRGB32F ||
      Format == WebGLImageConversion::kDataFormatRG32F ||
      Format == WebGLImageConversion::kDataFormatRA32F ||
      Format == WebGLImageConversion::kDataFormatR32F ||
      Format == WebGLImageConversion::kDataFormatRGBA16F ||
      Format == WebGLImageConversion::kDataFormatRGB16F ||
      Format == WebGLImageConversion::kDataFormatRG16F ||
      Format == WebGLImageConversion::kDataFormatRA16F ||
      Format == WebGLImageConversion::kDataFormatR16F ||
      Format == WebGLImageConversion::kDataFormatRGBA5551 ||
      Format == WebGLImageConversion::kDataFormatRGBA4444 ||
      Format == WebGLImageConversion::kDataFormatRGB565 ||
      Format == WebGLImageConversion::kDataFormatRGBA2_10_10_10;
};

template <WebGLImageConversion::DataFormat SrcFormat,
          WebGLImageConversion::DataFormat DstFormat,
          WebGLImageConversion::AlphaOp alphaOp>
void FormatConverter::Convert() {
  // Many instantiations of this template function will never be entered, so we
  // try to return immediately in these cases to avoid generating useless code.
  if (SrcFormat == DstFormat &&
      alphaOp == WebGLImageConversion::kAlphaDoNothing) {
    NOTREACHED();
    return;
  }
  // Note that ImageBitmaps with SrcFormat==kDataFormatRGBA16F return
  // false for IsFloatFormat since the input data is uint16_t.
  if (!IsFloatFormat<DstFormat>::value && IsFloatFormat<SrcFormat>::value) {
    NOTREACHED();
    return;
  }

  // Only textures uploaded from DOM elements or ImageData can allow DstFormat
  // != SrcFormat.
  const bool src_format_comes_from_dom_element_or_image_data =
      WebGLImageConversion::SrcFormatComesFromDOMElementOrImageData(SrcFormat);
  if (!src_format_comes_from_dom_element_or_image_data &&
      SrcFormat != DstFormat) {
    NOTREACHED();
    return;
  }
  // Likewise, only textures uploaded from DOM elements or ImageData can
  // possibly need to be unpremultiplied.
  if (!src_format_comes_from_dom_element_or_image_data &&
      alphaOp == WebGLImageConversion::kAlphaDoUnmultiply) {
    NOTREACHED();
    return;
  }
  if (src_format_comes_from_dom_element_or_image_data &&
      alphaOp == WebGLImageConversion::kAlphaDoUnmultiply &&
      !SupportsConversionFromDomElements<DstFormat>::value) {
    NOTREACHED();
    return;
  }
  if ((!HasAlpha(SrcFormat) || !HasColor(SrcFormat) || !HasColor(DstFormat)) &&
      alphaOp != WebGLImageConversion::kAlphaDoNothing) {
    NOTREACHED();
    return;
  }
  // If converting DOM element data to UNSIGNED_INT_5_9_9_9_REV or
  // UNSIGNED_INT_10F_11F_11F_REV, we should always switch to FLOAT instead to
  // avoid unpacking/packing these two types.
  if (src_format_comes_from_dom_element_or_image_data &&
      SrcFormat != DstFormat &&
      (DstFormat == WebGLImageConversion::kDataFormatRGB5999 ||
       DstFormat == WebGLImageConversion::kDataFormatRGB10F11F11F)) {
    NOTREACHED();
    return;
  }

  typedef typename DataTypeForFormat<SrcFormat>::Type SrcType;
  typedef typename DataTypeForFormat<DstFormat>::Type DstType;
  const int kIntermFormat = IntermediateFormat<DstFormat>::value;
  typedef typename DataTypeForFormat<kIntermFormat>::Type IntermType;
  // stride here could be negative.
  const ptrdiff_t src_stride_in_elements =
      src_stride_ / static_cast<int>(sizeof(SrcType));
  const ptrdiff_t dst_stride_in_elements =
      dst_stride_ / static_cast<int>(sizeof(DstType));
  const bool kTrivialUnpack = SrcFormat == kIntermFormat;
  const bool kTrivialPack = DstFormat == kIntermFormat &&
                            alphaOp == WebGLImageConversion::kAlphaDoNothing;
  DCHECK(!kTrivialUnpack || !kTrivialPack);

  const SrcType* src_row_start =
      static_cast<const SrcType*>(static_cast<const void*>(
          static_cast<const uint8_t*>(src_start_) +
          ((src_stride_ * src_sub_rectangle_.Y()) + src_row_offset_)));

  // If packing multiple images into a 3D texture, and flipY is true,
  // then the sub-rectangle is pointing at the start of the
  // "bottommost" of those images. Since the source pointer strides in
  // the positive direction, we need to back it up to point at the
  // last, or "topmost", of these images.
  if (dst_stride_ < 0 && depth_ > 1) {
    src_row_start -=
        (depth_ - 1) * src_stride_in_elements * unpack_image_height_;
  }

  DstType* dst_row_start = static_cast<DstType*>(dst_start_);
  if (kTrivialUnpack) {
    for (int d = 0; d < depth_; ++d) {
      for (int i = 0; i < src_sub_rectangle_.Height(); ++i) {
        Pack<DstFormat, alphaOp>(src_row_start, dst_row_start,
                                 src_sub_rectangle_.Width());
        src_row_start += src_stride_in_elements;
        dst_row_start += dst_stride_in_elements;
      }
      src_row_start += src_stride_in_elements *
                       (unpack_image_height_ - src_sub_rectangle_.Height());
    }
  } else if (kTrivialPack) {
    for (int d = 0; d < depth_; ++d) {
      for (int i = 0; i < src_sub_rectangle_.Height(); ++i) {
        Unpack<SrcFormat>(src_row_start, dst_row_start,
                          src_sub_rectangle_.Width());
        src_row_start += src_stride_in_elements;
        dst_row_start += dst_stride_in_elements;
      }
      src_row_start += src_stride_in_elements *
                       (unpack_image_height_ - src_sub_rectangle_.Height());
    }
  } else {
    for (int d = 0; d < depth_; ++d) {
      for (int i = 0; i < src_sub_rectangle_.Height(); ++i) {
        Unpack<SrcFormat>(src_row_start,
                          reinterpret_cast<IntermType*>(
                              unpacked_intermediate_src_data_.get()),
                          src_sub_rectangle_.Width());
        Pack<DstFormat, alphaOp>(reinterpret_cast<IntermType*>(
                                     unpacked_intermediate_src_data_.get()),
                                 dst_row_start, src_sub_rectangle_.Width());
        src_row_start += src_stride_in_elements;
        dst_row_start += dst_stride_in_elements;
      }
      src_row_start += src_stride_in_elements *
                       (unpack_image_height_ - src_sub_rectangle_.Height());
    }
  }
  success_ = true;
  return;
}

bool FrameIsValid(const SkBitmap& frame_bitmap) {
  return !frame_bitmap.isNull() && !frame_bitmap.empty() &&
         frame_bitmap.colorType() == kN32_SkColorType;
}

}  // anonymous namespace

WebGLImageConversion::PixelStoreParams::PixelStoreParams()
    : alignment(4),
      row_length(0),
      image_height(0),
      skip_pixels(0),
      skip_rows(0),
      skip_images(0) {}

bool WebGLImageConversion::ComputeFormatAndTypeParameters(
    GLenum format,
    GLenum type,
    unsigned* components_per_pixel,
    unsigned* bytes_per_component) {
  switch (format) {
    case GL_ALPHA:
    case GL_LUMINANCE:
    case GL_RED:
    case GL_RED_INTEGER:
    case GL_DEPTH_COMPONENT:
    case GL_DEPTH_STENCIL:  // Treat it as one component.
      *components_per_pixel = 1;
      break;
    case GL_LUMINANCE_ALPHA:
    case GL_RG:
    case GL_RG_INTEGER:
      *components_per_pixel = 2;
      break;
    case GL_RGB:
    case GL_RGB_INTEGER:
    case GL_SRGB_EXT:  // GL_EXT_sRGB
      *components_per_pixel = 3;
      break;
    case GL_RGBA:
    case GL_RGBA_INTEGER:
    case GL_BGRA_EXT:        // GL_EXT_texture_format_BGRA8888
    case GL_SRGB_ALPHA_EXT:  // GL_EXT_sRGB
      *components_per_pixel = 4;
      break;
    default:
      return false;
  }
  switch (type) {
    case GL_BYTE:
      *bytes_per_component = sizeof(GLbyte);
      break;
    case GL_UNSIGNED_BYTE:
      *bytes_per_component = sizeof(GLubyte);
      break;
    case GL_SHORT:
      *bytes_per_component = sizeof(GLshort);
      break;
    case GL_UNSIGNED_SHORT:
      *bytes_per_component = sizeof(GLushort);
      break;
    case GL_UNSIGNED_SHORT_5_6_5:
    case GL_UNSIGNED_SHORT_4_4_4_4:
    case GL_UNSIGNED_SHORT_5_5_5_1:
      *components_per_pixel = 1;
      *bytes_per_component = sizeof(GLushort);
      break;
    case GL_INT:
      *bytes_per_component = sizeof(GLint);
      break;
    case GL_UNSIGNED_INT:
      *bytes_per_component = sizeof(GLuint);
      break;
    case GL_UNSIGNED_INT_24_8_OES:
    case GL_UNSIGNED_INT_10F_11F_11F_REV:
    case GL_UNSIGNED_INT_5_9_9_9_REV:
    case GL_UNSIGNED_INT_2_10_10_10_REV:
      *components_per_pixel = 1;
      *bytes_per_component = sizeof(GLuint);
      break;
    case GL_FLOAT:  // OES_texture_float
      *bytes_per_component = sizeof(GLfloat);
      break;
    case GL_HALF_FLOAT:
    case GL_HALF_FLOAT_OES:  // OES_texture_half_float
      *bytes_per_component = sizeof(GLushort);
      break;
    default:
      return false;
  }
  return true;
}

GLenum WebGLImageConversion::ComputeImageSizeInBytes(
    GLenum format,
    GLenum type,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    const PixelStoreParams& params,
    unsigned* image_size_in_bytes,
    unsigned* padding_in_bytes,
    unsigned* skip_size_in_bytes) {
  DCHECK(image_size_in_bytes);
  DCHECK(params.alignment == 1 || params.alignment == 2 ||
         params.alignment == 4 || params.alignment == 8);
  DCHECK_GE(params.row_length, 0);
  DCHECK_GE(params.image_height, 0);
  DCHECK_GE(params.skip_pixels, 0);
  DCHECK_GE(params.skip_rows, 0);
  DCHECK_GE(params.skip_images, 0);
  if (width < 0 || height < 0 || depth < 0)
    return GL_INVALID_VALUE;
  if (!width || !height || !depth) {
    *image_size_in_bytes = 0;
    if (padding_in_bytes)
      *padding_in_bytes = 0;
    if (skip_size_in_bytes)
      *skip_size_in_bytes = 0;
    return GL_NO_ERROR;
  }

  int row_length = params.row_length > 0 ? params.row_length : width;
  int image_height = params.image_height > 0 ? params.image_height : height;

  unsigned bytes_per_component, components_per_pixel;
  if (!ComputeFormatAndTypeParameters(format, type, &bytes_per_component,
                                      &components_per_pixel))
    return GL_INVALID_ENUM;
  unsigned bytes_per_group = bytes_per_component * components_per_pixel;
  base::CheckedNumeric<uint32_t> checked_value =
      static_cast<uint32_t>(row_length);
  checked_value *= bytes_per_group;
  if (!checked_value.IsValid())
    return GL_INVALID_VALUE;

  unsigned last_row_size;
  if (params.row_length > 0 && params.row_length != width) {
    base::CheckedNumeric<uint32_t> tmp = width;
    tmp *= bytes_per_group;
    if (!tmp.IsValid())
      return GL_INVALID_VALUE;
    last_row_size = tmp.ValueOrDie();
  } else {
    last_row_size = checked_value.ValueOrDie();
  }

  unsigned padding = 0;
  base::CheckedNumeric<uint32_t> checked_residual = checked_value;
  checked_residual %= static_cast<uint32_t>(params.alignment);
  if (!checked_residual.IsValid()) {
    return GL_INVALID_VALUE;
  }
  unsigned residual = checked_residual.ValueOrDie();
  if (residual) {
    padding = params.alignment - residual;
    checked_value += padding;
  }
  if (!checked_value.IsValid())
    return GL_INVALID_VALUE;
  unsigned padded_row_size = checked_value.ValueOrDie();

  base::CheckedNumeric<uint32_t> rows = image_height;
  rows *= (depth - 1);
  // Last image is not affected by IMAGE_HEIGHT parameter.
  rows += height;
  if (!rows.IsValid())
    return GL_INVALID_VALUE;
  checked_value *= (rows - 1);
  // Last row is not affected by ROW_LENGTH parameter.
  checked_value += last_row_size;
  if (!checked_value.IsValid())
    return GL_INVALID_VALUE;
  *image_size_in_bytes = checked_value.ValueOrDie();
  if (padding_in_bytes)
    *padding_in_bytes = padding;

  base::CheckedNumeric<uint32_t> skip_size = 0;
  if (params.skip_images > 0) {
    base::CheckedNumeric<uint32_t> tmp = padded_row_size;
    tmp *= image_height;
    tmp *= params.skip_images;
    if (!tmp.IsValid())
      return GL_INVALID_VALUE;
    skip_size += tmp.ValueOrDie();
  }
  if (params.skip_rows > 0) {
    base::CheckedNumeric<uint32_t> tmp = padded_row_size;
    tmp *= params.skip_rows;
    if (!tmp.IsValid())
      return GL_INVALID_VALUE;
    skip_size += tmp.ValueOrDie();
  }
  if (params.skip_pixels > 0) {
    base::CheckedNumeric<uint32_t> tmp = bytes_per_group;
    tmp *= params.skip_pixels;
    if (!tmp.IsValid())
      return GL_INVALID_VALUE;
    skip_size += tmp.ValueOrDie();
  }
  if (!skip_size.IsValid())
    return GL_INVALID_VALUE;
  if (skip_size_in_bytes)
    *skip_size_in_bytes = skip_size.ValueOrDie();

  checked_value += skip_size.ValueOrDie();
  if (!checked_value.IsValid())
    return GL_INVALID_VALUE;
  return GL_NO_ERROR;
}

WebGLImageConversion::ImageExtractor::ImageExtractor(
    Image* image,
    ImageHtmlDomSource image_html_dom_source,
    bool premultiply_alpha,
    bool ignore_color_space) {
  image_ = image;
  image_html_dom_source_ = image_html_dom_source;
  ExtractImage(premultiply_alpha, ignore_color_space);
}

void WebGLImageConversion::ImageExtractor::ExtractImage(
    bool premultiply_alpha,
    bool ignore_color_space) {
  DCHECK(!image_pixel_locker_);

  if (!image_)
    return;

  sk_sp<SkImage> skia_image = image_->PaintImageForCurrentFrame().GetSkImage();
  SkImageInfo info =
      skia_image ? SkImageInfo::MakeN32Premul(image_->width(), image_->height())
                 : SkImageInfo::MakeUnknown();
  alpha_op_ = kAlphaDoNothing;
  bool has_alpha = skia_image ? !skia_image->isOpaque() : true;

  bool need_unpremultiplied = has_alpha && !premultiply_alpha;
  bool need_color_conversion = !ignore_color_space && skia_image &&
                               skia_image->colorSpace() &&
                               !skia_image->colorSpace()->isSRGB();
  if ((!skia_image || ignore_color_space || need_unpremultiplied ||
       need_color_conversion) &&
      image_->Data()) {
    // Attempt to get raw unpremultiplied image data.
    const bool data_complete = true;
    std::unique_ptr<ImageDecoder> decoder(ImageDecoder::Create(
        image_->Data(), data_complete, ImageDecoder::kAlphaNotPremultiplied,
        ImageDecoder::kDefaultBitDepth,
        ignore_color_space ? ColorBehavior::Ignore()
                           : ColorBehavior::TransformToSRGB(),
        ImageDecoder::OverrideAllowDecodeToYuv::kDeny));
    if (!decoder || !decoder->FrameCount())
      return;
    ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
    if (!frame || frame->GetStatus() != ImageFrame::kFrameComplete)
      return;
    has_alpha = frame->HasAlpha();
    SkBitmap bitmap = frame->Bitmap();
    if (!FrameIsValid(bitmap))
      return;

    // TODO(fmalita): Partial frames are not supported currently: only fully
    // decoded frames make it through.  We could potentially relax this and
    // use SkImage::MakeFromBitmap(bitmap) to make a copy.
    skia_image = frame->FinalizePixelsAndGetImage();
    info = bitmap.info();

    if (has_alpha && premultiply_alpha)
      alpha_op_ = kAlphaDoPremultiply;
  } else if (!premultiply_alpha && has_alpha) {
    // 1. For texImage2D with HTMLVideoElment input, assume no PremultiplyAlpha
    //    had been applied and the alpha value for each pixel is 0xFF.  This is
    //    true at present; if it is changed in the future it will need
    //    adjustment accordingly.
    // 2. For texImage2D with HTMLCanvasElement input in which alpha is already
    //    premultiplied in this port, do AlphaDoUnmultiply if
    //    UNPACK_PREMULTIPLY_ALPHA_WEBGL is set to false.
    if (image_html_dom_source_ != kHtmlDomVideo)
      alpha_op_ = kAlphaDoUnmultiply;
  }

  if (!skia_image)
    return;

#if SK_B32_SHIFT
  image_source_format_ = kDataFormatRGBA8;
#else
  image_source_format_ = kDataFormatBGRA8;
#endif
  image_source_unpack_alignment_ =
      0;  // FIXME: this seems to always be zero - why use at all?

  DCHECK(skia_image->width());
  DCHECK(skia_image->height());
  image_width_ = skia_image->width();
  image_height_ = skia_image->height();

  // Fail if the image was downsampled because of memory limits.
  if (image_width_ != (unsigned)image_->width() ||
      image_height_ != (unsigned)image_->height())
    return;

  image_pixel_locker_.emplace(std::move(skia_image), info.alphaType(),
                              kN32_SkColorType);
}

unsigned WebGLImageConversion::GetChannelBitsByFormat(GLenum format) {
  switch (format) {
    case GL_ALPHA:
      return kChannelAlpha;
    case GL_RED:
    case GL_RED_INTEGER:
    case GL_R8:
    case GL_R8_SNORM:
    case GL_R8UI:
    case GL_R8I:
    case GL_R16UI:
    case GL_R16I:
    case GL_R32UI:
    case GL_R32I:
    case GL_R16F:
    case GL_R32F:
      return kChannelRed;
    case GL_RG:
    case GL_RG_INTEGER:
    case GL_RG8:
    case GL_RG8_SNORM:
    case GL_RG8UI:
    case GL_RG8I:
    case GL_RG16UI:
    case GL_RG16I:
    case GL_RG32UI:
    case GL_RG32I:
    case GL_RG16F:
    case GL_RG32F:
      return kChannelRG;
    case GL_LUMINANCE:
      return kChannelRGB;
    case GL_LUMINANCE_ALPHA:
      return kChannelRGBA;
    case GL_RGB:
    case GL_RGB_INTEGER:
    case GL_RGB8:
    case GL_RGB8_SNORM:
    case GL_RGB8UI:
    case GL_RGB8I:
    case GL_RGB16UI:
    case GL_RGB16I:
    case GL_RGB32UI:
    case GL_RGB32I:
    case GL_RGB16F:
    case GL_RGB32F:
    case GL_RGB565:
    case GL_R11F_G11F_B10F:
    case GL_RGB9_E5:
    case GL_SRGB_EXT:
    case GL_SRGB8:
      return kChannelRGB;
    case GL_RGBA:
    case GL_RGBA_INTEGER:
    case GL_RGBA8:
    case GL_RGBA8_SNORM:
    case GL_RGBA8UI:
    case GL_RGBA8I:
    case GL_RGBA16UI:
    case GL_RGBA16I:
    case GL_RGBA32UI:
    case GL_RGBA32I:
    case GL_RGBA16F:
    case GL_RGBA32F:
    case GL_RGBA4:
    case GL_RGB5_A1:
    case GL_RGB10_A2:
    case GL_RGB10_A2UI:
    case GL_SRGB_ALPHA_EXT:
    case GL_SRGB8_ALPHA8:
      return kChannelRGBA;
    case GL_DEPTH_COMPONENT:
    case GL_DEPTH_COMPONENT16:
    case GL_DEPTH_COMPONENT24:
    case GL_DEPTH_COMPONENT32F:
      return kChannelDepth;
    case GL_STENCIL:
    case GL_STENCIL_INDEX8:
      return kChannelStencil;
    case GL_DEPTH_STENCIL:
    case GL_DEPTH24_STENCIL8:
    case GL_DEPTH32F_STENCIL8:
      return kChannelDepthStencil;
    default:
      return 0;
  }
}

bool WebGLImageConversion::PackImageData(
    Image* image,
    const void* pixels,
    GLenum format,
    GLenum type,
    bool flip_y,
    AlphaOp alpha_op,
    DataFormat source_format,
    unsigned source_image_width,
    unsigned source_image_height,
    const IntRect& source_image_sub_rectangle,
    int depth,
    unsigned source_unpack_alignment,
    int unpack_image_height,
    Vector<uint8_t>& data) {
  if (!pixels)
    return false;

  unsigned packed_size;
  // Output data is tightly packed (alignment == 1).
  PixelStoreParams params;
  params.alignment = 1;
  if (ComputeImageSizeInBytes(format, type, source_image_sub_rectangle.Width(),
                              source_image_sub_rectangle.Height(), depth,
                              params, &packed_size, nullptr,
                              nullptr) != GL_NO_ERROR)
    return false;
  data.resize(packed_size);

  return PackPixels(reinterpret_cast<const uint8_t*>(pixels), source_format,
                    source_image_width, source_image_height,
                    source_image_sub_rectangle, depth, source_unpack_alignment,
                    unpack_image_height, format, type, alpha_op, data.data(),
                    flip_y);
}

bool WebGLImageConversion::ExtractImageData(
    const uint8_t* image_data,
    DataFormat source_data_format,
    const IntSize& image_data_size,
    const IntRect& source_image_sub_rectangle,
    int depth,
    int unpack_image_height,
    GLenum format,
    GLenum type,
    bool flip_y,
    bool premultiply_alpha,
    Vector<uint8_t>& data) {
  if (!image_data)
    return false;
  int width = image_data_size.Width();
  int height = image_data_size.Height();

  unsigned packed_size;
  // Output data is tightly packed (alignment == 1).
  PixelStoreParams params;
  params.alignment = 1;
  if (ComputeImageSizeInBytes(format, type, source_image_sub_rectangle.Width(),
                              source_image_sub_rectangle.Height(), depth,
                              params, &packed_size, nullptr,
                              nullptr) != GL_NO_ERROR)
    return false;
  data.resize(packed_size);

  if (!PackPixels(image_data, source_data_format, width, height,
                  source_image_sub_rectangle, depth, 0, unpack_image_height,
                  format, type,
                  premultiply_alpha ? kAlphaDoPremultiply : kAlphaDoNothing,
                  data.data(), flip_y))
    return false;

  return true;
}

bool WebGLImageConversion::ExtractTextureData(
    unsigned width,
    unsigned height,
    GLenum format,
    GLenum type,
    const PixelStoreParams& unpack_params,
    bool flip_y,
    bool premultiply_alpha,
    const void* pixels,
    Vector<uint8_t>& data) {
  // Assumes format, type, etc. have already been validated.
  DataFormat source_data_format = GetDataFormat(format, type);
  if (source_data_format == kDataFormatNumFormats)
    return false;

  // Resize the output buffer.
  unsigned int components_per_pixel, bytes_per_component;
  if (!ComputeFormatAndTypeParameters(format, type, &components_per_pixel,
                                      &bytes_per_component))
    return false;
  unsigned bytes_per_pixel = components_per_pixel * bytes_per_component;
  data.resize(width * height * bytes_per_pixel);

  unsigned image_size_in_bytes, skip_size_in_bytes;
  ComputeImageSizeInBytes(format, type, width, height, 1, unpack_params,
                          &image_size_in_bytes, nullptr, &skip_size_in_bytes);
  const uint8_t* src_data = static_cast<const uint8_t*>(pixels);
  if (skip_size_in_bytes) {
    src_data += skip_size_in_bytes;
  }

  if (!PackPixels(src_data, source_data_format,
                  unpack_params.row_length ? unpack_params.row_length : width,
                  height, IntRect(0, 0, width, height), 1,
                  unpack_params.alignment, 0, format, type,
                  (premultiply_alpha ? kAlphaDoPremultiply : kAlphaDoNothing),
                  data.data(), flip_y))
    return false;

  return true;
}

bool WebGLImageConversion::PackPixels(const uint8_t* source_data,
                                      DataFormat source_data_format,
                                      unsigned source_data_width,
                                      unsigned source_data_height,
                                      const IntRect& source_data_sub_rectangle,
                                      int depth,
                                      unsigned source_unpack_alignment,
                                      int unpack_image_height,
                                      unsigned destination_format,
                                      unsigned destination_type,
                                      AlphaOp alpha_op,
                                      void* destination_data,
                                      bool flip_y) {
  DCHECK_GE(depth, 1);
  if (unpack_image_height == 0) {
    unpack_image_height = source_data_sub_rectangle.Height();
  }
  int valid_src = source_data_width * TexelBytesForFormat(source_data_format);
  int remainder =
      source_unpack_alignment ? (valid_src % source_unpack_alignment) : 0;
  int src_stride =
      remainder ? (valid_src + source_unpack_alignment - remainder) : valid_src;
  int src_row_offset =
      source_data_sub_rectangle.X() * TexelBytesForFormat(source_data_format);

  DataFormat dst_data_format =
      GetDataFormat(destination_format, destination_type);
  if (dst_data_format == kDataFormatNumFormats)
    return false;
  int dst_stride =
      source_data_sub_rectangle.Width() * TexelBytesForFormat(dst_data_format);
  if (flip_y) {
    destination_data =
        static_cast<uint8_t*>(destination_data) +
        dst_stride * ((depth * source_data_sub_rectangle.Height()) - 1);
    dst_stride = -dst_stride;
  }
  if (!HasAlpha(source_data_format) || !HasColor(source_data_format) ||
      !HasColor(dst_data_format))
    alpha_op = kAlphaDoNothing;

  if (source_data_format == dst_data_format && alpha_op == kAlphaDoNothing) {
    const uint8_t* base_ptr =
        source_data + src_stride * source_data_sub_rectangle.Y();
    const uint8_t* base_end =
        source_data + src_stride * source_data_sub_rectangle.MaxY();

    // If packing multiple images into a 3D texture, and flipY is true,
    // then the sub-rectangle is pointing at the start of the
    // "bottommost" of those images. Since the source pointer strides in
    // the positive direction, we need to back it up to point at the
    // last, or "topmost", of these images.
    if (flip_y && depth > 1) {
      const ptrdiff_t distance_to_top_image =
          (depth - 1) * src_stride * unpack_image_height;
      base_ptr -= distance_to_top_image;
      base_end -= distance_to_top_image;
    }

    unsigned row_size = (dst_stride > 0) ? dst_stride : -dst_stride;
    uint8_t* dst = static_cast<uint8_t*>(destination_data);

    for (int i = 0; i < depth; ++i) {
      const uint8_t* ptr = base_ptr;
      const uint8_t* ptr_end = base_end;
      while (ptr < ptr_end) {
        memcpy(dst, ptr + src_row_offset, row_size);
        ptr += src_stride;
        dst += dst_stride;
      }
      base_ptr += unpack_image_height * src_stride;
      base_end += unpack_image_height * src_stride;
    }
    return true;
  }

  FormatConverter converter(source_data_sub_rectangle, depth,
                            unpack_image_height, source_data, destination_data,
                            src_stride, src_row_offset, dst_stride);
  converter.Convert(source_data_format, dst_data_format, alpha_op);
  if (!converter.Success())
    return false;
  return true;
}

void WebGLImageConversion::UnpackPixels(const uint16_t* source_data,
                                        DataFormat source_data_format,
                                        unsigned pixels_per_row,
                                        uint8_t* destination_data) {
  switch (source_data_format) {
    case kDataFormatRGBA4444: {
      typedef typename DataTypeForFormat<
          WebGLImageConversion::kDataFormatRGBA4444>::Type SrcType;
      const SrcType* src_row_start = static_cast<const SrcType*>(source_data);
      Unpack<WebGLImageConversion::kDataFormatRGBA4444>(
          src_row_start, destination_data, pixels_per_row);
    } break;
    case kDataFormatRGBA5551: {
      typedef typename DataTypeForFormat<
          WebGLImageConversion::kDataFormatRGBA5551>::Type SrcType;
      const SrcType* src_row_start = static_cast<const SrcType*>(source_data);
      Unpack<WebGLImageConversion::kDataFormatRGBA5551>(
          src_row_start, destination_data, pixels_per_row);
    } break;
    case kDataFormatBGRA8: {
      const uint8_t* psrc = (const uint8_t*)source_data;
      typedef typename DataTypeForFormat<
          WebGLImageConversion::kDataFormatBGRA8>::Type SrcType;
      const SrcType* src_row_start = static_cast<const SrcType*>(psrc);
      Unpack<WebGLImageConversion::kDataFormatBGRA8>(
          src_row_start, destination_data, pixels_per_row);
    } break;
    default:
      break;
  }
}

void WebGLImageConversion::PackPixels(const uint8_t* source_data,
                                      DataFormat source_data_format,
                                      unsigned pixels_per_row,
                                      uint8_t* destination_data) {
  switch (source_data_format) {
    case kDataFormatRA8: {
      typedef typename DataTypeForFormat<
          WebGLImageConversion::kDataFormatRGBA8>::Type SrcType;
      const SrcType* src_row_start = static_cast<const SrcType*>(source_data);
      Pack<WebGLImageConversion::kDataFormatRA8,
           WebGLImageConversion::kAlphaDoUnmultiply>(
          src_row_start, destination_data, pixels_per_row);
    } break;
    case kDataFormatR8: {
      typedef typename DataTypeForFormat<
          WebGLImageConversion::kDataFormatRGBA8>::Type SrcType;
      const SrcType* src_row_start = static_cast<const SrcType*>(source_data);
      Pack<WebGLImageConversion::kDataFormatR8,
           WebGLImageConversion::kAlphaDoUnmultiply>(
          src_row_start, destination_data, pixels_per_row);
    } break;
    case kDataFormatRGBA8: {
      typedef typename DataTypeForFormat<
          WebGLImageConversion::kDataFormatRGBA8>::Type SrcType;
      const SrcType* src_row_start = static_cast<const SrcType*>(source_data);
      Pack<WebGLImageConversion::kDataFormatRGBA8,
           WebGLImageConversion::kAlphaDoUnmultiply>(
          src_row_start, destination_data, pixels_per_row);
    } break;
    case kDataFormatRGBA4444: {
      uint16_t* pdst = (uint16_t*)destination_data;
      typedef typename DataTypeForFormat<
          WebGLImageConversion::kDataFormatRGBA8>::Type SrcType;
      const SrcType* src_row_start = static_cast<const SrcType*>(source_data);
      typedef typename DataTypeForFormat<
          WebGLImageConversion::kDataFormatRGBA4444>::Type DstType;
      DstType* dst_row_start = static_cast<DstType*>(pdst);
      Pack<WebGLImageConversion::kDataFormatRGBA4444,
           WebGLImageConversion::kAlphaDoNothing>(src_row_start, dst_row_start,
                                                  pixels_per_row);
    } break;
    case kDataFormatRGBA5551: {
      uint16_t* pdst = (uint16_t*)destination_data;
      typedef typename DataTypeForFormat<
          WebGLImageConversion::kDataFormatRGBA8>::Type SrcType;
      const SrcType* src_row_start = static_cast<const SrcType*>(source_data);
      typedef typename DataTypeForFormat<
          WebGLImageConversion::kDataFormatRGBA5551>::Type DstType;
      DstType* dst_row_start = static_cast<DstType*>(pdst);
      Pack<WebGLImageConversion::kDataFormatRGBA5551,
           WebGLImageConversion::kAlphaDoNothing>(src_row_start, dst_row_start,
                                                  pixels_per_row);
    } break;
    case kDataFormatRGB565: {
      uint16_t* pdst = (uint16_t*)destination_data;
      typedef typename DataTypeForFormat<
          WebGLImageConversion::kDataFormatRGBA8>::Type SrcType;
      const SrcType* src_row_start = static_cast<const SrcType*>(source_data);
      typedef typename DataTypeForFormat<
          WebGLImageConversion::kDataFormatRGB565>::Type DstType;
      DstType* dst_row_start = static_cast<DstType*>(pdst);
      Pack<WebGLImageConversion::kDataFormatRGB565,
           WebGLImageConversion::kAlphaDoNothing>(src_row_start, dst_row_start,
                                                  pixels_per_row);
    } break;
    default:
      break;
  }
}

}  // namespace blink
