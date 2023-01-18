// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/blink_ax_tree_source.h"

#include <stddef.h>

#include <algorithm>
#include <set>

#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/accessibility/ax_selection.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_deque.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/accessibility/ax_common.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace blink {

namespace {

#if DCHECK_IS_ON()
AXObject* ParentObjectUnignored(AXObject* child) {
  if (!child || child->IsDetached())
    return nullptr;
  AXObject* parent = child->ParentObjectIncludedInTree();
  while (parent && !parent->IsDetached() &&
         !parent->AccessibilityIsIncludedInTree())
    parent = parent->ParentObjectIncludedInTree();
  return parent;
}

// Check that |parent| is the first unignored parent of |child|.
void CheckParentUnignoredOf(AXObject* parent, AXObject* child) {
  AXObject* preexisting_parent = ParentObjectUnignored(child);
  DCHECK(preexisting_parent == parent)
      << "Child thinks it has a different preexisting parent:"
      << "\nChild: " << child << "\nPassed-in parent: " << parent
      << "\nPreexisting parent: " << preexisting_parent;
}
#endif

}  // namespace

BlinkAXTreeSource::BlinkAXTreeSource(AXObjectCacheImpl& ax_object_cache)
    : ax_object_cache_(ax_object_cache) {}

BlinkAXTreeSource::~BlinkAXTreeSource() = default;

bool BlinkAXTreeSource::ShouldLoadInlineTextBoxes(const AXObject* obj) const {
#if !BUILDFLAG(IS_ANDROID)
  // If inline text boxes are enabled globally, no need to explicitly load them.
  if (ax_object_cache_->GetAXMode().has_mode(ui::AXMode::kInlineTextBoxes))
    return false;
#endif

  // On some platforms, like Android, we only load inline text boxes for
  // a subset of nodes:
  //
  // Within the subtree of a focused editable text area.
  // When specifically enabled for a subtree via |load_inline_text_boxes_ids_|.

  AXObject* focused_object = GetFocusedObject();
  AXID focus_id = -1;
  if (focused_object && !focused_object->IsDetached())
    focus_id = focused_object->AXObjectID();
  const AXObject* ancestor = obj;
  while (ancestor && !ancestor->IsDetached()) {
    AXID ancestor_id = ancestor->AXObjectID();
    if (load_inline_text_boxes_ids_.Contains(ancestor_id) ||
        (ancestor_id == focus_id && ancestor->IsEditable())) {
      return true;
    }
    ancestor = ancestor->ParentObjectIncludedInTree();
  }

  return false;
}

void BlinkAXTreeSource::SetLoadInlineTextBoxesForId(int32_t id) {
  // Keeping stale IDs in the set is harmless but we don't want it to keep
  // growing without bound, so clear out any unnecessary IDs whenever this
  // method is called.
  WTF::Vector<int32_t> to_remove;
  for (auto iter : load_inline_text_boxes_ids_) {
    auto* obj = GetFromId(iter);
    if (!obj || obj->IsDetached())
      to_remove.push_back(iter);
  }
  for (auto iter : to_remove)
    load_inline_text_boxes_ids_.erase(iter);

  load_inline_text_boxes_ids_.insert(id);
}

static ax::mojom::blink::TextAffinity ToAXAffinity(TextAffinity affinity) {
  switch (affinity) {
    case TextAffinity::kUpstream:
      return ax::mojom::blink::TextAffinity::kUpstream;
    case TextAffinity::kDownstream:
      return ax::mojom::blink::TextAffinity::kDownstream;
    default:
      NOTREACHED();
      return ax::mojom::blink::TextAffinity::kDownstream;
  }
}

void BlinkAXTreeSource::Selection(
    const AXObject* obj,
    bool& is_selection_backward,
    AXObject** anchor_object,
    int& anchor_offset,
    ax::mojom::blink::TextAffinity& anchor_affinity,
    AXObject** focus_object,
    int& focus_offset,
    ax::mojom::blink::TextAffinity& focus_affinity) const {
  is_selection_backward = false;
  *anchor_object = nullptr;
  anchor_offset = -1;
  anchor_affinity = ax::mojom::blink::TextAffinity::kDownstream;
  *focus_object = nullptr;
  focus_offset = -1;
  focus_affinity = ax::mojom::blink::TextAffinity::kDownstream;

  if (!obj || obj->IsDetached())
    return;

  AXObject* focus = GetFocusedObject();
  if (!focus || focus->IsDetached())
    return;

  const auto ax_selection =
      focus->IsAtomicTextField()
          ? AXSelection::FromCurrentSelection(ToTextControl(*focus->GetNode()))
          : AXSelection::FromCurrentSelection(*focus->GetDocument());
  if (!ax_selection)
    return;

  const AXPosition base = ax_selection.Base();
  *anchor_object = const_cast<AXObject*>(base.ContainerObject());
  const AXPosition extent = ax_selection.Extent();
  *focus_object = const_cast<AXObject*>(extent.ContainerObject());

  is_selection_backward = base > extent;
  if (base.IsTextPosition()) {
    anchor_offset = base.TextOffset();
    anchor_affinity = ToAXAffinity(base.Affinity());
  } else {
    anchor_offset = base.ChildIndex();
  }

  if (extent.IsTextPosition()) {
    focus_offset = extent.TextOffset();
    focus_affinity = ToAXAffinity(extent.Affinity());
  } else {
    focus_offset = extent.ChildIndex();
  }
}

static ui::AXTreeID GetAXTreeID(LocalFrame* local_frame) {
  const absl::optional<base::UnguessableToken>& embedding_token =
      local_frame->GetEmbeddingToken();
  if (embedding_token && !embedding_token->is_empty())
    return ui::AXTreeID::FromToken(embedding_token.value());
  return ui::AXTreeIDUnknown();
}

bool BlinkAXTreeSource::GetTreeData(ui::AXTreeData* tree_data) const {
  CHECK(frozen_);
  AXObject* root = GetRoot();
  tree_data->doctype = "html";
  tree_data->loaded = root->IsLoaded();
  tree_data->loading_progress = root->EstimatedLoadingProgress();
  const Document& document = ax_object_cache_->GetDocument();
  tree_data->mimetype = document.IsXHTMLDocument() ? "text/xhtml" : "text/html";
  tree_data->title = document.title().Utf8();
  tree_data->url = document.Url().GetString().Utf8();

  if (AXObject* focus = GetFocusedObject())
    tree_data->focus_id = focus->AXObjectID();

  bool is_selection_backward = false;
  AXObject *anchor_object, *focus_object;
  int anchor_offset, focus_offset;
  ax::mojom::blink::TextAffinity anchor_affinity, focus_affinity;
  Selection(root, is_selection_backward, &anchor_object, anchor_offset,
            anchor_affinity, &focus_object, focus_offset, focus_affinity);
  if (anchor_object && focus_object && anchor_offset >= 0 &&
      focus_offset >= 0) {
    int32_t anchor_id = anchor_object->AXObjectID();
    int32_t focus_id = focus_object->AXObjectID();
    tree_data->sel_is_backward = is_selection_backward;
    tree_data->sel_anchor_object_id = anchor_id;
    tree_data->sel_anchor_offset = anchor_offset;
    tree_data->sel_focus_object_id = focus_id;
    tree_data->sel_focus_offset = focus_offset;
    tree_data->sel_anchor_affinity = anchor_affinity;
    tree_data->sel_focus_affinity = focus_affinity;
  }

  // Get the tree ID for this frame.
  if (LocalFrame* local_frame = document.GetFrame())
    tree_data->tree_id = GetAXTreeID(local_frame);

  if (auto* root_scroller = root->RootScroller())
    tree_data->root_scroller_id = root_scroller->AXObjectID();
  else
    tree_data->root_scroller_id = 0;

  if (ax_object_cache_->GetAXMode().has_mode(ui::AXMode::kHTMLMetadata)) {
    if (HTMLHeadElement* head = ax_object_cache_->GetDocument().head()) {
      for (Node* child = head->firstChild(); child;
           child = child->nextSibling()) {
        if (!child->IsElementNode())
          continue;
        Element* elem = To<Element>(child);
        if (elem->IsHTMLWithTagName("SCRIPT")) {
          if (elem->getAttribute("type") != "application/ld+json")
            continue;
        } else if (!elem->IsHTMLWithTagName("LINK") &&
                   !elem->IsHTMLWithTagName("TITLE") &&
                   !elem->IsHTMLWithTagName("META")) {
          continue;
        }
        // TODO(chrishtr): replace the below with elem->outerHTML().
        String tag = elem->tagName().LowerASCII();
        String html = "<" + tag;
        for (unsigned i = 0; i < elem->Attributes().size(); i++) {
          html = html + String(" ") + elem->Attributes().at(i).LocalName() +
                 String("=\"") + elem->Attributes().at(i).Value() + "\"";
        }
        html = html + String(">") + elem->innerHTML() + String("</") + tag +
               String(">");
        tree_data->metadata.push_back(html.Utf8());
      }
    }
  }

  return true;
}

void BlinkAXTreeSource::Freeze() {
  CHECK(!frozen_);
  frozen_ = true;
  root_ = GetRoot();
  focus_ = ax_object_cache_->FocusedObject();
}

void BlinkAXTreeSource::Thaw() {
  CHECK(frozen_);
  root_ = nullptr;
  focus_ = nullptr;
  frozen_ = false;
}

AXObject* BlinkAXTreeSource::GetRoot() const {
  if (root_)
    return root_;
  ax_object_cache_->UpdateLifecycleIfNeeded();
  return ax_object_cache_->Root();
}

AXObject* BlinkAXTreeSource::GetFocusedObject() const {
  if (focus_)
    return focus_;
  ax_object_cache_->UpdateLifecycleIfNeeded();
  return ax_object_cache_->FocusedObject();
}

AXObject* BlinkAXTreeSource::GetFromId(int32_t id) const {
  return ax_object_cache_->ObjectFromAXID(id);
}

int32_t BlinkAXTreeSource::GetId(AXObject* node) const {
  return node->AXObjectID();
}

void BlinkAXTreeSource::GetChildren(
    AXObject* parent,
    std::vector<AXObject*>* out_children) const {
  if (ui::CanHaveInlineTextBoxChildren(parent->RoleValue()) &&
      ShouldLoadInlineTextBoxes(parent)) {
    parent->LoadInlineTextBoxes();
  }

  for (int i = 0; i < parent->ChildCountIncludingIgnored(); i++) {
    AXObject* child = parent->ChildAtIncludingIgnored(i);

    // The child may be invalid due to issues in blink accessibility code.
    if (!child || child->IsDetached()) {
      NOTREACHED() << "Should not try to serialize an invalid child:"
                   << "\nParent: " << parent->ToString(true).Utf8()
                   << "\nChild: " << child->ToString(true).Utf8();
      continue;
    }

    if (!child->AccessibilityIsIncludedInTree()) {
      NOTREACHED() << "Should not receive unincluded child."
                   << "\nChild: " << child->ToString(true).Utf8()
                   << "\nParent: " << parent->ToString(true).Utf8();
      continue;
    }

#if DCHECK_IS_ON()
    CheckParentUnignoredOf(parent, child);
#endif

    // These should not be produced by Blink. They are only needed on Mac and
    // handled in AXTableInfo on the browser side.
    DCHECK_NE(child->RoleValue(), ax::mojom::blink::Role::kColumn);
    DCHECK_NE(child->RoleValue(),
              ax::mojom::blink::Role::kTableHeaderContainer);

    // If an optional exclude_offscreen flag is set (only intended to be
    // used for a one-time snapshot of the accessibility tree), prune any
    // node that's entirely offscreen from the tree.
    if (exclude_offscreen_ && child->IsOffScreen())
      continue;

    out_children->push_back(child);
  }
}

AXObject* BlinkAXTreeSource::GetParent(AXObject* node) const {
  // Blink returns ignored objects when walking up the parent chain,
  // we have to skip those here. Also, stop when we get to the root
  // element.
  do {
    if (node == GetRoot())
      return nullptr;
    node = node->ParentObject();
  } while (node && !node->IsDetached() &&
           !node->AccessibilityIsIncludedInTree());

  return node;
}

bool BlinkAXTreeSource::IsIgnored(AXObject* node) const {
  if (!node || node->IsDetached())
    return false;
  return node->AccessibilityIsIgnored();
}

bool BlinkAXTreeSource::IsValid(AXObject* node) const {
  return node && !node->IsDetached();
}

bool BlinkAXTreeSource::IsEqual(AXObject* node1, AXObject* node2) const {
  return node1 == node2;
}

AXObject* BlinkAXTreeSource::GetNull() const {
  return nullptr;
}

std::string BlinkAXTreeSource::GetDebugString(AXObject* node) const {
  if (!node || node->IsDetached())
    return "";
  return node->ToString(true).Utf8();
}

void BlinkAXTreeSource::SerializerClearedNode(int32_t node_id) {
  ax_object_cache_->SerializerClearedNode(node_id);
}

void BlinkAXTreeSource::SerializeNode(AXObject* src,
                                      ui::AXNodeData* dst) const {
#if DCHECK_IS_ON()
  // Never causes a document lifecycle change during serialization,
  // because the assumption is that layout is in a safe, stable state.
  DocumentLifecycle::DisallowTransitionScope disallow(
      ax_object_cache_->GetDocument().Lifecycle());
#endif

  if (!src || src->IsDetached() || !src->AccessibilityIsIncludedInTree()) {
    dst->AddState(ax::mojom::blink::State::kIgnored);
    dst->id = -1;
    dst->role = ax::mojom::blink::Role::kUnknown;
    NOTREACHED();
    return;
  }

  dst->id = src->AXObjectID();
  dst->role = src->RoleValue();

  // TODO(crbug.com/1068668): AX onion soup - finish migrating the rest of
  // this function inside of AXObject::Serialize and removing
  // unneeded AXObject interfaces.
  src->Serialize(dst, ax_object_cache_->GetAXMode());

  if (dst->id == ax_object_cache_->image_data_node_id()) {
    // In general, string attributes should be truncated using
    // TruncateAndAddStringAttribute, but ImageDataUrl contains a data url
    // representing an image, so add it directly using AddStringAttribute.
    dst->AddStringAttribute(ax::mojom::blink::StringAttribute::kImageDataUrl,
                            src->ImageDataUrl(max_image_data_size_).Utf8());
  }
}

void BlinkAXTreeSource::Trace(Visitor* visitor) const {
  visitor->Trace(ax_object_cache_);
  visitor->Trace(root_);
  visitor->Trace(focus_);
}

void BlinkAXTreeSource::OnLoadInlineTextBoxes(AXObject& obj) {
  if (ShouldLoadInlineTextBoxes(&obj))
    return;

  SetLoadInlineTextBoxesForId(obj.AXObjectID());

  ax_object_cache_->InvalidateSerializerSubtree(obj);
}

AXObject* BlinkAXTreeSource::GetPluginRoot() {
  AXObject* root = GetRoot();

  ax_object_cache_->UpdateLifecycleIfNeeded();

  HeapDeque<Member<AXObject>> objs_to_explore;
  objs_to_explore.push_back(root);
  while (objs_to_explore.size()) {
    AXObject* obj = objs_to_explore.front();
    objs_to_explore.pop_front();

    Node* node = obj->GetNode();
    if (node && node->IsElementNode()) {
      Element* element = To<Element>(node);
      if (element->IsHTMLWithTagName("embed")) {
        return obj;
      }
    }

    // Explore children of this object.
    std::vector<AXObject*> children;
    GetChildren(obj, &children);
    for (auto* child : children)
      objs_to_explore.push_back(child);
  }

  return nullptr;
}

}  // namespace blink
