/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_CACHE_KEY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_CACHE_KEY_H_

#include <limits>

#include "build/build_config.h"
#include "third_party/blink/renderer/platform/fonts/font_face_creation_params.h"
#include "third_party/blink/renderer/platform/fonts/font_palette.h"
#include "third_party/blink/renderer/platform/fonts/opentype/font_settings.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_table_deleted_value_type.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"

namespace blink {

// Multiplying the floating point size by 100 gives two decimal point
// precision which should be sufficient.
static constexpr unsigned kFontSizePrecisionMultiplier = 100;

struct FontCacheKey {
  DISALLOW_NEW();

 public:
  FontCacheKey() = default;
  FontCacheKey(FontFaceCreationParams creation_params,
               float font_size,
               unsigned options,
               float device_scale_factor,
               scoped_refptr<FontVariationSettings> variation_settings,
               scoped_refptr<FontPalette> palette,
               bool is_unique_match)
      : creation_params_(creation_params),
        font_size_(font_size * kFontSizePrecisionMultiplier),
        options_(options),
        device_scale_factor_(device_scale_factor),
        variation_settings_(std::move(variation_settings)),
        palette_(palette),
        is_unique_match_(is_unique_match) {}

  FontCacheKey(WTF::HashTableDeletedValueType)
      : font_size_(std::numeric_limits<unsigned>::max()),
        device_scale_factor_(std::numeric_limits<float>::max()) {}

  bool IsHashTableDeletedValue() const {
    return font_size_ == std::numeric_limits<unsigned>::max() &&
           device_scale_factor_ == std::numeric_limits<float>::max();
  }

  unsigned GetHash() const {
    // Convert from float with 3 digit precision before hashing.
    unsigned device_scale_factor_hash = device_scale_factor_ * 1000;
    unsigned hash_codes[7] = {
      creation_params_.GetHash(),
      font_size_,
      options_,
      device_scale_factor_hash,
#if BUILDFLAG(IS_ANDROID)
      (locale_.IsEmpty() ? 0 : AtomicStringHash::GetHash(locale_)) ^
#endif  // BUILDFLAG(IS_ANDROID)
          (variation_settings_ ? variation_settings_->GetHash() : 0),
      palette_ ? palette_->GetHash() : 0,
      is_unique_match_
    };
    return StringHasher::HashMemory<sizeof(hash_codes)>(hash_codes);
  }

  bool operator==(const FontCacheKey& other) const {
    bool variation_settings_equal =
        (!variation_settings_ && !other.variation_settings_) ||
        (variation_settings_ && other.variation_settings_ &&
         *variation_settings_ == *other.variation_settings_);
    bool palette_equal =
        (!palette_ && !other.palette_) ||
        (palette_ && other.palette_ && *palette_ == *other.palette_);
    return creation_params_ == other.creation_params_ &&
           font_size_ == other.font_size_ && options_ == other.options_ &&
           device_scale_factor_ == other.device_scale_factor_ &&
#if BUILDFLAG(IS_ANDROID)
           locale_ == other.locale_ &&
#endif  // BUILDFLAG(IS_ANDROID)
           variation_settings_equal && palette_equal &&
           is_unique_match_ == other.is_unique_match_;
  }

  bool operator!=(const FontCacheKey& other) const { return !(*this == other); }

  static constexpr unsigned PrecisionMultiplier() {
    return kFontSizePrecisionMultiplier;
  }

  void ClearFontSize() { font_size_ = 0; }

#if BUILDFLAG(IS_ANDROID)
  // Set the locale if the font is locale-specific. This allows different
  // |FontPlatformData| instances for each locale.
  void SetLocale(const AtomicString& locale) { locale_ = locale.LowerASCII(); }
#endif  // BUILDFLAG(IS_ANDROID)

 private:
  FontFaceCreationParams creation_params_;
  unsigned font_size_ = 0;
  unsigned options_ = 0;
  // FontCacheKey is the key to retrieve FontPlatformData entries from the
  // FontCache. FontPlatformData queries the platform's font render style, which
  // is dependent on the device scale factor. That's why we need
  // device_scale_factor_ to be a part of computing the cache key.
  float device_scale_factor_ = 0;
#if BUILDFLAG(IS_ANDROID)
  AtomicString locale_;
#endif  // BUILDFLAG(IS_ANDROID)
  scoped_refptr<FontVariationSettings> variation_settings_;
  scoped_refptr<FontPalette> palette_;
  bool is_unique_match_ = false;
};

struct FontCacheKeyHash {
  STATIC_ONLY(FontCacheKeyHash);
  static unsigned GetHash(const FontCacheKey& key) { return key.GetHash(); }

  static bool Equal(const FontCacheKey& a, const FontCacheKey& b) {
    return a == b;
  }

  static const bool safe_to_compare_to_empty_or_deleted = true;
};

struct FontCacheKeyTraits : WTF::SimpleClassHashTraits<FontCacheKey> {
  STATIC_ONLY(FontCacheKeyTraits);

  // std::string's empty state need not be zero in all implementations,
  // and it is held within FontFaceCreationParams.
  static const bool kEmptyValueIsZero = false;
};

}  // namespace blink

namespace WTF {
template <>
struct DefaultHash<blink::FontCacheKey> {
  STATIC_ONLY(DefaultHash);
  typedef blink::FontCacheKeyHash Hash;
};

template <>
struct HashTraits<blink::FontCacheKey>
    : WTF::SimpleClassHashTraits<blink::FontCacheKey> {
  STATIC_ONLY(HashTraits);

  // std::string's empty state need not be zero in all implementations,
  // and it is held within FontFaceCreationParams.
  static const bool kEmptyValueIsZero = false;
};

}  // namespace WTF

template <>
struct std::hash<blink::FontCacheKey> {
  std::size_t operator()(blink::FontCacheKey const& s) const noexcept {
    return static_cast<size_t>(s.GetHash());
  }
};

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_CACHE_KEY_H_
