/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef SVGFEBlendElement_h
#define SVGFEBlendElement_h

#include "core/svg/SVGAnimatedEnumeration.h"
#include "core/svg/SVGFilterPrimitiveStandardAttributes.h"
#include "platform/heap/Handle.h"

namespace blink {

class SVGFEBlendElement final : public SVGFilterPrimitiveStandardAttributes {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum Mode {
    kModeUnknown = 0,
    kModeNormal = 1,
    kModeMultiply = 2,
    kModeScreen = 3,
    kModeDarken = 4,
    kModeLighten = 5,

    // The following modes do not map to IDL constants on
    // SVGFEBlendElement.
    kModeOverlay,
    kModeColorDodge,
    kModeColorBurn,
    kModeHardLight,
    kModeSoftLight,
    kModeDifference,
    kModeExclusion,
    kModeHue,
    kModeSaturation,
    kModeColor,
    kModeLuminosity,
  };

  DECLARE_NODE_FACTORY(SVGFEBlendElement);
  SVGAnimatedString* in1() { return in1_.Get(); }
  SVGAnimatedString* in2() { return in2_.Get(); }
  SVGAnimatedEnumeration<Mode>* mode() { return mode_.Get(); }

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit SVGFEBlendElement(Document&);

  bool SetFilterEffectAttribute(FilterEffect*,
                                const QualifiedName& attr_name) override;
  void SvgAttributeChanged(const QualifiedName&) override;
  FilterEffect* Build(SVGFilterBuilder*, Filter*) override;

  Member<SVGAnimatedString> in1_;
  Member<SVGAnimatedString> in2_;
  Member<SVGAnimatedEnumeration<Mode>> mode_;
};

template <>
const SVGEnumerationStringEntries&
GetStaticStringEntries<SVGFEBlendElement::Mode>();
template <>
unsigned short GetMaxExposedEnumValue<SVGFEBlendElement::Mode>();

}  // namespace blink

#endif  // SVGFEBlendElement_h
