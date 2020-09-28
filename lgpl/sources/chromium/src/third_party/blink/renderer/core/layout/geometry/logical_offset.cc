// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/geometry/logical_offset.h"

#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_size.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

PhysicalOffset LogicalOffset::ConvertToPhysical(
    WritingDirectionMode writing_direction,
    PhysicalSize outer_size,
    PhysicalSize inner_size) const {
  return WritingModeConverter(writing_direction, outer_size)
      .ToPhysical(*this, inner_size);
}

PhysicalOffset LogicalOffset::ConvertToPhysical(WritingMode writing_mode,
                                                TextDirection direction,
                                                PhysicalSize outer_size,
                                                PhysicalSize inner_size) const {
  return ConvertToPhysical({writing_mode, direction}, outer_size, inner_size);
}

String LogicalOffset::ToString() const {
  return String::Format("%d,%d", inline_offset.ToInt(), block_offset.ToInt());
}

std::ostream& operator<<(std::ostream& os, const LogicalOffset& value) {
  return os << value.ToString();
}

}  // namespace blink
