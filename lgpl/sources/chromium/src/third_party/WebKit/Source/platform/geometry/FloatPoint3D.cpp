/*
    Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
                  2005 Eric Seidel <eric@webkit.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "platform/geometry/FloatPoint3D.h"

#include <math.h>
#include "platform/wtf/MathExtras.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

void FloatPoint3D::Normalize() {
  float temp_length = length();

  if (temp_length) {
    x_ /= temp_length;
    y_ /= temp_length;
    z_ /= temp_length;
  }
}

float FloatPoint3D::AngleBetween(const FloatPoint3D& y) const {
  float x_length = this->length();
  float y_length = y.length();

  if (x_length && y_length) {
    float cos_angle = this->Dot(y) / (x_length * y_length);
    // Due to round-off |cosAngle| can have a magnitude greater than 1.  Clamp
    // the value to [-1, 1] before computing the angle.
    return acos(clampTo(cos_angle, -1.0, 1.0));
  }
  return 0;
}

String FloatPoint3D::ToString() const {
  return String::Format("%lg,%lg,%lg", X(), Y(), Z());
}

}  // namespace blink
