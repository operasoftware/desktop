// Copyright (C) 2022 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera

#include "third_party/blink/renderer/core/frame/opera_colors_client_impl.h"

#include "third_party/blink/renderer/core/css/document_style_environment_variables.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"

#include "third_party/blink/renderer/platform/graphics/color.h"

namespace blink {

OperaColorsClientImpl::OperaColorsClientImpl(
    LocalFrame* frame,
    mojo::PendingAssociatedReceiver<mojom::blink::OperaColorsClient> receiver)
    : frame_(frame), receiver_(this, std::move(receiver)) {}

void OperaColorsClientImpl::BindMojoReceiver(
    LocalFrame* frame,
    mojo::PendingAssociatedReceiver<mojom::blink::OperaColorsClient> receiver) {
  if (!frame)
    return;
  MakeGarbageCollected<OperaColorsClientImpl>(frame, std::move(receiver));
}

void OperaColorsClientImpl::SetColors(mojom::blink::OperaColorsPtr colors) {
  DocumentStyleEnvironmentVariables& vars =
      frame_->GetDocument()->GetStyleEngine().EnsureEnvironmentVariables();
  if (colors->opera_gx_accent) {
    Color gx_accent_color = Color::FromSkColor(colors->opera_gx_accent->color);
    double gx_accent_color_h, gx_accent_color_s, gx_accent_color_l;
    gx_accent_color.GetHSL(gx_accent_color_h, gx_accent_color_s,
                           gx_accent_color_l);
    vars.SetVariable(
        UADefinedVariable::kOperaGxAccentColor,
        WTF::String::Format("rgb(%d,%d,%d)", gx_accent_color.Red(),
                            gx_accent_color.Green(), gx_accent_color.Blue()));
    vars.SetVariable(UADefinedVariable::kOperaGxAccentColorH,
                     WTF::String::Format("%d", int(gx_accent_color_h * 360)));
    vars.SetVariable(UADefinedVariable::kOperaGxAccentColorS,
                     WTF::String::Format("%d%%", int(gx_accent_color_s * 100)));
    vars.SetVariable(UADefinedVariable::kOperaGxAccentColorL,
                     WTF::String::Format("%d%%", int(gx_accent_color_l * 100)));
    vars.SetVariable(UADefinedVariable::kOperaGxAccentColorR,
                     WTF::String::Format("%d", gx_accent_color.Red()));
    vars.SetVariable(UADefinedVariable::kOperaGxAccentColorG,
                     WTF::String::Format("%d", gx_accent_color.Green()));
    vars.SetVariable(UADefinedVariable::kOperaGxAccentColorB,
                     WTF::String::Format("%d", gx_accent_color.Blue()));
  }
  if (colors->opera_gx_base) {
    Color gx_background_color = Color::FromSkColor(colors->opera_gx_base->color);
    double gx_background_color_h, gx_background_color_s, gx_background_color_l;
    gx_background_color.GetHSL(gx_background_color_h, gx_background_color_s,
                               gx_background_color_l);
    vars.SetVariable(
        UADefinedVariable::kOperaGxBackgroundColor,
        WTF::String::Format("rgb(%d,%d,%d)", gx_background_color.Red(),
                            gx_background_color.Green(),
                            gx_background_color.Blue()));
    vars.SetVariable(UADefinedVariable::kOperaGxBackgroundColorH,
                    WTF::String::Format("%d", int(gx_background_color_h * 360)));
    vars.SetVariable(
        UADefinedVariable::kOperaGxBackgroundColorS,
        WTF::String::Format("%d%%", int(gx_background_color_s * 100)));
    vars.SetVariable(
        UADefinedVariable::kOperaGxBackgroundColorL,
        WTF::String::Format("%d%%", int(gx_background_color_l * 100)));
    vars.SetVariable(UADefinedVariable::kOperaGxBackgroundColorR,
                     WTF::String::Format("%d", gx_background_color.Red()));
    vars.SetVariable(UADefinedVariable::kOperaGxBackgroundColorG,
                     WTF::String::Format("%d", gx_background_color.Green()));
    vars.SetVariable(UADefinedVariable::kOperaGxBackgroundColorB,
                     WTF::String::Format("%d", gx_background_color.Blue()));
  }
}

void OperaColorsClientImpl::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
}

}  // namespace blink
