/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebSettingsImpl_h
#define WebSettingsImpl_h

#include "platform/heap/Handle.h"
#include "platform/wtf/Compiler.h"
#include "public/web/WebSettings.h"
#include "web/WebExport.h"

namespace blink {

class DevToolsEmulator;
class Settings;

class WEB_EXPORT WebSettingsImpl final : NON_EXPORTED_BASE(public WebSettings) {
 public:
  WebSettingsImpl(Settings*, DevToolsEmulator*);
  virtual ~WebSettingsImpl() {}

  void SetFromStrings(const WebString& name, const WebString& value) override;

  bool ShrinksViewportContentToFit() const override;
  bool ViewportEnabled() const override;
  void SetAccelerated2dCanvasMSAASampleCount(int) override;
  void SetAcceleratedCompositingEnabled(bool) override;
  void SetAutoplayPolicy(AutoplayPolicy) override;
  void SetPreferCompositingToLCDTextEnabled(bool) override;
  void SetAccessibilityEnabled(bool) override;
  void SetAccessibilityPasswordValuesEnabled(bool) override;
  void SetAllowFileAccessFromFileURLs(bool) override;
  void SetAllowCustomScrollbarInMainFrame(bool) override;
  void SetAllowGeolocationOnInsecureOrigins(bool) override;
  void SetAllowRunningOfInsecureContent(bool) override;
  void SetAllowScriptsToCloseWindows(bool) override;
  void SetAllowVideoDetach(bool) override;
  void SetAllowUniversalAccessFromFileURLs(bool) override;
  void SetAlwaysShowContextMenuOnTouch(bool) override;
  void SetAntialiased2dCanvasEnabled(bool) override;
  void SetAntialiasedClips2dCanvasEnabled(bool) override;
  void SetAutoZoomFocusedNodeToLegibleScale(bool) override;
  void SetBrowserSideNavigationEnabled(bool) override;
  void SetClobberUserAgentInitialScaleQuirk(bool) override;
  void SetCookieEnabled(bool) override;
  void SetNavigateOnDragDrop(bool) override;
  void SetCursiveFontFamily(const WebString&,
                            UScriptCode = USCRIPT_COMMON) override;
  void SetDNSPrefetchingEnabled(bool) override;
  void SetDataSaverEnabled(bool) override;
  void SetDOMPasteAllowed(bool) override;
  void SetDefaultFixedFontSize(int) override;
  void SetDefaultFontSize(int) override;
  void SetDefaultTextEncodingName(const WebString&) override;
  void SetDefaultVideoPosterURL(const WebString&) override;
  void SetDeviceScaleAdjustment(float) override;

  void SetDisableReadingFromCanvas(bool) override;
  void SetDoubleTapToZoomEnabled(bool) override;
  void SetDownloadableBinaryFontsEnabled(bool) override;
  void SetEditingBehavior(EditingBehavior) override;
  void SetEnableScrollAnimator(bool) override;
  void SetEnableTouchAdjustment(bool) override;
  bool MultiTargetTapNotificationEnabled() override;
  void SetMultiTargetTapNotificationEnabled(bool) override;
  void SetExperimentalWebGLEnabled(bool) override;
  void SetFantasyFontFamily(const WebString&,
                            UScriptCode = USCRIPT_COMMON) override;
  void SetFixedFontFamily(const WebString&,
                          UScriptCode = USCRIPT_COMMON) override;
  void SetForcePreloadNoneForMediaElements(bool) override;
  void SetForceZeroLayoutHeight(bool) override;
  void SetFullscreenSupported(bool) override;
  void SetHideDownloadUI(bool) override;
  void SetPresentationReceiver(bool) override;
  void SetHistoryEntryRequiresUserGesture(bool) override;
  void SetHyperlinkAuditingEnabled(bool) override;
  void SetIgnoreMainFrameOverflowHiddenQuirk(bool) override;
  void SetImageAnimationPolicy(ImageAnimationPolicy) override;
  void SetImagesEnabled(bool) override;
  void SetInlineTextBoxAccessibilityEnabled(bool) override;
  void SetInertVisualViewport(bool) override;
  void SetJavaScriptCanAccessClipboard(bool) override;
  void SetJavaScriptEnabled(bool) override;
  void SetLoadsImagesAutomatically(bool) override;
  void SetLoadWithOverviewMode(bool) override;
  void SetShouldReuseGlobalForUnownedMainFrame(bool) override;
  void SetProgressBarCompletion(ProgressBarCompletion) override;
  void SetLocalStorageEnabled(bool) override;
  void SetMainFrameClipsContent(bool) override;
  void SetMainFrameResizesAreOrientationChanges(bool) override;
  void SetMaxTouchPoints(int) override;
  void SetMediaPlaybackGestureWhitelistScope(const WebString&) override;
  void SetMediaEnabled(bool) override;
  void SetPresentationRequiresUserGesture(bool) override;
  void SetEmbeddedMediaExperienceEnabled(bool) override;
  void SetPagePopupsSuppressed(bool) override;
  void SetMinimumAccelerated2dCanvasSize(int) override;
  void SetMinimumFontSize(int) override;
  void SetMinimumLogicalFontSize(int) override;
  void SetMockScrollbarsEnabled(bool) override;
  void SetHideScrollbars(bool) override;
  void SetOfflineWebApplicationCacheEnabled(bool) override;
  void SetPassiveEventListenerDefault(PassiveEventListenerDefault) override;
  void SetPasswordEchoDurationInSeconds(double) override;
  void SetPasswordEchoEnabled(bool) override;
  void SetPerTilePaintingEnabled(bool) override;
  void SetPictographFontFamily(const WebString&,
                               UScriptCode = USCRIPT_COMMON) override;
  void SetPluginsEnabled(bool) override;
  void SetEncryptedMediaEnabled(bool) override;
  void SetAvailablePointerTypes(int) override;
  void SetPrimaryPointerType(PointerType) override;
  void SetAvailableHoverTypes(int) override;
  void SetPrimaryHoverType(HoverType) override;
  void SetPreferHiddenVolumeControls(bool) override;
  void SetRenderVSyncNotificationEnabled(bool) override;
  void SetReportScreenSizeInPhysicalPixelsQuirk(bool) override;
  void SetRubberBandingOnCompositorThread(bool) override;
  void SetSansSerifFontFamily(const WebString&,
                              UScriptCode = USCRIPT_COMMON) override;
  void SetSelectTrailingWhitespaceEnabled(bool override);
  void SetSelectionIncludesAltImageText(bool) override;
  void SetSelectionStrategy(SelectionStrategyType) override;
  void SetSerifFontFamily(const WebString&,
                          UScriptCode = USCRIPT_COMMON) override;
  void SetShouldPrintBackgrounds(bool) override;
  void SetShouldClearDocumentBackground(bool) override;
  void SetShouldRespectImageOrientation(bool) override;
  void SetShowContextMenuOnMouseUp(bool) override;
  void SetShowFPSCounter(bool) override;
  void SetShowPaintRects(bool) override;
  void SetShrinksViewportContentToFit(bool) override;
  void SetSmartInsertDeleteEnabled(bool) override;
  void SetSpatialNavigationEnabled(bool) override;
  void SetSpellCheckEnabledByDefault(bool) override;
  void SetStandardFontFamily(const WebString&,
                             UScriptCode = USCRIPT_COMMON) override;
  void SetStrictMixedContentChecking(bool) override;
  void SetStrictMixedContentCheckingForPlugin(bool) override;
  void SetStrictPowerfulFeatureRestrictions(bool) override;
  void SetStrictlyBlockBlockableMixedContent(bool) override;
  void SetSupportDeprecatedTargetDensityDPI(bool) override;
  void SetSupportsMultipleWindows(bool) override;
  void SetSyncXHRInDocumentsEnabled(bool) override;
  void SetTextAreasAreResizable(bool) override;
  void SetTextAutosizingEnabled(bool) override;
  void SetAccessibilityFontScaleFactor(float) override;
  void SetTextTrackKindUserPreference(TextTrackKindUserPreference) override;
  void SetTextTrackBackgroundColor(const WebString&) override;
  void SetTextTrackFontFamily(const WebString&) override;
  void SetTextTrackFontStyle(const WebString&) override;
  void SetTextTrackFontVariant(const WebString&) override;
  void SetTextTrackMarginPercentage(float) override;
  void SetTextTrackTextColor(const WebString&) override;
  void SetTextTrackTextShadow(const WebString&) override;
  void SetTextTrackTextSize(const WebString&) override;
  void SetThreadedScrollingEnabled(bool) override;
  void SetTouchDragDropEnabled(bool) override;
  void SetUseLegacyBackgroundSizeShorthandBehavior(bool) override;
  void SetViewportStyle(WebViewportStyle) override;
  void SetUseSolidColorScrollbars(bool) override;
  void SetUseWideViewport(bool) override;
  void SetV8CacheOptions(V8CacheOptions) override;
  void SetV8CacheStrategiesForCacheStorage(
      V8CacheStrategiesForCacheStorage) override;
  void SetValidationMessageTimerMagnification(int) override;
  void SetViewportEnabled(bool) override;
  void SetViewportMetaEnabled(bool) override;
  void SetViewportMetaLayoutSizeQuirk(bool) override;
  void SetViewportMetaMergeContentQuirk(bool) override;
  void SetViewportMetaNonUserScalableQuirk(bool) override;
  void SetViewportMetaZeroValuesQuirk(bool) override;
  void SetWebGLErrorsToConsoleEnabled(bool) override;
  void SetWebSecurityEnabled(bool) override;
  void SetWideViewportQuirkEnabled(bool) override;
  void SetXSSAuditorEnabled(bool) override;
  void SetExpensiveBackgroundThrottlingCPUBudget(float) override;
  void SetExpensiveBackgroundThrottlingInitialBudget(float) override;
  void SetExpensiveBackgroundThrottlingMaxBudget(float) override;
  void SetExpensiveBackgroundThrottlingMaxDelay(float) override;
  void SetMediaControlsEnabled(bool) override;
  void SetDoNotUpdateSelectionOnMutatingSelectionRange(bool) override;

  bool ShowFPSCounter() const { return show_fps_counter_; }
  bool ShowPaintRects() const { return show_paint_rects_; }
  bool RenderVSyncNotificationEnabled() const {
    return render_v_sync_notification_enabled_;
  }
  bool AutoZoomFocusedNodeToLegibleScale() const {
    return auto_zoom_focused_node_to_legible_scale_;
  }
  bool DoubleTapToZoomEnabled() const;
  bool PerTilePaintingEnabled() const { return per_tile_painting_enabled_; }
  bool SupportDeprecatedTargetDensityDPI() const {
    return support_deprecated_target_density_dpi_;
  }
  bool ViewportMetaEnabled() const;
  bool ViewportMetaLayoutSizeQuirk() const {
    return viewport_meta_layout_size_quirk_;
  }
  bool ViewportMetaNonUserScalableQuirk() const {
    return viewport_meta_non_user_scalable_quirk_;
  }
  bool ClobberUserAgentInitialScaleQuirk() const {
    return clobber_user_agent_initial_scale_quirk_;
  }
  float ExpensiveBackgroundThrottlingCPUBudget() const {
    return expensive_background_throttling_cpu_budget_;
  }
  float ExpensiveBackgroundThrottlingInitialBudget() const {
    return expensive_background_throttling_initial_budget_;
  }
  float ExpensiveBackgroundThrottlingMaxDelay() const {
    return expensive_background_throttling_max_delay_;
  }
  float ExpensiveBackgroundThrottlingMaxBudget() const {
    return expensive_background_throttling_max_budget_;
  }

  void SetMockGestureTapHighlightsEnabled(bool);
  bool MockGestureTapHighlightsEnabled() const;

 private:
  Settings* settings_;
  UntracedMember<DevToolsEmulator> dev_tools_emulator_;
  bool show_fps_counter_;
  bool show_paint_rects_;
  bool render_v_sync_notification_enabled_;
  bool auto_zoom_focused_node_to_legible_scale_;
  bool per_tile_painting_enabled_;
  bool support_deprecated_target_density_dpi_;
  bool shrinks_viewport_content_to_fit_;
  // This quirk is to maintain compatibility with Android apps built on
  // the Android SDK prior to and including version 18. Presumably, this
  // can be removed any time after 2015. See http://crbug.com/277369.
  bool viewport_meta_layout_size_quirk_;
  // This quirk is to maintain compatibility with Android apps built on
  // the Android SDK prior to and including version 18. Presumably, this
  // can be removed any time after 2015. See http://crbug.com/312691.
  bool viewport_meta_non_user_scalable_quirk_;
  // This quirk is to maintain compatibility with Android apps built on
  // the Android SDK prior to and including version 18. Presumably, this
  // can be removed any time after 2015. See http://crbug.com/313754.
  bool clobber_user_agent_initial_scale_quirk_;
  float expensive_background_throttling_cpu_budget_;
  float expensive_background_throttling_initial_budget_;
  float expensive_background_throttling_max_budget_;
  float expensive_background_throttling_max_delay_;
};

}  // namespace blink

#endif
