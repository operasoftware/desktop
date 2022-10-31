// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/events/pointer_event_factory.h"

#include <gtest/gtest.h>

#include <limits>

#include "base/time/time.h"
#include "third_party/blink/public/common/input/web_pointer_properties.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/pointer_type_names.h"

namespace blink {

class PointerEventFactoryTest : public testing::Test {
 protected:
  void SetUp() override;
  PointerEvent* CreateAndCheckPointerCancel(WebPointerProperties::PointerType,
                                            int raw_id,
                                            int unique_id,
                                            bool is_primary);
  PointerEvent* CreateAndCheckWebPointerEvent(
      WebPointerProperties::PointerType pointer_type,
      int raw_id,
      int unique_id,
      bool is_primary,
      bool hovering,
      WebInputEvent::Modifiers modifiers = WebInputEvent::kNoModifiers,
      WebInputEvent::Type type = WebInputEvent::Type::kPointerDown,
      WebPointerProperties::Button button =
          WebPointerProperties::Button::kNoButton,
      wtf_size_t coalesced_event_count = 0,
      wtf_size_t predicted_event_count = 0) {
    WebPointerEvent web_pointer_event;
    web_pointer_event.pointer_type = pointer_type;
    web_pointer_event.id = raw_id;
    web_pointer_event.SetType(type);
    web_pointer_event.SetTimeStamp(WebInputEvent::GetStaticTimeStampForTests());
    web_pointer_event.SetModifiers(modifiers);
    web_pointer_event.force = 1.0;
    web_pointer_event.hovering = hovering;
    web_pointer_event.button = button;
    web_pointer_event.SetPositionInScreen(100, 100);
    Vector<WebPointerEvent> coalesced_events;
    for (wtf_size_t i = 0; i < coalesced_event_count; i++) {
      coalesced_events.push_back(web_pointer_event);
    }
    Vector<WebPointerEvent> predicted_events;
    for (wtf_size_t i = 0; i < predicted_event_count; i++) {
      predicted_events.push_back(web_pointer_event);
    }
    PointerEvent* pointer_event = pointer_event_factory_.Create(
        web_pointer_event, coalesced_events, predicted_events, nullptr);
    EXPECT_EQ(unique_id, pointer_event->pointerId());
    EXPECT_EQ(is_primary, pointer_event->isPrimary());
    EXPECT_EQ(WebInputEvent::GetStaticTimeStampForTests(),
              pointer_event->PlatformTimeStamp());
    const String& expected_pointer_type =
        PointerEventFactory::PointerTypeNameForWebPointPointerType(
            pointer_type);
    EXPECT_EQ(expected_pointer_type, pointer_event->pointerType());

    EXPECT_EQ(!!(modifiers & WebInputEvent::kControlKey),
              pointer_event->ctrlKey());
    EXPECT_EQ(!!(modifiers & WebInputEvent::kShiftKey),
              pointer_event->shiftKey());
    EXPECT_EQ(!!(modifiers & WebInputEvent::kAltKey), pointer_event->altKey());
    EXPECT_EQ(!!(modifiers & WebInputEvent::kMetaKey),
              pointer_event->metaKey());

    if (type == WebInputEvent::Type::kPointerMove) {
      EXPECT_EQ(coalesced_event_count,
                pointer_event->getCoalescedEvents().size());
      EXPECT_EQ(predicted_event_count,
                pointer_event->getPredictedEvents().size());
      for (wtf_size_t i = 0; i < coalesced_event_count; i++) {
        EXPECT_EQ(unique_id,
                  pointer_event->getCoalescedEvents()[i]->pointerId());
        EXPECT_EQ(is_primary,
                  pointer_event->getCoalescedEvents()[i]->isPrimary());
        EXPECT_EQ(expected_pointer_type, pointer_event->pointerType());
        EXPECT_EQ(WebInputEvent::GetStaticTimeStampForTests(),
                  pointer_event->PlatformTimeStamp());
      }
      for (wtf_size_t i = 0; i < predicted_event_count; i++) {
        EXPECT_EQ(unique_id,
                  pointer_event->getPredictedEvents()[i]->pointerId());
        EXPECT_EQ(is_primary,
                  pointer_event->getPredictedEvents()[i]->isPrimary());
        EXPECT_EQ(expected_pointer_type, pointer_event->pointerType());
        EXPECT_EQ(WebInputEvent::GetStaticTimeStampForTests(),
                  pointer_event->PlatformTimeStamp());
      }
    } else {
      EXPECT_EQ(0u, pointer_event->getCoalescedEvents().size());
      EXPECT_EQ(0u, pointer_event->getPredictedEvents().size());
    }
    EXPECT_EQ(
        pointer_event_factory_.GetLastPointerPosition(
            pointer_event->pointerId(),
            WebPointerProperties(1, WebPointerProperties::PointerType::kUnknown,
                                 WebPointerProperties::Button::kNoButton,
                                 gfx::PointF(50, 50), gfx::PointF(20, 20)),
            type),
        gfx::PointF(100, 100));
    return pointer_event;
  }
  void CreateAndCheckPointerTransitionEvent(PointerEvent*, const AtomicString&);
  void CheckNonHoveringPointers(const HashSet<int>& expected);

  PointerEventFactory pointer_event_factory_;
  int expected_mouse_id_;
  int mapped_id_start_;

};

void PointerEventFactoryTest::SetUp() {
  expected_mouse_id_ = 1;
  mapped_id_start_ = 2;
}

PointerEvent* PointerEventFactoryTest::CreateAndCheckPointerCancel(
    WebPointerProperties::PointerType pointer_type,
    int raw_id,
    int unique_id,
    bool is_primary) {
  PointerEvent* pointer_event = pointer_event_factory_.CreatePointerCancelEvent(
      unique_id, WebInputEvent::GetStaticTimeStampForTests());
  EXPECT_EQ("pointercancel", pointer_event->type());
  EXPECT_EQ(unique_id, pointer_event->pointerId());
  EXPECT_EQ(is_primary, pointer_event->isPrimary());
  EXPECT_EQ(
      PointerEventFactory::PointerTypeNameForWebPointPointerType(pointer_type),
      pointer_event->pointerType());
  EXPECT_EQ(WebInputEvent::GetStaticTimeStampForTests(),
            pointer_event->PlatformTimeStamp());

  return pointer_event;
}

void PointerEventFactoryTest::CreateAndCheckPointerTransitionEvent(
    PointerEvent* pointer_event,
    const AtomicString& type) {
  PointerEvent* clone_pointer_event =
      pointer_event_factory_.CreatePointerBoundaryEvent(pointer_event, type,
                                                        nullptr);
  EXPECT_EQ(clone_pointer_event->pointerType(), pointer_event->pointerType());
  EXPECT_EQ(clone_pointer_event->pointerId(), pointer_event->pointerId());
  EXPECT_EQ(clone_pointer_event->isPrimary(), pointer_event->isPrimary());
  EXPECT_EQ(clone_pointer_event->type(), type);

  EXPECT_EQ(clone_pointer_event->ctrlKey(), pointer_event->ctrlKey());
  EXPECT_EQ(clone_pointer_event->shiftKey(), pointer_event->shiftKey());
  EXPECT_EQ(clone_pointer_event->altKey(), pointer_event->altKey());
  EXPECT_EQ(clone_pointer_event->metaKey(), pointer_event->metaKey());
}

void PointerEventFactoryTest::CheckNonHoveringPointers(
    const HashSet<int>& expected_pointers) {
  Vector<int> pointers =
      pointer_event_factory_.GetPointerIdsOfNonHoveringPointers();
  EXPECT_EQ(pointers.size(), expected_pointers.size());
  for (int p : pointers) {
    EXPECT_TRUE(expected_pointers.find(p) != expected_pointers.end());
  }
}

TEST_F(PointerEventFactoryTest, MousePointer) {
  EXPECT_TRUE(pointer_event_factory_.IsActive(expected_mouse_id_));
  EXPECT_FALSE(pointer_event_factory_.IsActiveButtonsState(expected_mouse_id_));

  PointerEvent* pointer_event1 = CreateAndCheckWebPointerEvent(
      WebPointerProperties::PointerType::kMouse, 0, expected_mouse_id_,
      true /* isprimary */, true /* hovering */);
  PointerEvent* pointer_event2 = CreateAndCheckWebPointerEvent(
      WebPointerProperties::PointerType::kMouse, 0, expected_mouse_id_,
      true /* isprimary */, true /* hovering */,
      WebInputEvent::kLeftButtonDown);

  CreateAndCheckPointerTransitionEvent(pointer_event1,
                                       event_type_names::kPointerout);

  EXPECT_TRUE(pointer_event_factory_.IsActive(expected_mouse_id_));
  EXPECT_TRUE(pointer_event_factory_.IsActiveButtonsState(expected_mouse_id_));

  pointer_event_factory_.Remove(pointer_event1->pointerId());

  EXPECT_TRUE(pointer_event_factory_.IsActive(expected_mouse_id_));
  EXPECT_TRUE(pointer_event_factory_.IsActiveButtonsState(expected_mouse_id_));

  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kMouse, 0,
                                expected_mouse_id_, true /* isprimary */,
                                true /* hovering */);

  EXPECT_TRUE(pointer_event_factory_.IsActive(expected_mouse_id_));
  EXPECT_FALSE(pointer_event_factory_.IsActiveButtonsState(expected_mouse_id_));

  pointer_event_factory_.Remove(pointer_event1->pointerId());
  pointer_event_factory_.Remove(pointer_event2->pointerId());

  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kMouse, 1,
                                expected_mouse_id_, true /* isprimary */,
                                true /* hovering */);
  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kMouse, 20,
                                expected_mouse_id_, true /* isprimary */,
                                true /* hovering */);

  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kMouse, 0,
                                expected_mouse_id_, true /* isprimary */,
                                true /* hovering */,
                                WebInputEvent::kLeftButtonDown);

  EXPECT_TRUE(pointer_event_factory_.IsActive(expected_mouse_id_));
  EXPECT_TRUE(pointer_event_factory_.IsActiveButtonsState(expected_mouse_id_));

  CreateAndCheckPointerCancel(WebPointerProperties::PointerType::kMouse, 0,
                              expected_mouse_id_, true);

  EXPECT_TRUE(pointer_event_factory_.IsActive(expected_mouse_id_));
  EXPECT_FALSE(pointer_event_factory_.IsActiveButtonsState(expected_mouse_id_));
}

TEST_F(PointerEventFactoryTest, TouchPointerPrimaryRemovedWhileAnotherIsThere) {
  PointerEvent* pointer_event1 = CreateAndCheckWebPointerEvent(
      WebPointerProperties::PointerType::kTouch, 0, mapped_id_start_,
      true /* isprimary */, false /* hovering */);
  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kTouch, 1,
                                mapped_id_start_ + 1, false /* isprimary */,
                                false /* hovering */);

  pointer_event_factory_.Remove(pointer_event1->pointerId());

  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kTouch, 2,
                                mapped_id_start_ + 2, false /* isprimary */,
                                false /* hovering */);
  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kTouch, 1,
                                mapped_id_start_ + 1, false /* isprimary */,
                                false /* hovering */);
}

TEST_F(PointerEventFactoryTest, TouchPointerReleasedAndPressedAgain) {
  EXPECT_FALSE(pointer_event_factory_.IsActive(mapped_id_start_));
  EXPECT_FALSE(pointer_event_factory_.IsActive(mapped_id_start_ + 1));
  EXPECT_FALSE(pointer_event_factory_.IsActiveButtonsState(mapped_id_start_));
  EXPECT_FALSE(
      pointer_event_factory_.IsActiveButtonsState(mapped_id_start_ + 1));

  PointerEvent* pointer_event1 = CreateAndCheckWebPointerEvent(
      WebPointerProperties::PointerType::kTouch, 0, mapped_id_start_,
      true /* isprimary */, false /* hovering */);
  PointerEvent* pointer_event2 = CreateAndCheckWebPointerEvent(
      WebPointerProperties::PointerType::kTouch, 1, mapped_id_start_ + 1,
      false /* isprimary */, false /* hovering */);

  CreateAndCheckPointerTransitionEvent(pointer_event1,
                                       event_type_names::kPointerleave);
  CreateAndCheckPointerTransitionEvent(pointer_event2,
                                       event_type_names::kPointerenter);

  EXPECT_TRUE(pointer_event_factory_.IsActive(mapped_id_start_));
  EXPECT_TRUE(pointer_event_factory_.IsActive(mapped_id_start_ + 1));
  EXPECT_TRUE(pointer_event_factory_.IsActiveButtonsState(mapped_id_start_));
  EXPECT_TRUE(
      pointer_event_factory_.IsActiveButtonsState(mapped_id_start_ + 1));

  pointer_event_factory_.Remove(pointer_event1->pointerId());
  pointer_event_factory_.Remove(pointer_event2->pointerId());

  EXPECT_FALSE(pointer_event_factory_.IsActive(mapped_id_start_));
  EXPECT_FALSE(pointer_event_factory_.IsActive(mapped_id_start_ + 1));
  EXPECT_FALSE(pointer_event_factory_.IsActiveButtonsState(mapped_id_start_));
  EXPECT_FALSE(
      pointer_event_factory_.IsActiveButtonsState(mapped_id_start_ + 1));

  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kTouch, 1,
                                mapped_id_start_ + 2, true /* isprimary */,
                                false /* hovering */);
  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kTouch, 0,
                                mapped_id_start_ + 3, false /* isprimary */,
                                false /* hovering */);

  pointer_event_factory_.Clear();

  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kTouch, 10,
                                mapped_id_start_, true /* isprimary */,
                                false /* hovering */);
}

TEST_F(PointerEventFactoryTest, TouchAndDrag) {
  EXPECT_FALSE(pointer_event_factory_.IsActive(mapped_id_start_));
  EXPECT_FALSE(pointer_event_factory_.IsActiveButtonsState(mapped_id_start_));

  PointerEvent* pointer_event1 = CreateAndCheckWebPointerEvent(
      WebPointerProperties::PointerType::kTouch, 0, mapped_id_start_,
      true /* isprimary */, false /* hovering */);
  PointerEvent* pointer_event2 = CreateAndCheckWebPointerEvent(
      WebPointerProperties::PointerType::kTouch, 0, mapped_id_start_,
      true /* isprimary */, false /* hovering */);

  EXPECT_TRUE(pointer_event_factory_.IsActive(mapped_id_start_));
  EXPECT_TRUE(pointer_event_factory_.IsActiveButtonsState(mapped_id_start_));

  CreateAndCheckWebPointerEvent(
      WebPointerProperties::PointerType::kTouch, 0, mapped_id_start_,
      true /* isprimary */, false /* hovering */, WebInputEvent::kNoModifiers,
      WebInputEvent::Type::kPointerUp);

  EXPECT_TRUE(pointer_event_factory_.IsActive(mapped_id_start_));
  EXPECT_FALSE(pointer_event_factory_.IsActiveButtonsState(mapped_id_start_));

  pointer_event_factory_.Remove(pointer_event1->pointerId());
  pointer_event_factory_.Remove(pointer_event2->pointerId());

  EXPECT_FALSE(pointer_event_factory_.IsActive(mapped_id_start_));
  EXPECT_FALSE(pointer_event_factory_.IsActiveButtonsState(mapped_id_start_));

  EXPECT_FALSE(pointer_event_factory_.IsActive(mapped_id_start_ + 1));
  EXPECT_FALSE(
      pointer_event_factory_.IsActiveButtonsState(mapped_id_start_ + 1));

  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kTouch, 0,
                                mapped_id_start_ + 1, true /* isprimary */,
                                false /* hovering */);
  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kTouch, 0,
                                mapped_id_start_ + 1, true /* isprimary */,
                                false /* hovering */);

  // Remove an obsolete (i.e. already removed) pointer event which should have
  // no effect.
  pointer_event_factory_.Remove(pointer_event1->pointerId());

  EXPECT_TRUE(pointer_event_factory_.IsActive(mapped_id_start_ + 1));
  EXPECT_TRUE(
      pointer_event_factory_.IsActiveButtonsState(mapped_id_start_ + 1));

  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kTouch, 0,
                                mapped_id_start_ + 1, true /* isprimary */,
                                false /* hovering */);
  CreateAndCheckPointerCancel(WebPointerProperties::PointerType::kTouch, 0,
                              mapped_id_start_ + 1, true);

  EXPECT_TRUE(pointer_event_factory_.IsActive(mapped_id_start_ + 1));
  EXPECT_FALSE(
      pointer_event_factory_.IsActiveButtonsState(mapped_id_start_ + 1));

  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kTouch, 0,
                                mapped_id_start_ + 1, true /* isprimary */,
                                false /* hovering */);

  EXPECT_TRUE(pointer_event_factory_.IsActive(mapped_id_start_ + 1));
  EXPECT_TRUE(
      pointer_event_factory_.IsActiveButtonsState(mapped_id_start_ + 1));
}

TEST_F(PointerEventFactoryTest, MouseAndTouchAndPen) {
  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kMouse, 0,
                                expected_mouse_id_, true /* isprimary */,
                                true /* hovering */);
  PointerEvent* pointer_event1 = CreateAndCheckWebPointerEvent(
      WebPointerProperties::PointerType::kTouch, 0, mapped_id_start_,
      true /* isprimary */, false /* hovering */);
  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kPen, 0,
                                mapped_id_start_ + 1, true /* isprimary */,
                                false /* hovering */);

  PointerEvent* pointer_event2 = CreateAndCheckWebPointerEvent(
      WebPointerProperties::PointerType::kTouch, 1, mapped_id_start_ + 2,
      false /* isprimary */, false /* hovering */);
  PointerEvent* pointer_event3 = CreateAndCheckWebPointerEvent(
      WebPointerProperties::PointerType::kTouch, 2, mapped_id_start_ + 3,
      false /* isprimary */, false /* hovering */);
  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kPen, 0,
                                mapped_id_start_ + 1, true /* isprimary */,
                                false /* hovering */);
  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kPen, 47213,
                                mapped_id_start_ + 4, false /* isprimary */,
                                false /* hovering */);

  pointer_event_factory_.Remove(pointer_event1->pointerId());
  pointer_event_factory_.Remove(pointer_event2->pointerId());
  pointer_event_factory_.Remove(pointer_event3->pointerId());

  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kTouch, 100,
                                mapped_id_start_ + 5, true /* isprimary */,
                                false /* hovering */);

  pointer_event_factory_.Clear();

  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kMouse, 0,
                                expected_mouse_id_, true /* isprimary */,
                                true /* hovering */);
  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kTouch, 0,
                                mapped_id_start_, true /* isprimary */,
                                false /* hovering */);
  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kPen, 0,
                                mapped_id_start_ + 1, true /* isprimary */,
                                false /* hovering */);
}

TEST_F(PointerEventFactoryTest, NonHoveringPointers) {
  CheckNonHoveringPointers({});

  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kMouse, 0,
                                expected_mouse_id_, true /* isprimary */,
                                true /* hovering */);
  PointerEvent* pointer_event1 = CreateAndCheckWebPointerEvent(
      WebPointerProperties::PointerType::kPen, 0, mapped_id_start_,
      true /* isprimary */, true /* hovering */);
  CheckNonHoveringPointers({});

  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kPen, 0,
                                mapped_id_start_, true /* isprimary */,
                                false /* hovering */);
  CheckNonHoveringPointers({mapped_id_start_});

  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kTouch, 0,
                                mapped_id_start_ + 1, true /* isprimary */,
                                false /* hovering */);
  CheckNonHoveringPointers({mapped_id_start_, mapped_id_start_ + 1});

  pointer_event_factory_.Remove(pointer_event1->pointerId());
  CheckNonHoveringPointers({mapped_id_start_ + 1});

  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kTouch, 1,
                                mapped_id_start_ + 2, false /* isprimary */,
                                false /* hovering */);

  CheckNonHoveringPointers({mapped_id_start_ + 1, mapped_id_start_ + 2});

  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kTouch, 1,
                                mapped_id_start_ + 2, false /* isprimary */,
                                true /* hovering */);

  CheckNonHoveringPointers({mapped_id_start_ + 1});

  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kPen, 0,
                                mapped_id_start_ + 3, true /* isprimary */,
                                false /* hovering */);

  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kPen, 1,
                                mapped_id_start_ + 4, false /* isprimary */,
                                false /* hovering */);

  CheckNonHoveringPointers(
      {mapped_id_start_ + 1, mapped_id_start_ + 3, mapped_id_start_ + 4});

  pointer_event_factory_.Clear();
  CheckNonHoveringPointers({});
}

TEST_F(PointerEventFactoryTest, PenAsTouchAndMouseEvent) {
  PointerEvent* pointer_event1 = CreateAndCheckWebPointerEvent(
      WebPointerProperties::PointerType::kPen, 0, mapped_id_start_,
      true /* isprimary */, true /* hovering */);
  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kPen, 1,
                                mapped_id_start_ + 1, false /* isprimary */,
                                true /* hovering */);
  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kPen, 2,
                                mapped_id_start_ + 2, false /* isprimary */,
                                true /* hovering */);
  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kPen, 0,
                                mapped_id_start_, true /* isprimary */,
                                true /* hovering */);
  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kPen, 1,
                                mapped_id_start_ + 1, false /* isprimary */,
                                true /* hovering */);
  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kPen, 0,
                                mapped_id_start_, true /* isprimary */,
                                false /* hovering */);
  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kPen, 1,
                                mapped_id_start_ + 1, false /* isprimary */,
                                false /* hovering */);

  pointer_event_factory_.Remove(pointer_event1->pointerId());

  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kPen, 0,
                                mapped_id_start_ + 3, false /* isprimary */,
                                false /* hovering */);
  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kPen, 0,
                                mapped_id_start_ + 3, false /* isprimary */,
                                true /* hovering */);
  CreateAndCheckPointerCancel(WebPointerProperties::PointerType::kPen, 0,
                              mapped_id_start_ + 3, false);

  pointer_event_factory_.Clear();

  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kPen, 1,
                                mapped_id_start_, true /* isprimary */,
                                false /* hovering */);
  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kPen, 0,
                                mapped_id_start_ + 1, false /* isprimary */,
                                false /* hovering */);
  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kPen, 1,
                                mapped_id_start_, true /* isprimary */,
                                true /* hovering */);
  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kPen, 0,
                                mapped_id_start_ + 1, false /* isprimary */,
                                true /* hovering */);
  CreateAndCheckPointerCancel(WebPointerProperties::PointerType::kPen, 1,
                              mapped_id_start_, true);
  CreateAndCheckPointerCancel(WebPointerProperties::PointerType::kPen, 0,
                              mapped_id_start_ + 1, false);
}

TEST_F(PointerEventFactoryTest, OutOfRange) {
  PointerEvent* pointer_event1 = CreateAndCheckWebPointerEvent(
      WebPointerProperties::PointerType::kUnknown, 0, mapped_id_start_,
      true /* isprimary */, true /* hovering */);
  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kUnknown, 1,
                                mapped_id_start_ + 1, false /* isprimary */,
                                true /* hovering */);
  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kUnknown, 2,
                                mapped_id_start_ + 2, false /* isprimary */,
                                true /* hovering */);
  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kUnknown, 0,
                                mapped_id_start_, true /* isprimary */,
                                false /* hovering */);
  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kUnknown, 3,
                                mapped_id_start_ + 3, false /* isprimary */,
                                false /* hovering */);
  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kUnknown, 2,
                                mapped_id_start_ + 2, false /* isprimary */,
                                true /* hovering */);
  CreateAndCheckPointerCancel(WebPointerProperties::PointerType::kUnknown, 3,
                              mapped_id_start_ + 3, false);

  pointer_event_factory_.Remove(pointer_event1->pointerId());

  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kUnknown, 0,
                                mapped_id_start_ + 4, false /* isprimary */,
                                false /* hovering */);
  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kUnknown,
                                std::numeric_limits<int>::max(),
                                mapped_id_start_ + 5, false /* isprimary */,
                                false /* hovering */);

  pointer_event_factory_.Clear();

  for (int i = 0; i < 100; ++i) {
    CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kTouch, i,
                                  mapped_id_start_ + i, i == 0 /* isprimary */,
                                  true /* hovering */);
  }

  for (int i = 0; i < 100; ++i) {
    CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kMouse, i,
                                  expected_mouse_id_, true /* isprimary */,
                                  false /* hovering */);
  }
  CreateAndCheckPointerCancel(WebPointerProperties::PointerType::kMouse, 0,
                              expected_mouse_id_, true);

  EXPECT_EQ(pointer_event_factory_.IsActive(0), false);
  EXPECT_EQ(pointer_event_factory_.IsActive(-1), false);
  EXPECT_EQ(
      pointer_event_factory_.IsActive(std::numeric_limits<PointerId>::max()),
      false);
}

TEST_F(PointerEventFactoryTest, LastPointerPosition) {
  CreateAndCheckWebPointerEvent(
      WebPointerProperties::PointerType::kMouse, 0, expected_mouse_id_,
      true /* isprimary */, true /* hovering */, WebInputEvent::kNoModifiers,
      WebInputEvent::Type::kPointerMove,
      WebPointerProperties::Button::kNoButton, 4);
  pointer_event_factory_.RemoveLastPosition(expected_mouse_id_);
  EXPECT_EQ(
      pointer_event_factory_.GetLastPointerPosition(
          expected_mouse_id_,
          WebPointerProperties(1, WebPointerProperties::PointerType::kUnknown,
                               WebPointerProperties::Button::kNoButton,
                               gfx::PointF(50, 50), gfx::PointF(20, 20)),
          WebInputEvent::Type::kPointerMove),
      gfx::PointF(20, 20));
}

TEST_F(PointerEventFactoryTest, CoalescedEvents) {
  CreateAndCheckWebPointerEvent(
      WebPointerProperties::PointerType::kMouse, 0, expected_mouse_id_,
      true /* isprimary */, true /* hovering */, WebInputEvent::kNoModifiers,
      WebInputEvent::Type::kPointerMove,
      WebPointerProperties::Button::kNoButton, 4);
  CreateAndCheckWebPointerEvent(
      WebPointerProperties::PointerType::kTouch, 0, mapped_id_start_,
      true /* isprimary */, false /* hovering */, WebInputEvent::kNoModifiers,
      WebInputEvent::Type::kPointerMove,
      WebPointerProperties::Button::kNoButton, 3);
}

TEST_F(PointerEventFactoryTest, PredictedEvents) {
  CreateAndCheckWebPointerEvent(
      WebPointerProperties::PointerType::kMouse, 0, expected_mouse_id_,
      true /* isprimary */, true /* hovering */, WebInputEvent::kNoModifiers,
      WebInputEvent::Type::kPointerMove,
      WebPointerProperties::Button::kNoButton, 0 /* coalesced_count */,
      4 /* predicted_count */);
  CreateAndCheckWebPointerEvent(
      WebPointerProperties::PointerType::kTouch, 0, mapped_id_start_,
      true /* isprimary */, false /* hovering */, WebInputEvent::kNoModifiers,
      WebInputEvent::Type::kPointerMove,
      WebPointerProperties::Button::kNoButton, 0 /* coalesced_count */,
      3 /* predicted_count */);

  // Check predicted_event_count when type != kPointerMove
  CreateAndCheckWebPointerEvent(
      WebPointerProperties::PointerType::kMouse, 0, expected_mouse_id_,
      true /* isprimary */, true /* hovering */, WebInputEvent::kNoModifiers,
      WebInputEvent::Type::kPointerDown,
      WebPointerProperties::Button::kNoButton, 0 /* coalesced_count */,
      4 /* predicted_count */);
  CreateAndCheckWebPointerEvent(
      WebPointerProperties::PointerType::kTouch, 0, mapped_id_start_,
      true /* isprimary */, false /* hovering */, WebInputEvent::kNoModifiers,
      WebInputEvent::Type::kPointerUp, WebPointerProperties::Button::kNoButton,
      0 /* coalesced_count */, 3 /* predicted_count */);
}

TEST_F(PointerEventFactoryTest, MousePointerKeyStates) {
  WebInputEvent::Modifiers modifiers = static_cast<WebInputEvent::Modifiers>(
      WebInputEvent::kControlKey | WebInputEvent::kMetaKey);

  PointerEvent* pointer_event1 = CreateAndCheckWebPointerEvent(
      WebPointerProperties::PointerType::kMouse, 0, expected_mouse_id_,
      true /* isprimary */, true /* hovering */, modifiers,
      WebInputEvent::Type::kPointerMove);

  CreateAndCheckPointerTransitionEvent(pointer_event1,
                                       event_type_names::kPointerout);

  modifiers = static_cast<WebInputEvent::Modifiers>(WebInputEvent::kAltKey |
                                                    WebInputEvent::kShiftKey);
  PointerEvent* pointer_event2 = CreateAndCheckWebPointerEvent(
      WebPointerProperties::PointerType::kMouse, 0, expected_mouse_id_,
      true /* isprimary */, true /* hovering */, modifiers,
      WebInputEvent::Type::kPointerMove);

  CreateAndCheckPointerTransitionEvent(pointer_event2,
                                       event_type_names::kPointerover);
}

}  // namespace blink
