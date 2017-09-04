/*
 * Copyright (C) 2010 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/accessibility/AXMenuListOption.h"

#include "SkMatrix44.h"
#include "core/dom/AccessibleNode.h"
#include "core/html/HTMLSelectElement.h"
#include "modules/accessibility/AXMenuList.h"
#include "modules/accessibility/AXMenuListPopup.h"
#include "modules/accessibility/AXObjectCacheImpl.h"

namespace blink {

using namespace HTMLNames;

AXMenuListOption::AXMenuListOption(HTMLOptionElement* element,
                                   AXObjectCacheImpl& ax_object_cache)
    : AXMockObject(ax_object_cache), element_(element) {}

AXMenuListOption::~AXMenuListOption() {
  DCHECK(!element_);
}

void AXMenuListOption::Detach() {
  element_ = nullptr;
  AXMockObject::Detach();
}

FrameView* AXMenuListOption::DocumentFrameView() const {
  if (IsDetached())
    return nullptr;
  return element_->GetDocument().View();
}

AccessibilityRole AXMenuListOption::RoleValue() const {
  const AtomicString& aria_role =
      GetAOMPropertyOrARIAAttribute(AOMStringProperty::kRole);
  if (aria_role.IsEmpty())
    return kMenuListOptionRole;

  AccessibilityRole role = AriaRoleToWebCoreRole(aria_role);
  if (role)
    return role;
  return kMenuListOptionRole;
}

Element* AXMenuListOption::ActionElement() const {
  return element_;
}

AXObjectImpl* AXMenuListOption::ComputeParent() const {
  Node* node = GetNode();
  if (!node)
    return nullptr;
  HTMLSelectElement* select = toHTMLOptionElement(node)->OwnerSelectElement();
  if (!select)
    return nullptr;
  AXObjectImpl* select_ax_object = AxObjectCache().GetOrCreate(select);

  // This happens if the <select> is not rendered. Return it and move on.
  if (!select_ax_object->IsMenuList())
    return select_ax_object;

  AXMenuList* menu_list = ToAXMenuList(select_ax_object);
  if (menu_list->HasChildren()) {
    const auto& child_objects = menu_list->Children();
    if (child_objects.IsEmpty())
      return nullptr;
    DCHECK_EQ(child_objects.size(), 1UL);
    DCHECK(child_objects[0]->IsMenuListPopup());
    ToAXMenuListPopup(child_objects[0].Get())->UpdateChildrenIfNecessary();
  } else {
    menu_list->UpdateChildrenIfNecessary();
  }
  return parent_.Get();
}

bool AXMenuListOption::IsEnabled() const {
  // isDisabledFormControl() returns true if the parent <select> element is
  // disabled, which we don't want.
  return element_ && !element_->OwnElementDisabled();
}

bool AXMenuListOption::IsVisible() const {
  if (!parent_)
    return false;

  // In a single-option select with the popup collapsed, only the selected
  // item is considered visible.
  return !parent_->IsOffScreen() || IsSelected();
}

bool AXMenuListOption::IsOffScreen() const {
  // Invisible list options are considered to be offscreen.
  return !IsVisible();
}

bool AXMenuListOption::IsSelected() const {
  AXMenuListPopup* parent = static_cast<AXMenuListPopup*>(ParentObject());
  if (parent && !parent->IsOffScreen())
    return parent->ActiveDescendant() == this;
  return element_ && element_->Selected();
}

void AXMenuListOption::SetSelected(bool b) {
  if (!element_ || !CanSetSelectedAttribute())
    return;

  element_->SetSelected(b);
}

bool AXMenuListOption::CanSetFocusAttribute() const {
  return CanSetSelectedAttribute();
}

bool AXMenuListOption::CanSetSelectedAttribute() const {
  if (!isHTMLOptionElement(GetNode()))
    return false;

  if (toHTMLOptionElement(GetNode())->IsDisabledFormControl())
    return false;

  HTMLSelectElement* select_element = ParentSelectNode();
  if (!select_element || select_element->IsDisabledFormControl())
    return false;

  return IsEnabled();
}

bool AXMenuListOption::ComputeAccessibilityIsIgnored(
    IgnoredReasons* ignored_reasons) const {
  return AccessibilityIsIgnoredByDefault(ignored_reasons);
}

void AXMenuListOption::GetRelativeBounds(
    AXObjectImpl** out_container,
    FloatRect& out_bounds_in_container,
    SkMatrix44& out_container_transform) const {
  *out_container = nullptr;
  out_bounds_in_container = FloatRect();
  out_container_transform.setIdentity();

  AXObjectImpl* parent = ParentObject();
  if (!parent)
    return;
  DCHECK(parent->IsMenuListPopup());

  AXObjectImpl* grandparent = parent->ParentObject();
  if (!grandparent)
    return;
  DCHECK(grandparent->IsMenuList());
  grandparent->GetRelativeBounds(out_container, out_bounds_in_container,
                                 out_container_transform);
}

String AXMenuListOption::TextAlternative(bool recursive,
                                         bool in_aria_labelled_by_traversal,
                                         AXObjectSet& visited,
                                         AXNameFrom& name_from,
                                         AXRelatedObjectVector* related_objects,
                                         NameSources* name_sources) const {
  // If nameSources is non-null, relatedObjects is used in filling it in, so it
  // must be non-null as well.
  if (name_sources)
    DCHECK(related_objects);

  if (!GetNode())
    return String();

  bool found_text_alternative = false;
  String text_alternative = AriaTextAlternative(
      recursive, in_aria_labelled_by_traversal, visited, name_from,
      related_objects, name_sources, &found_text_alternative);
  if (found_text_alternative && !name_sources)
    return text_alternative;

  name_from = kAXNameFromContents;
  text_alternative = element_->DisplayLabel();
  if (name_sources) {
    name_sources->push_back(NameSource(found_text_alternative));
    name_sources->back().type = name_from;
    name_sources->back().text = text_alternative;
    found_text_alternative = true;
  }

  return text_alternative;
}

HTMLSelectElement* AXMenuListOption::ParentSelectNode() const {
  if (!GetNode())
    return 0;

  if (isHTMLOptionElement(GetNode()))
    return toHTMLOptionElement(GetNode())->OwnerSelectElement();

  return 0;
}

DEFINE_TRACE(AXMenuListOption) {
  visitor->Trace(element_);
  AXMockObject::Trace(visitor);
}

}  // namespace blink
