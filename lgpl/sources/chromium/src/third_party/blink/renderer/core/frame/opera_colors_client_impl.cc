// Copyright (C) 2022 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera

#include "third_party/blink/renderer/core/frame/opera_colors_client_impl.h"

#include <tuple>

#include "third_party/blink/renderer/core/css/document_style_environment_variables.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"

#include "third_party/blink/renderer/platform/graphics/color.h"

namespace blink {

namespace {
std::tuple<WTF::String /* full RGB color */,
           WTF::String /* h */,
           WTF::String /* s*/,
           WTF::String /* l */,
           WTF::String /* r */,
           WTF::String /* g */,
           WTF::String /* b*/>
GetColorVariables(Color color) {
  double color_h, color_s, color_l;
  color.GetHSL(color_h, color_s, color_l);

  const auto rgb = WTF::String::Format("rgb(%d,%d,%d)", color.Red(),
                                       color.Green(), color.Blue());
  const auto h = WTF::String::Format("%d", int(color_h * 360));
  const auto s = WTF::String::Format("%d%%", int(color_s * 100));
  const auto l = WTF::String::Format("%d%%", int(color_l * 100));
  const auto r = WTF::String::Format("%d", color.Red());
  const auto g = WTF::String::Format("%d", color.Green());
  const auto b = WTF::String::Format("%d", color.Blue());
  return {rgb, h, s, l, r, g, b};
}

}  // namespace

OperaColorsClientImpl::OperaColorsClientImpl(
    LocalFrame* frame,
    mojo::PendingAssociatedReceiver<mojom::blink::OperaColorsClient> receiver)
    : frame_(frame) {
  receiver_.Bind(std::move(receiver),
                 frame->GetTaskRunner(TaskType::kMiscPlatformAPI));
}

void OperaColorsClientImpl::BindMojoReceiver(
    LocalFrame* frame,
    mojo::PendingAssociatedReceiver<mojom::blink::OperaColorsClient> receiver) {
  if (!frame) {
    return;
  }
  MakeGarbageCollected<OperaColorsClientImpl>(frame, std::move(receiver));
}

void OperaColorsClientImpl::SetColors(mojom::blink::OperaColorsPtr colors) {
  DocumentStyleEnvironmentVariables& vars =
      frame_->GetDocument()->GetStyleEngine().EnsureEnvironmentVariables();
  if (colors->opera_accent) {
    WTF::String rgb, h, s, l, r, g, b;
    std::tie(rgb, h, s, l, r, g, b) =
        GetColorVariables(Color::FromSkColor(colors->opera_accent->color));
    vars.SetVariable(UADefinedVariable::kOperaAccentColor, rgb);
    vars.SetVariable(UADefinedVariable::kOperaAccentColorH, h);
    vars.SetVariable(UADefinedVariable::kOperaAccentColorS, s);
    vars.SetVariable(UADefinedVariable::kOperaAccentColorL, l);
    vars.SetVariable(UADefinedVariable::kOperaAccentColorR, r);
    vars.SetVariable(UADefinedVariable::kOperaAccentColorG, g);
    vars.SetVariable(UADefinedVariable::kOperaAccentColorB, b);
    // For backward compatibility
    vars.SetVariable(UADefinedVariable::kOperaGxAccentColor, rgb);
    vars.SetVariable(UADefinedVariable::kOperaGxAccentColorH, h);
    vars.SetVariable(UADefinedVariable::kOperaGxAccentColorS, s);
    vars.SetVariable(UADefinedVariable::kOperaGxAccentColorL, l);
    vars.SetVariable(UADefinedVariable::kOperaGxAccentColorR, r);
    vars.SetVariable(UADefinedVariable::kOperaGxAccentColorG, g);
    vars.SetVariable(UADefinedVariable::kOperaGxAccentColorB, b);
  }
  if (colors->opera_background) {
    WTF::String rgb, h, s, l, r, g, b;
    std::tie(rgb, h, s, l, r, g, b) =
        GetColorVariables(Color::FromSkColor(colors->opera_background->color));
    vars.SetVariable(UADefinedVariable::kOperaBackgroundColor, rgb);
    vars.SetVariable(UADefinedVariable::kOperaBackgroundColorH, h);
    vars.SetVariable(UADefinedVariable::kOperaBackgroundColorS, s);
    vars.SetVariable(UADefinedVariable::kOperaBackgroundColorL, l);
    vars.SetVariable(UADefinedVariable::kOperaBackgroundColorR, r);
    vars.SetVariable(UADefinedVariable::kOperaBackgroundColorG, g);
    vars.SetVariable(UADefinedVariable::kOperaBackgroundColorB, b);
    // For backward compatibility
    vars.SetVariable(UADefinedVariable::kOperaGxBackgroundColor, rgb);
    vars.SetVariable(UADefinedVariable::kOperaGxBackgroundColorH, h);
    vars.SetVariable(UADefinedVariable::kOperaGxBackgroundColorS, s);
    vars.SetVariable(UADefinedVariable::kOperaGxBackgroundColorL, l);
    vars.SetVariable(UADefinedVariable::kOperaGxBackgroundColorR, r);
    vars.SetVariable(UADefinedVariable::kOperaGxBackgroundColorG, g);
    vars.SetVariable(UADefinedVariable::kOperaGxBackgroundColorB, b);
  }
}

void OperaColorsClientImpl::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
  visitor->Trace(receiver_);
}

}  // namespace blink
