// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_CONTAINER_FRAGMENT_BUILDER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_CONTAINER_FRAGMENT_BUILDER_H_

#include "base/dcheck_is_on.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/ng/exclusions/ng_exclusion_space.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_bfc_offset.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_margin_strut.h"
#include "third_party/blink/renderer/core/layout/ng/list/ng_unpositioned_list_marker.h"
#include "third_party/blink/renderer/core/layout/ng/ng_anchor_query.h"
#include "third_party/blink/renderer/core/layout/ng/ng_break_appeal.h"
#include "third_party/blink/renderer/core/layout/ng/ng_early_break.h"
#include "third_party/blink/renderer/core/layout/ng/ng_floats_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_link.h"
#include "third_party/blink/renderer/core/layout/ng/ng_logical_link.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_positioned_node.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class NGColumnSpannerPath;
class NGExclusionSpace;
class NGInlineBreakToken;
class NGPhysicalFragment;

class CORE_EXPORT NGContainerFragmentBuilder : public NGFragmentBuilder {
  STACK_ALLOCATED();

 public:
  ~NGContainerFragmentBuilder() {
    // Clear collections so the backing gets promptly freed, and reused.
    oof_positioned_candidates_.clear();
    oof_positioned_fragmentainer_descendants_.clear();
    oof_positioned_descendants_.clear();
    multicols_with_pending_oofs_.clear();
    child_break_tokens_.clear();
  }

  using ChildrenVector = HeapVector<NGLogicalLink, 4>;
  using MulticolCollection =
      HeapHashMap<Member<LayoutBox>,
                  Member<NGMulticolWithPendingOOFs<LogicalOffset>>>;

  LayoutUnit BfcLineOffset() const { return bfc_line_offset_; }
  void SetBfcLineOffset(LayoutUnit bfc_line_offset) {
    bfc_line_offset_ = bfc_line_offset;
  }

  // The BFC block-offset is where this fragment was positioned within the BFC.
  // If it is not set, this fragment may be placed anywhere within the BFC.
  const absl::optional<LayoutUnit>& BfcBlockOffset() const {
    return bfc_block_offset_;
  }
  void SetBfcBlockOffset(LayoutUnit bfc_block_offset) {
    bfc_block_offset_ = bfc_block_offset;
  }
  void ResetBfcBlockOffset() { bfc_block_offset_.reset(); }

  void SetEndMarginStrut(const NGMarginStrut& end_margin_strut) {
    end_margin_strut_ = end_margin_strut;
  }

  void SetMayHaveDescendantAboveBlockStart(bool b) {
#if DCHECK_IS_ON()
    is_may_have_descendant_above_block_start_explicitly_set_ = true;
#endif
    may_have_descendant_above_block_start_ = b;
  }

  NGExclusionSpace& ExclusionSpace() { return exclusion_space_; }
  void SetExclusionSpace(const NGExclusionSpace& exclusion_space) {
    exclusion_space_ = exclusion_space;
  }

  void SetLinesUntilClamp(const absl::optional<int>& value) {
    lines_until_clamp_ = value;
  }

  const NGUnpositionedListMarker& UnpositionedListMarker() const {
    return unpositioned_list_marker_;
  }
  void SetUnpositionedListMarker(const NGUnpositionedListMarker& marker) {
    DCHECK(!unpositioned_list_marker_ || !marker);
    unpositioned_list_marker_ = marker;
  }
  void ClearUnpositionedListMarker() {
    unpositioned_list_marker_ = NGUnpositionedListMarker();
  }

  void ReplaceChild(wtf_size_t index,
                    const NGPhysicalFragment& new_child,
                    const LogicalOffset offset);

  const ChildrenVector& Children() const { return children_; }

  // Builder has non-trivial OOF-positioned methods.
  // They are intended to be used by a layout algorithm like this:
  //
  // Part 1: layout algorithm positions in-flow children.
  //   out-of-flow children, and out-of-flow descendants of fragments
  //   are stored inside builder.
  //
  // for (child : children)
  //   if (child->position == (Absolute or Fixed))
  //     builder->AddOutOfFlowChildCandidate(child);
  //   else
  //     fragment = child->Layout()
  //     builder->AddChild(fragment)
  // end
  //
  // builder->SetSize
  //
  // Part 2: Out-of-flow layout part positions OOF-positioned nodes.
  //
  // NGOutOfFlowLayoutPart(container_style, builder).Run();
  //
  // See layout part for builder interaction.
  void AddOutOfFlowChildCandidate(NGBlockNode,
                                  const LogicalOffset& child_offset,
                                  NGLogicalStaticPosition::InlineEdge =
                                      NGLogicalStaticPosition::kInlineStart,
                                  NGLogicalStaticPosition::BlockEdge =
                                      NGLogicalStaticPosition::kBlockStart);

  void AddOutOfFlowChildCandidate(
      const NGLogicalOutOfFlowPositionedNode& candidate);

  // This should only be used for inline-level OOF-positioned nodes.
  // |inline_container_direction| is the current text direction for determining
  // the correct static-position.
  void AddOutOfFlowInlineChildCandidate(
      NGBlockNode,
      const LogicalOffset& child_offset,
      TextDirection inline_container_direction);

  void AddOutOfFlowFragmentainerDescendant(
      const NGLogicalOOFNodeForFragmentation& descendant);
  void AddOutOfFlowFragmentainerDescendant(
      const NGLogicalOutOfFlowPositionedNode& descendant);

  void AddOutOfFlowDescendant(
      const NGLogicalOutOfFlowPositionedNode& descendant);

  // Out-of-flow positioned elements inside a nested fragmentation context
  // are laid out once they've reached the outermost fragmentation context.
  // However, once at the outer context, they will get laid out inside the
  // inner multicol in which their containing block resides. Thus, we need to
  // store such inner multicols for later use.
  void AddMulticolWithPendingOOFs(
      const NGBlockNode& multicol,
      NGMulticolWithPendingOOFs<LogicalOffset>* multicol_info =
          MakeGarbageCollected<NGMulticolWithPendingOOFs<LogicalOffset>>());

  bool HasOutOfFlowFragmentChild() const {
    return has_out_of_flow_fragment_child_;
  }

  void SetHasOutOfFlowFragmentChild(bool has_out_of_flow_fragment_child) {
    has_out_of_flow_fragment_child_ = has_out_of_flow_fragment_child;
  }

  bool HasOutOfFlowInFragmentainerSubtree() const {
    return has_out_of_flow_in_fragmentainer_subtree_;
  }

  void SetHasOutOfFlowInFragmentainerSubtree(
      bool has_out_of_flow_in_fragmentainer_subtree) {
    has_out_of_flow_in_fragmentainer_subtree_ =
        has_out_of_flow_in_fragmentainer_subtree;
  }

  void SwapOutOfFlowPositionedCandidates(
      HeapVector<NGLogicalOutOfFlowPositionedNode>* candidates);

  void SwapOutOfFlowFragmentainerDescendants(
      HeapVector<NGLogicalOOFNodeForFragmentation>* descendants);

  void SwapMulticolsWithPendingOOFs(
      MulticolCollection* multicols_with_pending_oofs);

  // Transfer the candidates from |oof_positioned_candidates_| to
  // |destination_builder|, adding any |additional_offset| to the candidate
  // static positions. |multicol| indicates that the candidates were passed
  // up the tree via an inner multicol. This will be used to determine if
  // a candidate should be added as a fragmentainer descendant instead
  // (i.e. in the case where the |multicol| has found a fixedpos containing
  // block in its ancestor path).
  void TransferOutOfFlowCandidates(
      NGContainerFragmentBuilder* destination_builder,
      LogicalOffset additional_offset,
      const NGMulticolWithPendingOOFs<LogicalOffset>* multicol = nullptr);

  bool HasOutOfFlowPositionedCandidates() const {
    return !oof_positioned_candidates_.IsEmpty();
  }

  bool HasOutOfFlowPositionedDescendants() const {
    return !oof_positioned_descendants_.IsEmpty();
  }

  bool HasOutOfFlowFragmentainerDescendants() const {
    return !oof_positioned_fragmentainer_descendants_.IsEmpty();
  }

  bool HasMulticolsWithPendingOOFs() const {
    return !multicols_with_pending_oofs_.IsEmpty();
  }

  HeapVector<NGLogicalOutOfFlowPositionedNode>*
  MutableOutOfFlowPositionedCandidates() {
    return &oof_positioned_candidates_;
  }

  // This method should only be used within the inline layout algorithm. It is
  // used to convert all OOF-positioned candidates to descendants.
  //
  // During the inline layout algorithm, we don't have enough information to
  // position OOF candidates yet, (as a containing box may be split over
  // multiple lines), instead we bubble all the descendants up to the parent
  // block layout algorithm, to perform the final OOF layout and positioning.
  void MoveOutOfFlowDescendantCandidatesToDescendants();

  // Propagate the OOF descendants from a fragment to the builder. Since the OOF
  // descendants on the fragment are NGPhysicalOutOfFlowPositionedNodes, we
  // first have to create NGLogicalOutOfFlowPositionedNodes copies before
  // appending them to our list of descendants.
  // In addition, propagate any inner multicols with pending OOF descendants.
  void PropagateOOFPositionedInfo(
      const NGPhysicalFragment& fragment,
      LogicalOffset offset,
      LogicalOffset relative_offset,
      LogicalOffset offset_adjustment = LogicalOffset(),
      const NGInlineContainer<LogicalOffset>* inline_container = nullptr,
      LayoutUnit containing_block_adjustment = LayoutUnit(),
      const NGContainingBlock<LogicalOffset>* fixedpos_containing_block =
          nullptr,
      const NGInlineContainer<LogicalOffset>* fixedpos_inline_container =
          nullptr,
      LogicalOffset additional_fixedpos_offset = LogicalOffset());
  // Same as PropagateOOFPositionedInfo(), but only performs the propagation of
  // OOF fragmentainer descendants. If |out_list| is provided, any OOF
  // fragmentainer descendants should be propagated there rather than to this
  // builder.
  void PropagateOOFFragmentainerDescendants(
      const NGPhysicalFragment& fragment,
      LogicalOffset offset,
      LogicalOffset relative_offset,
      LayoutUnit containing_block_adjustment,
      const NGContainingBlock<LogicalOffset>* fixedpos_containing_block,
      HeapVector<NGLogicalOOFNodeForFragmentation>* out_list = nullptr);

  void SetIsSelfCollapsing() { is_self_collapsing_ = true; }

  void SetIsPushedByFloats() { is_pushed_by_floats_ = true; }
  bool IsPushedByFloats() const { return is_pushed_by_floats_; }

  // Set when this subtree has modified the incoming margin-strut, such that it
  // may change our final position.
  void SetSubtreeModifiedMarginStrut() {
    DCHECK(!BfcBlockOffset());
    subtree_modified_margin_strut_ = true;
  }

  void ResetAdjoiningObjectTypes() {
    adjoining_object_types_ = kAdjoiningNone;
    has_adjoining_object_descendants_ = false;
  }
  void AddAdjoiningObjectTypes(NGAdjoiningObjectTypes adjoining_object_types) {
    adjoining_object_types_ |= adjoining_object_types;
    has_adjoining_object_descendants_ |= adjoining_object_types;
  }
  void SetAdjoiningObjectTypes(NGAdjoiningObjectTypes adjoining_object_types) {
    adjoining_object_types_ = adjoining_object_types;
  }
  NGAdjoiningObjectTypes AdjoiningObjectTypes() const {
    return adjoining_object_types_;
  }

  void SetIsBlockInInline() { is_block_in_inline_ = true; }

  void SetHasBlockFragmentation() { has_block_fragmentation_ = true; }

  // Set for any node that establishes a fragmentation context, such as multicol
  // containers.
  void SetIsBlockFragmentationContextRoot() {
    is_fragmentation_context_root_ = true;
  }

  bool IsBlockFragmentationContextRoot() const {
    return is_fragmentation_context_root_;
  }

  // There may be cases where a column spanner was previously found but is no
  // longer accessible. For example, in simplified OOF layout, we may want to
  // recreate a spanner break for an existing fragment being relaid out, but
  // the spanner node is no longer available. In such cases,
  // |has_column_spanner_| may be true while |column_spanner_path_| is not set.
  void SetHasColumnSpanner(bool has_column_spanner) {
    has_column_spanner_ = has_column_spanner;
  }
  void SetColumnSpannerPath(const NGColumnSpannerPath* spanner_path) {
    column_spanner_path_ = spanner_path;
    SetHasColumnSpanner(!!spanner_path);
  }
  bool FoundColumnSpanner() const {
    DCHECK(has_column_spanner_ || !column_spanner_path_);
    return has_column_spanner_;
  }
  void SetIsEmptySpannerParent(bool is_empty_spanner_parent) {
    DCHECK(FoundColumnSpanner());
    is_empty_spanner_parent_ = is_empty_spanner_parent;
  }
  bool IsEmptySpannerParent() const { return is_empty_spanner_parent_; }

  void SetShouldForceSameFragmentationFlow() {
    should_force_same_fragmentation_flow_ = true;
  }

  // Specify that all child break tokens be added manually, instead of being
  // added automatically as part of adding child fragments.
  void SetShouldAddBreakTokensManually() {
    should_add_break_tokens_manually_ = true;
  }

  // See NGLayoutResult::AnnotationOverflow().
  void SetAnnotationOverflow(LayoutUnit overflow) {
    annotation_overflow_ = overflow;
  }
  LayoutUnit AnnotationOverflow() const { return annotation_overflow_; }

  // See NGLayoutRsult::BlockEndAnnotatioSpace().
  void SetBlockEndAnnotationSpace(LayoutUnit space) {
    block_end_annotation_space_ = space;
  }

  void SetHasDescendantThatDependsOnPercentageBlockSize(bool b = true) {
    has_descendant_that_depends_on_percentage_block_size_ = b;
  }

  const NGConstraintSpace& ConstraintSpace() const { return space_; }

  const NGLogicalAnchorQuery& AnchorQuery() const { return anchor_query_; }

  const NGLayoutResult* Abort(NGLayoutResult::EStatus);

#if DCHECK_IS_ON()
  String ToString() const;
#endif

 protected:
  friend class NGInlineLayoutStateStack;
  friend class NGLayoutResult;
  friend class NGPhysicalFragment;

  NGContainerFragmentBuilder(NGLayoutInputNode node,
                             scoped_refptr<const ComputedStyle> style,
                             const NGConstraintSpace& space,
                             WritingDirectionMode writing_direction)
      : NGFragmentBuilder(std::move(style), writing_direction),
        node_(node),
        space_(space) {
    layout_object_ = node.GetLayoutBox();
  }

  void PropagateChildData(
      const NGPhysicalFragment& child,
      LogicalOffset child_offset,
      LogicalOffset relative_offset,
      const NGInlineContainer<LogicalOffset>* inline_container = nullptr,
      absl::optional<LayoutUnit> adjustment_for_oof_propagation = LayoutUnit());

  void AddChildInternal(const NGPhysicalFragment*, const LogicalOffset&);

  // Set the fixedpos inline container and containing block based on the current
  // |box_fragment|, |relative_offset| and |current_inline_container|.
  void AdjustFixedposContainerInfo(
      const NGPhysicalFragment* box_fragment,
      LogicalOffset relative_offset,
      NGInlineContainer<LogicalOffset>* fixedpos_inline_container,
      const NGPhysicalFragment** fixedpos_containing_block_fragment,
      const NGInlineContainer<LogicalOffset>* current_inline_container =
          nullptr) const;

  NGLayoutInputNode node_;
  const NGConstraintSpace& space_;

  LayoutUnit bfc_line_offset_;
  absl::optional<LayoutUnit> bfc_block_offset_;
  NGMarginStrut end_margin_strut_;
  NGExclusionSpace exclusion_space_;
  absl::optional<int> lines_until_clamp_;

  HeapVector<NGLogicalOutOfFlowPositionedNode> oof_positioned_candidates_;
  HeapVector<NGLogicalOOFNodeForFragmentation>
      oof_positioned_fragmentainer_descendants_;
  HeapVector<NGLogicalOutOfFlowPositionedNode> oof_positioned_descendants_;
  NGLogicalAnchorQuery anchor_query_;

  MulticolCollection multicols_with_pending_oofs_;

  NGUnpositionedListMarker unpositioned_list_marker_;

  ChildrenVector children_;

  // Only used by the NGBoxFragmentBuilder subclass, but defined here to avoid
  // a virtual function call.
  NGBreakTokenVector child_break_tokens_;
  const NGInlineBreakToken* last_inline_break_token_ = nullptr;

  const NGColumnSpannerPath* column_spanner_path_ = nullptr;

  const NGEarlyBreak* early_break_ = nullptr;

  // See NGLayoutResult::AnnotationOverflow().
  LayoutUnit annotation_overflow_;
  // See NGLayoutResult::BlockEndAnotationSpace().
  LayoutUnit block_end_annotation_space_;

  // The number of line boxes or flex lines added to the builder. Only updated
  // if we're performing block fragmentation.
  int line_count_ = 0;

  NGAdjoiningObjectTypes adjoining_object_types_ = kAdjoiningNone;
  bool has_adjoining_object_descendants_ = false;

  bool is_self_collapsing_ = false;
  bool is_pushed_by_floats_ = false;
  bool subtree_modified_margin_strut_ = false;
  bool is_legacy_layout_root_ = false;
  bool is_block_in_inline_ = false;

  bool has_floating_descendants_for_paint_ = false;
  bool has_descendant_that_depends_on_percentage_block_size_ = false;
  bool may_have_descendant_above_block_start_ = false;
  bool has_block_fragmentation_ = false;
  bool is_fragmentation_context_root_ = false;
  bool has_column_spanner_ = false;
  bool is_empty_spanner_parent_ = false;
  bool should_force_same_fragmentation_flow_ = false;
  bool should_add_break_tokens_manually_ = false;

  bool has_out_of_flow_fragment_child_ = false;
  bool has_out_of_flow_in_fragmentainer_subtree_ = false;

#if DCHECK_IS_ON()
  bool is_may_have_descendant_above_block_start_explicitly_set_ = false;
#endif
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_CONTAINER_FRAGMENT_BUILDER_H_
