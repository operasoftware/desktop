// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/grid/layout_ng_grid.h"

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_line_resolver.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"

namespace blink {

LayoutNGGrid::LayoutNGGrid(Element* element)
    : LayoutNGMixin<LayoutBlock>(element) {}

void LayoutNGGrid::UpdateBlockLayout(bool relayout_children) {
  if (IsOutOfFlowPositioned()) {
    UpdateOutOfFlowBlockLayout();
    return;
  }
  UpdateInFlowBlockLayout();
}

void LayoutNGGrid::AddChild(LayoutObject* new_child,
                            LayoutObject* before_child) {
  NOT_DESTROYED();
  LayoutBlock::AddChild(new_child, before_child);

  // Out-of-flow grid items don't impact placement.
  if (!new_child->IsOutOfFlowPositioned())
    SetGridPlacementDirty(true);
}

void LayoutNGGrid::RemoveChild(LayoutObject* child) {
  NOT_DESTROYED();
  LayoutBlock::RemoveChild(child);

  // Out-of-flow grid items don't impact placement.
  if (!child->IsOutOfFlowPositioned())
    SetGridPlacementDirty(true);
}

namespace {

bool ExplicitGridDidResize(const ComputedStyle& new_style,
                           const ComputedStyle& old_style) {
  const auto& old_ng_columns_track_list =
      old_style.GridTemplateColumns().TrackList();
  const auto& new_ng_columns_track_list =
      new_style.GridTemplateColumns().TrackList();
  const auto& old_ng_rows_track_list = old_style.GridTemplateRows().TrackList();
  const auto& new_ng_rows_track_list = new_style.GridTemplateRows().TrackList();

  return old_ng_columns_track_list.TrackCountWithoutAutoRepeat() !=
             new_ng_columns_track_list.TrackCountWithoutAutoRepeat() ||
         old_ng_rows_track_list.TrackCountWithoutAutoRepeat() !=
             new_ng_rows_track_list.TrackCountWithoutAutoRepeat() ||
         old_ng_columns_track_list.AutoRepeatTrackCount() !=
             new_ng_columns_track_list.AutoRepeatTrackCount() ||
         old_ng_rows_track_list.AutoRepeatTrackCount() !=
             new_ng_rows_track_list.AutoRepeatTrackCount() ||
         old_style.NamedGridAreaColumnCount() !=
             new_style.NamedGridAreaColumnCount() ||
         old_style.NamedGridAreaRowCount() != new_style.NamedGridAreaRowCount();
}

bool NamedGridLinesDefinitionDidChange(const ComputedStyle& new_style,
                                       const ComputedStyle& old_style) {
  return new_style.GridTemplateRows().named_grid_lines !=
             old_style.GridTemplateRows().named_grid_lines ||
         new_style.GridTemplateColumns().named_grid_lines !=
             old_style.GridTemplateColumns().named_grid_lines ||
         new_style.ImplicitNamedGridRowLines() !=
             old_style.ImplicitNamedGridRowLines() ||
         new_style.ImplicitNamedGridColumnLines() !=
             old_style.ImplicitNamedGridColumnLines();
}

}  // namespace

void LayoutNGGrid::StyleDidChange(StyleDifference diff,
                                  const ComputedStyle* old_style) {
  NOT_DESTROYED();
  LayoutBlock::StyleDidChange(diff, old_style);
  if (!old_style)
    return;

  const auto& new_style = StyleRef();
  const auto& new_grid_columns_track_list =
      new_style.GridTemplateColumns().TrackList();
  const auto& new_grid_rows_track_list =
      new_style.GridTemplateRows().TrackList();

  if (new_grid_columns_track_list !=
          old_style->GridTemplateColumns().TrackList() ||
      new_grid_rows_track_list != old_style->GridTemplateRows().TrackList() ||
      new_style.GridAutoColumns() != old_style->GridAutoColumns() ||
      new_style.GridAutoRows() != old_style->GridAutoRows() ||
      new_style.GetGridAutoFlow() != old_style->GetGridAutoFlow()) {
    SetGridPlacementDirty(true);
  }

  if (ExplicitGridDidResize(new_style, *old_style) ||
      NamedGridLinesDefinitionDidChange(new_style, *old_style) ||
      (diff.NeedsLayout() &&
       (new_grid_columns_track_list.AutoRepeatTrackCount() ||
        new_grid_rows_track_list.AutoRepeatTrackCount()))) {
    SetGridPlacementDirty(true);
  }
}

const LayoutNGGridInterface* LayoutNGGrid::ToLayoutNGGridInterface() const {
  NOT_DESTROYED();
  return this;
}

const NGGridPlacementData& LayoutNGGrid::CachedPlacementData() const {
  DCHECK(!IsGridPlacementDirty());
  return cached_placement_data_;
}

void LayoutNGGrid::SetCachedPlacementData(
    NGGridPlacementData&& placement_data) {
  cached_placement_data_ = std::move(placement_data);
  SetGridPlacementDirty(false);
}

const NGGridLayoutData* LayoutNGGrid::GridLayoutData() const {
  // Retrieve the layout data from the last fragment as it has the most
  // up-to-date grid geometry.
  const wtf_size_t fragment_count = PhysicalFragmentCount();
  if (fragment_count == 0)
    return nullptr;
  return GetLayoutResult(fragment_count - 1)->GridLayoutData();
}

wtf_size_t LayoutNGGrid::AutoRepeatCountForDirection(
    const GridTrackSizingDirection track_direction) const {
  NOT_DESTROYED();
  if (IsGridPlacementDirty())
    return 0;

  const bool is_for_columns = track_direction == kForColumns;
  const wtf_size_t auto_repeat_size =
      is_for_columns
          ? StyleRef().GridTemplateColumns().TrackList().AutoRepeatTrackCount()
          : StyleRef().GridTemplateRows().TrackList().AutoRepeatTrackCount();

  return auto_repeat_size *
         (is_for_columns ? cached_placement_data_.column_auto_repetitions
                         : cached_placement_data_.row_auto_repetitions);
}

wtf_size_t LayoutNGGrid::ExplicitGridStartForDirection(
    const GridTrackSizingDirection track_direction) const {
  NOT_DESTROYED();
  if (IsGridPlacementDirty())
    return 0;
  return (track_direction == kForColumns)
             ? cached_placement_data_.column_start_offset
             : cached_placement_data_.row_start_offset;
}

wtf_size_t LayoutNGGrid::ExplicitGridEndForDirection(
    const GridTrackSizingDirection track_direction) const {
  NOT_DESTROYED();
  if (IsGridPlacementDirty())
    return 0;

  const bool is_for_columns = track_direction == kForColumns;
  const wtf_size_t subgrid_span_size =
      is_for_columns ? cached_placement_data_.column_subgrid_span_size
                     : cached_placement_data_.row_subgrid_span_size;

  const wtf_size_t explicit_grid_track_count =
      is_for_columns ? NGGridLineResolver::ExplicitGridColumnCount(
                           StyleRef(), AutoRepeatCountForDirection(kForColumns),
                           subgrid_span_size)
                     : NGGridLineResolver::ExplicitGridRowCount(
                           StyleRef(), AutoRepeatCountForDirection(kForRows),
                           subgrid_span_size);

  return base::checked_cast<wtf_size_t>(
      ExplicitGridStartForDirection(track_direction) +
      explicit_grid_track_count);
}

LayoutUnit LayoutNGGrid::GridGap(
    const GridTrackSizingDirection track_direction) const {
  NOT_DESTROYED();
  const auto* grid_layout_data = GridLayoutData();
  if (!grid_layout_data)
    return LayoutUnit();

  return (track_direction == kForColumns)
             ? grid_layout_data->Columns()->GutterSize()
             : grid_layout_data->Rows()->GutterSize();
}

LayoutUnit LayoutNGGrid::GridItemOffset(
    const GridTrackSizingDirection track_direction) const {
  NOT_DESTROYED();
  // Distribution offset is baked into the gutter_size in GridNG.
  return LayoutUnit();
}

Vector<LayoutUnit, 1> LayoutNGGrid::TrackSizesForComputedStyle(
    const GridTrackSizingDirection track_direction) const {
  NOT_DESTROYED();
  Vector<LayoutUnit, 1> track_sizes;
  const auto* grid_layout_data = GridLayoutData();
  if (!grid_layout_data)
    return track_sizes;

  const auto& track_collection = (track_direction == kForColumns)
                                     ? *grid_layout_data->Columns()
                                     : *grid_layout_data->Rows();

  // |EndLineOfImplicitGrid| is equivalent to the total track count.
  track_sizes.ReserveInitialCapacity(std::min<wtf_size_t>(
      track_collection.EndLineOfImplicitGrid(), kGridMaxTracks));

  const wtf_size_t range_count = track_collection.RangeCount();
  for (wtf_size_t i = 0; i < range_count; ++i) {
    auto track_sizes_in_range =
        ComputeTrackSizeRepeaterForRange(track_collection, i);

    const wtf_size_t range_track_count = track_collection.RangeTrackCount(i);
    for (wtf_size_t j = 0; j < range_track_count; ++j) {
      track_sizes.emplace_back(
          track_sizes_in_range[j % track_sizes_in_range.size()]);

      // Respect total track count limit.
      DCHECK(track_sizes.size() <= kGridMaxTracks);
      if (track_sizes.size() == kGridMaxTracks)
        return track_sizes;
    }
  }
  return track_sizes;
}

Vector<LayoutUnit> LayoutNGGrid::RowPositions() const {
  NOT_DESTROYED();
  return ComputeExpandedPositions(kForRows);
}

Vector<LayoutUnit> LayoutNGGrid::ColumnPositions() const {
  NOT_DESTROYED();
  return ComputeExpandedPositions(kForColumns);
}

Vector<LayoutUnit> LayoutNGGrid::ComputeTrackSizeRepeaterForRange(
    const NGGridLayoutTrackCollection& track_collection,
    wtf_size_t range_index) const {
  const wtf_size_t range_set_count =
      track_collection.RangeSetCount(range_index);

  if (!range_set_count)
    return {LayoutUnit()};

  Vector<LayoutUnit> track_sizes;
  track_sizes.ReserveInitialCapacity(range_set_count);

  const wtf_size_t begin_set_index =
      track_collection.RangeBeginSetIndex(range_index);
  const wtf_size_t end_set_index = begin_set_index + range_set_count;

  for (wtf_size_t i = begin_set_index; i < end_set_index; ++i) {
    LayoutUnit set_size =
        track_collection.GetSetOffset(i + 1) - track_collection.GetSetOffset(i);
    const wtf_size_t set_track_count = track_collection.GetSetTrackCount(i);

    DCHECK_GE(set_size, 0);
    set_size = (set_size - track_collection.GutterSize() * set_track_count)
                   .ClampNegativeToZero();

    // Once we have determined the size of the set, we can find the size of a
    // given track by dividing the |set_size| by the |set_track_count|.
    // In some situations, this will leave a remainder, but rather than try to
    // distribute the space unequally between tracks, discard it to prefer equal
    // length tracks.
    DCHECK_GT(set_track_count, 0u);
    track_sizes.emplace_back(set_size / set_track_count);
  }
  return track_sizes;
}

Vector<LayoutUnit> LayoutNGGrid::ComputeExpandedPositions(
    const GridTrackSizingDirection track_direction) const {
  Vector<LayoutUnit> expanded_positions;
  const auto* grid_layout_data = GridLayoutData();
  if (!grid_layout_data)
    return expanded_positions;

  const auto& track_collection = (track_direction == kForColumns)
                                     ? *grid_layout_data->Columns()
                                     : *grid_layout_data->Rows();

  // |EndLineOfImplicitGrid| is equivalent to the total track count.
  expanded_positions.ReserveInitialCapacity(std::min<wtf_size_t>(
      track_collection.EndLineOfImplicitGrid() + 1, kGridMaxTracks + 1));

  auto current_offset = track_collection.GetSetOffset(0);
  expanded_positions.emplace_back(current_offset);

  auto last_applied_gutter_size = LayoutUnit();
  auto BuildExpandedPositions = [&]() {
    const wtf_size_t range_count = track_collection.RangeCount();

    for (wtf_size_t i = 0; i < range_count; ++i) {
      auto track_sizes_in_range =
          ComputeTrackSizeRepeaterForRange(track_collection, i);
      last_applied_gutter_size = track_collection.RangeSetCount(i)
                                     ? track_collection.GutterSize()
                                     : LayoutUnit();

      const wtf_size_t range_track_count = track_collection.RangeTrackCount(i);
      for (wtf_size_t j = 0; j < range_track_count; ++j) {
        current_offset +=
            track_sizes_in_range[j % track_sizes_in_range.size()] +
            last_applied_gutter_size;
        expanded_positions.emplace_back(current_offset);

        // Respect total track count limit, don't forget to account for the
        // initial offset.
        DCHECK(expanded_positions.size() <= kGridMaxTracks + 1);
        if (expanded_positions.size() == kGridMaxTracks + 1)
          return;
      }
    }
  };

  BuildExpandedPositions();
  expanded_positions.back() -= last_applied_gutter_size;
  return expanded_positions;
}

}  // namespace blink
