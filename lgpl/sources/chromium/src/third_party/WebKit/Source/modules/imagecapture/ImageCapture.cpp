// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/imagecapture/ImageCapture.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/fileapi/Blob.h"
#include "core/frame/ImageBitmap.h"
#include "modules/EventTargetModules.h"
#include "modules/imagecapture/MediaSettingsRange.h"
#include "modules/imagecapture/PhotoCapabilities.h"
#include "modules/imagecapture/PhotoSettings.h"
#include "modules/mediastream/MediaStreamTrack.h"
#include "modules/mediastream/MediaTrackCapabilities.h"
#include "modules/mediastream/MediaTrackConstraints.h"
#include "platform/WaitableEvent.h"
#include "platform/mojo/MojoHelper.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"
#include "public/platform/WebImageCaptureFrameGrabber.h"
#include "public/platform/WebMediaStreamTrack.h"

namespace blink {

using FillLightMode = media::mojom::blink::FillLightMode;
using MeteringMode = media::mojom::blink::MeteringMode;

namespace {

const char kNoServiceError[] = "ImageCapture service unavailable.";

bool TrackIsInactive(const MediaStreamTrack& track) {
  // Spec instructs to return an exception if the Track's readyState() is not
  // "live". Also reject if the track is disabled or muted.
  return track.readyState() != "live" || !track.enabled() || track.muted();
}

MeteringMode ParseMeteringMode(const String& blink_mode) {
  if (blink_mode == "manual")
    return MeteringMode::MANUAL;
  if (blink_mode == "single-shot")
    return MeteringMode::SINGLE_SHOT;
  if (blink_mode == "continuous")
    return MeteringMode::CONTINUOUS;
  if (blink_mode == "none")
    return MeteringMode::NONE;
  NOTREACHED();
  return MeteringMode::NONE;
}

FillLightMode ParseFillLightMode(const String& blink_mode) {
  if (blink_mode == "off")
    return FillLightMode::OFF;
  if (blink_mode == "auto")
    return FillLightMode::AUTO;
  if (blink_mode == "flash")
    return FillLightMode::FLASH;
  NOTREACHED();
  return FillLightMode::OFF;
}

WebString ToString(MeteringMode value) {
  switch (value) {
    case MeteringMode::NONE:
      return WebString::FromUTF8("none");
    case MeteringMode::MANUAL:
      return WebString::FromUTF8("manual");
    case MeteringMode::SINGLE_SHOT:
      return WebString::FromUTF8("single-shot");
    case MeteringMode::CONTINUOUS:
      return WebString::FromUTF8("continuous");
    default:
      NOTREACHED() << "Unknown MeteringMode";
  }
  return WebString();
}

}  // anonymous namespace

ImageCapture* ImageCapture::Create(ExecutionContext* context,
                                   MediaStreamTrack* track,
                                   ExceptionState& exception_state) {
  if (track->kind() != "video") {
    exception_state.ThrowDOMException(
        kNotSupportedError,
        "Cannot create an ImageCapturer from a non-video Track.");
    return nullptr;
  }

  return new ImageCapture(context, track);
}

ImageCapture::~ImageCapture() {
  DCHECK(!HasEventListeners());
  // There should be no more outstanding |m_serviceRequests| at this point
  // since each of them holds a persistent handle to this object.
  DCHECK(service_requests_.IsEmpty());
}

const AtomicString& ImageCapture::InterfaceName() const {
  return EventTargetNames::ImageCapture;
}

ExecutionContext* ImageCapture::GetExecutionContext() const {
  return ContextLifecycleObserver::GetExecutionContext();
}

bool ImageCapture::HasPendingActivity() const {
  return GetExecutionContext() && HasEventListeners();
}

void ImageCapture::ContextDestroyed(ExecutionContext*) {
  RemoveAllEventListeners();
  service_requests_.clear();
  DCHECK(!HasEventListeners());
}

ScriptPromise ImageCapture::getPhotoCapabilities(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  if (!service_) {
    resolver->Reject(DOMException::Create(kNotFoundError, kNoServiceError));
    return promise;
  }
  service_requests_.insert(resolver);

  // m_streamTrack->component()->source()->id() is the renderer "name" of the
  // camera;
  // TODO(mcasas) consider sending the security origin as well:
  // scriptState->getExecutionContext()->getSecurityOrigin()->toString()
  service_->GetCapabilities(
      stream_track_->Component()->Source()->Id(),
      ConvertToBaseCallback(WTF::Bind(
          &ImageCapture::OnMojoPhotoCapabilities, WrapPersistent(this),
          WrapPersistent(resolver), false /* trigger_take_photo */)));
  return promise;
}

ScriptPromise ImageCapture::setOptions(ScriptState* script_state,
                                       const PhotoSettings& photo_settings,
                                       bool trigger_take_photo /* = false */) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  if (TrackIsInactive(*stream_track_)) {
    resolver->Reject(DOMException::Create(
        kInvalidStateError, "The associated Track is in an invalid state."));
    return promise;
  }

  if (!service_) {
    resolver->Reject(DOMException::Create(kNotFoundError, kNoServiceError));
    return promise;
  }
  service_requests_.insert(resolver);

  // TODO(mcasas): should be using a mojo::StructTraits instead.
  auto settings = media::mojom::blink::PhotoSettings::New();

  settings->has_height = photo_settings.hasImageHeight();
  if (settings->has_height) {
    const double height = photo_settings.imageHeight();
    if (photo_capabilities_ &&
        (height < photo_capabilities_->imageHeight()->min() ||
         height > photo_capabilities_->imageHeight()->max())) {
      resolver->Reject(DOMException::Create(
          kNotSupportedError, "imageHeight setting out of range"));
      return promise;
    }
    settings->height = height;
  }
  settings->has_width = photo_settings.hasImageWidth();
  if (settings->has_width) {
    const double width = photo_settings.imageWidth();
    if (photo_capabilities_ &&
        (width < photo_capabilities_->imageWidth()->min() ||
         width > photo_capabilities_->imageWidth()->max())) {
      resolver->Reject(DOMException::Create(kNotSupportedError,
                                            "imageWidth setting out of range"));
      return promise;
    }
    settings->width = width;
  }

  settings->has_red_eye_reduction = photo_settings.hasRedEyeReduction();
  if (settings->has_red_eye_reduction) {
    if (photo_capabilities_ &&
        !photo_capabilities_->IsRedEyeReductionControllable()) {
      resolver->Reject(DOMException::Create(
          kNotSupportedError, "redEyeReduction is not controllable."));
      return promise;
    }
    settings->red_eye_reduction = photo_settings.redEyeReduction();
  }

  settings->has_fill_light_mode = photo_settings.hasFillLightMode();
  if (settings->has_fill_light_mode) {
    const String fill_light_mode = photo_settings.fillLightMode();
    if (photo_capabilities_ && photo_capabilities_->fillLightMode().Find(
                                   fill_light_mode) == kNotFound) {
      resolver->Reject(DOMException::Create(kNotSupportedError,
                                            "Unsupported fillLightMode"));
      return promise;
    }
    settings->fill_light_mode = ParseFillLightMode(fill_light_mode);
  }

  service_->SetOptions(
      stream_track_->Component()->Source()->Id(), std::move(settings),
      ConvertToBaseCallback(
          WTF::Bind(&ImageCapture::OnMojoSetOptions, WrapPersistent(this),
                    WrapPersistent(resolver), trigger_take_photo)));
  return promise;
}

ScriptPromise ImageCapture::takePhoto(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  if (TrackIsInactive(*stream_track_)) {
    resolver->Reject(DOMException::Create(
        kInvalidStateError, "The associated Track is in an invalid state."));
    return promise;
  }
  if (!service_) {
    resolver->Reject(DOMException::Create(kNotFoundError, kNoServiceError));
    return promise;
  }

  service_requests_.insert(resolver);

  // m_streamTrack->component()->source()->id() is the renderer "name" of the
  // camera;
  // TODO(mcasas) consider sending the security origin as well:
  // scriptState->getExecutionContext()->getSecurityOrigin()->toString()
  service_->TakePhoto(stream_track_->Component()->Source()->Id(),
                      ConvertToBaseCallback(WTF::Bind(
                          &ImageCapture::OnMojoTakePhoto, WrapPersistent(this),
                          WrapPersistent(resolver))));
  return promise;
}

ScriptPromise ImageCapture::takePhoto(ScriptState* script_state,
                                      const PhotoSettings& photo_settings) {
  return setOptions(script_state, photo_settings,
                    true /* trigger_take_photo */);
}

ScriptPromise ImageCapture::grabFrame(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  if (TrackIsInactive(*stream_track_)) {
    resolver->Reject(DOMException::Create(
        kInvalidStateError, "The associated Track is in an invalid state."));
    return promise;
  }

  // Create |m_frameGrabber| the first time.
  if (!frame_grabber_) {
    frame_grabber_ = Platform::Current()->CreateImageCaptureFrameGrabber();
  }

  if (!frame_grabber_) {
    resolver->Reject(DOMException::Create(
        kUnknownError, "Couldn't create platform resources"));
    return promise;
  }

  // The platform does not know about MediaStreamTrack, so we wrap it up.
  WebMediaStreamTrack track(stream_track_->Component());
  frame_grabber_->GrabFrame(
      &track, new CallbackPromiseAdapter<ImageBitmap, void>(resolver));

  return promise;
}

MediaTrackCapabilities& ImageCapture::GetMediaTrackCapabilities() {
  return capabilities_;
}

// TODO(mcasas): make the implementation fully Spec compliant, see the TODOs
// inside the method, https://crbug.com/708723.
void ImageCapture::SetMediaTrackConstraints(
    ScriptPromiseResolver* resolver,
    const HeapVector<MediaTrackConstraintSet>& constraints_vector) {
  if (!service_) {
    resolver->Reject(DOMException::Create(kNotFoundError, kNoServiceError));
    return;
  }
  service_requests_.insert(resolver);

  // TODO(mcasas): add support more than one single advanced constraint.
  const auto constraints = constraints_vector[0];

  if ((constraints.hasWhiteBalanceMode() &&
       !capabilities_.hasWhiteBalanceMode()) ||
      (constraints.hasExposureMode() && !capabilities_.hasExposureMode()) ||
      (constraints.hasFocusMode() && !capabilities_.hasFocusMode()) ||
      (constraints.hasExposureCompensation() &&
       !capabilities_.hasExposureCompensation()) ||
      (constraints.hasColorTemperature() &&
       !capabilities_.hasColorTemperature()) ||
      (constraints.hasIso() && !capabilities_.hasIso()) ||
      (constraints.hasBrightness() && !capabilities_.hasBrightness()) ||
      (constraints.hasContrast() && !capabilities_.hasContrast()) ||
      (constraints.hasSaturation() && !capabilities_.hasSaturation()) ||
      (constraints.hasSharpness() && !capabilities_.hasSharpness()) ||
      (constraints.hasZoom() && !capabilities_.hasZoom()) ||
      (constraints.hasTorch() && !capabilities_.hasTorch())) {
    resolver->Reject(
        DOMException::Create(kNotSupportedError, "Unsupported constraint(s)"));
    return;
  }

  auto settings = media::mojom::blink::PhotoSettings::New();
  MediaTrackConstraintSet temp_constraints = current_constraints_;

  // TODO(mcasas): support other Mode types beyond simple string i.e. the
  // equivalents of "sequence<DOMString>"" or "ConstrainDOMStringParameters".
  settings->has_white_balance_mode = constraints.hasWhiteBalanceMode() &&
                                     constraints.whiteBalanceMode().isString();
  if (settings->has_white_balance_mode) {
    const auto white_balance_mode =
        constraints.whiteBalanceMode().getAsString();
    if (capabilities_.whiteBalanceMode().Find(white_balance_mode) ==
        kNotFound) {
      resolver->Reject(DOMException::Create(kNotSupportedError,
                                            "Unsupported whiteBalanceMode."));
      return;
    }
    temp_constraints.setWhiteBalanceMode(constraints.whiteBalanceMode());
    settings->white_balance_mode = ParseMeteringMode(white_balance_mode);
  }
  settings->has_exposure_mode =
      constraints.hasExposureMode() && constraints.exposureMode().isString();
  if (settings->has_exposure_mode) {
    const auto exposure_mode = constraints.exposureMode().getAsString();
    if (capabilities_.exposureMode().Find(exposure_mode) == kNotFound) {
      resolver->Reject(DOMException::Create(kNotSupportedError,
                                            "Unsupported exposureMode."));
      return;
    }
    temp_constraints.setExposureMode(constraints.exposureMode());
    settings->exposure_mode = ParseMeteringMode(exposure_mode);
  }

  settings->has_focus_mode =
      constraints.hasFocusMode() && constraints.focusMode().isString();
  if (settings->has_focus_mode) {
    const auto focus_mode = constraints.focusMode().getAsString();
    if (capabilities_.focusMode().Find(focus_mode) == kNotFound) {
      resolver->Reject(
          DOMException::Create(kNotSupportedError, "Unsupported focusMode."));
      return;
    }
    temp_constraints.setFocusMode(constraints.focusMode());
    settings->focus_mode = ParseMeteringMode(focus_mode);
  }

  // TODO(mcasas): support ConstrainPoint2DParameters.
  if (constraints.hasPointsOfInterest() &&
      constraints.pointsOfInterest().isPoint2DSequence()) {
    for (const auto& point :
         constraints.pointsOfInterest().getAsPoint2DSequence()) {
      auto mojo_point = media::mojom::blink::Point2D::New();
      mojo_point->x = point.x();
      mojo_point->y = point.y();
      settings->points_of_interest.push_back(std::move(mojo_point));
    }
    temp_constraints.setPointsOfInterest(constraints.pointsOfInterest());
  }

  // TODO(mcasas): support ConstrainDoubleRange where applicable.
  settings->has_exposure_compensation =
      constraints.hasExposureCompensation() &&
      constraints.exposureCompensation().isDouble();
  if (settings->has_exposure_compensation) {
    const auto exposure_compensation =
        constraints.exposureCompensation().getAsDouble();
    if (exposure_compensation < capabilities_.exposureCompensation()->min() ||
        exposure_compensation > capabilities_.exposureCompensation()->max()) {
      resolver->Reject(DOMException::Create(
          kNotSupportedError, "exposureCompensation setting out of range"));
      return;
    }
    temp_constraints.setExposureCompensation(
        constraints.exposureCompensation());
    settings->exposure_compensation = exposure_compensation;
  }
  settings->has_color_temperature = constraints.hasColorTemperature() &&
                                    constraints.colorTemperature().isDouble();
  if (settings->has_color_temperature) {
    const auto color_temperature = constraints.colorTemperature().getAsDouble();
    if (color_temperature < capabilities_.colorTemperature()->min() ||
        color_temperature > capabilities_.colorTemperature()->max()) {
      resolver->Reject(DOMException::Create(
          kNotSupportedError, "colorTemperature setting out of range"));
      return;
    }
    temp_constraints.setColorTemperature(constraints.colorTemperature());
    settings->color_temperature = color_temperature;
  }
  settings->has_iso = constraints.hasIso() && constraints.iso().isDouble();
  if (settings->has_iso) {
    const auto iso = constraints.iso().getAsDouble();
    if (iso < capabilities_.iso()->min() || iso > capabilities_.iso()->max()) {
      resolver->Reject(
          DOMException::Create(kNotSupportedError, "iso setting out of range"));
      return;
    }
    temp_constraints.setIso(constraints.iso());
    settings->iso = iso;
  }

  settings->has_brightness =
      constraints.hasBrightness() && constraints.brightness().isDouble();
  if (settings->has_brightness) {
    const auto brightness = constraints.brightness().getAsDouble();
    if (brightness < capabilities_.brightness()->min() ||
        brightness > capabilities_.brightness()->max()) {
      resolver->Reject(DOMException::Create(kNotSupportedError,
                                            "brightness setting out of range"));
      return;
    }
    temp_constraints.setBrightness(constraints.brightness());
    settings->brightness = brightness;
  }
  settings->has_contrast =
      constraints.hasContrast() && constraints.contrast().isDouble();
  if (settings->has_contrast) {
    const auto contrast = constraints.contrast().getAsDouble();
    if (contrast < capabilities_.contrast()->min() ||
        contrast > capabilities_.contrast()->max()) {
      resolver->Reject(DOMException::Create(kNotSupportedError,
                                            "contrast setting out of range"));
      return;
    }
    temp_constraints.setContrast(constraints.contrast());
    settings->contrast = contrast;
  }
  settings->has_saturation =
      constraints.hasSaturation() && constraints.saturation().isDouble();
  if (settings->has_saturation) {
    const auto saturation = constraints.saturation().getAsDouble();
    if (saturation < capabilities_.saturation()->min() ||
        saturation > capabilities_.saturation()->max()) {
      resolver->Reject(DOMException::Create(kNotSupportedError,
                                            "saturation setting out of range"));
      return;
    }
    temp_constraints.setSaturation(constraints.saturation());
    settings->saturation = saturation;
  }
  settings->has_sharpness =
      constraints.hasSharpness() && constraints.sharpness().isDouble();
  if (settings->has_sharpness) {
    const auto sharpness = constraints.sharpness().getAsDouble();
    if (sharpness < capabilities_.sharpness()->min() ||
        sharpness > capabilities_.sharpness()->max()) {
      resolver->Reject(DOMException::Create(kNotSupportedError,
                                            "sharpness setting out of range"));
      return;
    }
    temp_constraints.setSharpness(constraints.sharpness());
    settings->sharpness = sharpness;
  }

  settings->has_zoom = constraints.hasZoom() && constraints.zoom().isDouble();
  if (settings->has_zoom) {
    const auto zoom = constraints.zoom().getAsDouble();
    if (zoom < capabilities_.zoom()->min() ||
        zoom > capabilities_.zoom()->max()) {
      resolver->Reject(DOMException::Create(kNotSupportedError,
                                            "zoom setting out of range"));
      return;
    }
    temp_constraints.setZoom(constraints.zoom());
    settings->zoom = zoom;
  }

  // TODO(mcasas): support ConstrainBooleanParameters where applicable.
  settings->has_torch =
      constraints.hasTorch() && constraints.torch().isBoolean();
  if (settings->has_torch) {
    const auto torch = constraints.torch().getAsBoolean();
    if (torch && !capabilities_.torch()) {
      resolver->Reject(
          DOMException::Create(kNotSupportedError, "torch not supported"));
      return;
    }
    temp_constraints.setTorch(constraints.torch());
    settings->torch = torch;
  }

  current_constraints_ = temp_constraints;

  service_->SetOptions(
      stream_track_->Component()->Source()->Id(), std::move(settings),
      ConvertToBaseCallback(
          WTF::Bind(&ImageCapture::OnMojoSetOptions, WrapPersistent(this),
                    WrapPersistent(resolver), false /* trigger_take_photo */)));
}

const MediaTrackConstraintSet& ImageCapture::GetMediaTrackConstraints() const {
  return current_constraints_;
}

void ImageCapture::ClearMediaTrackConstraints(ScriptPromiseResolver* resolver) {
  current_constraints_ = MediaTrackConstraintSet();
  resolver->Resolve();

  // TODO(mcasas): Clear also any PhotoSettings that the device might have got
  // configured, for that we need to know a "default" state of the device; take
  // a snapshot upon first opening. https://crbug.com/700607.
}

void ImageCapture::GetMediaTrackSettings(MediaTrackSettings& settings) const {
  // Merge any present |settings_| members into |settings|.

  if (settings_.hasWhiteBalanceMode())
    settings.setWhiteBalanceMode(settings_.whiteBalanceMode());
  if (settings_.hasExposureMode())
    settings.setExposureMode(settings_.exposureMode());
  if (settings_.hasFocusMode())
    settings.setFocusMode(settings_.focusMode());

  if (settings_.hasPointsOfInterest() &&
      !settings_.pointsOfInterest().IsEmpty()) {
    settings.setPointsOfInterest(settings_.pointsOfInterest());
  }

  if (settings_.hasExposureCompensation())
    settings.setExposureCompensation(settings_.exposureCompensation());
  if (settings_.hasColorTemperature())
    settings.setColorTemperature(settings_.colorTemperature());
  if (settings_.hasIso())
    settings.setIso(settings_.iso());

  if (settings_.hasBrightness())
    settings.setBrightness(settings_.brightness());
  if (settings_.hasContrast())
    settings.setContrast(settings_.contrast());
  if (settings_.hasSaturation())
    settings.setSaturation(settings_.saturation());
  if (settings_.hasSharpness())
    settings.setSharpness(settings_.sharpness());

  if (settings_.hasZoom())
    settings.setZoom(settings_.zoom());
  if (settings_.hasTorch())
    settings.setTorch(settings_.torch());
}

bool ImageCapture::HasNonImageCaptureConstraints(
    const MediaTrackConstraints& constraints) const {
  if (!constraints.hasAdvanced())
    return false;

  const auto& advanced_constraints = constraints.advanced();
  for (const auto& constraint : advanced_constraints) {
    if (!constraint.hasWhiteBalanceMode() && !constraint.hasExposureMode() &&
        !constraint.hasFocusMode() && !constraint.hasPointsOfInterest() &&
        !constraint.hasExposureCompensation() &&
        !constraint.hasColorTemperature() && !constraint.hasIso() &&
        !constraint.hasBrightness() && !constraint.hasContrast() &&
        !constraint.hasSaturation() && !constraint.hasSharpness() &&
        !constraint.hasZoom() && !constraint.hasTorch()) {
      return true;
    }
  }
  return false;
}

ImageCapture::ImageCapture(ExecutionContext* context, MediaStreamTrack* track)
    : ContextLifecycleObserver(context), stream_track_(track) {
  DCHECK(stream_track_);
  DCHECK(!service_.is_bound());

  Platform::Current()->GetInterfaceProvider()->GetInterface(
      mojo::MakeRequest(&service_));

  service_.set_connection_error_handler(ConvertToBaseCallback(WTF::Bind(
      &ImageCapture::OnServiceConnectionError, WrapWeakPersistent(this))));

  // Launch a retrieval of the current capabilities, which arrive asynchronously
  // to avoid blocking the main UI thread.
  service_->GetCapabilities(
      stream_track_->Component()->Source()->Id(),
      ConvertToBaseCallback(WTF::Bind(
          &ImageCapture::UpdateMediaTrackCapabilities, WrapPersistent(this))));
}

void ImageCapture::OnMojoPhotoCapabilities(
    ScriptPromiseResolver* resolver,
    bool trigger_take_photo,
    media::mojom::blink::PhotoCapabilitiesPtr capabilities) {
  if (!service_requests_.Contains(resolver))
    return;

  if (capabilities.is_null()) {
    resolver->Reject(DOMException::Create(kUnknownError, "platform error"));
    service_requests_.erase(resolver);
    return;
  }

  photo_capabilities_ = PhotoCapabilities::Create();
  photo_capabilities_->SetRedEyeReduction(capabilities->red_eye_reduction);
  // TODO(mcasas): Remove the explicit MediaSettingsRange::create() when
  // mojo::StructTraits supports garbage-collected mappings,
  // https://crbug.com/700180.
  if (capabilities->height->min != 0 || capabilities->height->max != 0) {
    photo_capabilities_->SetImageHeight(
        MediaSettingsRange::Create(std::move(capabilities->height)));
  }
  if (capabilities->width->min != 0 || capabilities->width->max != 0) {
    photo_capabilities_->SetImageWidth(
        MediaSettingsRange::Create(std::move(capabilities->width)));
  }
  if (!capabilities->fill_light_mode.IsEmpty())
    photo_capabilities_->SetFillLightMode(capabilities->fill_light_mode);

  // Update the local track capabilities cache.
  UpdateMediaTrackCapabilities(std::move(capabilities));

  if (trigger_take_photo) {
    service_->TakePhoto(stream_track_->Component()->Source()->Id(),
                        ConvertToBaseCallback(WTF::Bind(
                            &ImageCapture::OnMojoTakePhoto,
                            WrapPersistent(this), WrapPersistent(resolver))));
    return;
  }

  resolver->Resolve(photo_capabilities_);
  service_requests_.erase(resolver);
}

void ImageCapture::OnMojoSetOptions(ScriptPromiseResolver* resolver,
                                    bool trigger_take_photo,
                                    bool result) {
  if (!service_requests_.Contains(resolver))
    return;

  if (!result) {
    resolver->Reject(DOMException::Create(kUnknownError, "setOptions failed"));
    service_requests_.erase(resolver);
    return;
  }

  // Retrieve the current device status after setting the options.
  service_->GetCapabilities(
      stream_track_->Component()->Source()->Id(),
      ConvertToBaseCallback(WTF::Bind(
          &ImageCapture::OnMojoPhotoCapabilities, WrapPersistent(this),
          WrapPersistent(resolver), trigger_take_photo)));
}

void ImageCapture::OnMojoTakePhoto(ScriptPromiseResolver* resolver,
                                   media::mojom::blink::BlobPtr blob) {
  if (!service_requests_.Contains(resolver))
    return;

  // TODO(mcasas): Should be using a mojo::StructTraits.
  if (blob->data.IsEmpty()) {
    resolver->Reject(DOMException::Create(kUnknownError, "platform error"));
  } else {
    resolver->Resolve(
        Blob::Create(blob->data.data(), blob->data.size(), blob->mime_type));
  }
  service_requests_.erase(resolver);
}

void ImageCapture::UpdateMediaTrackCapabilities(
    media::mojom::blink::PhotoCapabilitiesPtr capabilities) {
  if (!capabilities)
    return;

  WTF::Vector<WTF::String> supported_white_balance_modes;
  supported_white_balance_modes.ReserveInitialCapacity(
      capabilities->supported_white_balance_modes.size());
  for (const auto& supported_mode : capabilities->supported_white_balance_modes)
    supported_white_balance_modes.push_back(ToString(supported_mode));
  if (!supported_white_balance_modes.IsEmpty()) {
    capabilities_.setWhiteBalanceMode(std::move(supported_white_balance_modes));
    settings_.setWhiteBalanceMode(
        ToString(capabilities->current_white_balance_mode));
  }

  WTF::Vector<WTF::String> supported_exposure_modes;
  supported_exposure_modes.ReserveInitialCapacity(
      capabilities->supported_exposure_modes.size());
  for (const auto& supported_mode : capabilities->supported_exposure_modes)
    supported_exposure_modes.push_back(ToString(supported_mode));
  if (!supported_exposure_modes.IsEmpty()) {
    capabilities_.setExposureMode(std::move(supported_exposure_modes));
    settings_.setExposureMode(ToString(capabilities->current_exposure_mode));
  }

  WTF::Vector<WTF::String> supported_focus_modes;
  supported_focus_modes.ReserveInitialCapacity(
      capabilities->supported_focus_modes.size());
  for (const auto& supported_mode : capabilities->supported_focus_modes)
    supported_focus_modes.push_back(ToString(supported_mode));
  if (!supported_focus_modes.IsEmpty()) {
    capabilities_.setFocusMode(std::move(supported_focus_modes));
    settings_.setFocusMode(ToString(capabilities->current_focus_mode));
  }

  HeapVector<Point2D> current_points_of_interest;
  if (!capabilities->points_of_interest.IsEmpty()) {
    for (const auto& point : capabilities->points_of_interest) {
      Point2D web_point;
      web_point.setX(point->x);
      web_point.setY(point->y);
      current_points_of_interest.push_back(mojo::Clone(web_point));
    }
  }
  settings_.setPointsOfInterest(std::move(current_points_of_interest));

  // TODO(mcasas): Remove the explicit MediaSettingsRange::create() when
  // mojo::StructTraits supports garbage-collected mappings,
  // https://crbug.com/700180.
  if (capabilities->exposure_compensation->max !=
      capabilities->exposure_compensation->min) {
    capabilities_.setExposureCompensation(
        MediaSettingsRange::Create(*capabilities->exposure_compensation));
    settings_.setExposureCompensation(
        capabilities->exposure_compensation->current);
  }
  if (capabilities->color_temperature->max !=
      capabilities->color_temperature->min) {
    capabilities_.setColorTemperature(
        MediaSettingsRange::Create(*capabilities->color_temperature));
    settings_.setColorTemperature(capabilities->color_temperature->current);
  }
  if (capabilities->iso->max != capabilities->iso->min) {
    capabilities_.setIso(MediaSettingsRange::Create(*capabilities->iso));
    settings_.setIso(capabilities->iso->current);
  }

  if (capabilities->brightness->max != capabilities->brightness->min) {
    capabilities_.setBrightness(
        MediaSettingsRange::Create(*capabilities->brightness));
    settings_.setBrightness(capabilities->brightness->current);
  }
  if (capabilities->contrast->max != capabilities->contrast->min) {
    capabilities_.setContrast(
        MediaSettingsRange::Create(*capabilities->contrast));
    settings_.setContrast(capabilities->contrast->current);
  }
  if (capabilities->saturation->max != capabilities->saturation->min) {
    capabilities_.setSaturation(
        MediaSettingsRange::Create(*capabilities->saturation));
    settings_.setSaturation(capabilities->saturation->current);
  }
  if (capabilities->sharpness->max != capabilities->sharpness->min) {
    capabilities_.setSharpness(
        MediaSettingsRange::Create(*capabilities->sharpness));
    settings_.setSharpness(capabilities->sharpness->current);
  }

  if (capabilities->zoom->max != capabilities->zoom->min) {
    capabilities_.setZoom(MediaSettingsRange::Create(*capabilities->zoom));
    settings_.setZoom(capabilities->zoom->current);
  }

  if (capabilities->supports_torch)
    capabilities_.setTorch(capabilities->supports_torch);
  if (capabilities->supports_torch)
    settings_.setTorch(capabilities->torch);
}

void ImageCapture::OnServiceConnectionError() {
  service_.reset();
  for (ScriptPromiseResolver* resolver : service_requests_)
    resolver->Reject(DOMException::Create(kNotFoundError, kNoServiceError));
  service_requests_.clear();
}

DEFINE_TRACE(ImageCapture) {
  visitor->Trace(stream_track_);
  visitor->Trace(capabilities_);
  visitor->Trace(settings_);
  visitor->Trace(current_constraints_);
  visitor->Trace(photo_capabilities_);
  visitor->Trace(service_requests_);
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
