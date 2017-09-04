// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIAlignOrJustifyContent.h"

#include "core/css/properties/CSSPropertyAlignmentUtils.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

const CSSValue* CSSPropertyAPIAlignOrJustifyContent::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    CSSPropertyID) {
  DCHECK(RuntimeEnabledFeatures::cssGridLayoutEnabled());
  return CSSPropertyAlignmentUtils::ConsumeContentDistributionOverflowPosition(
      range);
}

}  // namespace blink
