// Copyright (C) 2022 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_OPERA_COLORS_CLIENT_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_OPERA_COLORS_CLIENT_IMPL_H_

#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/mojom/frame/opera_colors_client.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class LocalFrame;

class CORE_EXPORT OperaColorsClientImpl final
    : public GarbageCollected<OperaColorsClientImpl>,
      public mojom::blink::OperaColorsClient {
 public:
  static void BindMojoReceiver(
      LocalFrame*,
      mojo::PendingAssociatedReceiver<mojom::blink::OperaColorsClient>);

  OperaColorsClientImpl(
      LocalFrame*,
      mojo::PendingAssociatedReceiver<mojom::blink::OperaColorsClient>);
  OperaColorsClientImpl(const OperaColorsClientImpl&) = delete;
  OperaColorsClientImpl& operator=(const OperaColorsClientImpl&) = delete;

  // Notify the renderer that the safe areas have changed.
  void SetColors(mojom::blink::OperaColorsPtr colors) override;

  void Trace(Visitor*) const;

 private:
  Member<LocalFrame> frame_;

  mojo::AssociatedReceiver<mojom::blink::OperaColorsClient> receiver_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_OPERA_COLORS_CLIENT_IMPL_H_
