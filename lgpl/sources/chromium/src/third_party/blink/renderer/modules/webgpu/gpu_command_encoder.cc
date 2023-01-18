// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_command_encoder.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_command_buffer_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_command_encoder_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_compute_pass_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_compute_pass_timestamp_write.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_image_copy_buffer.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_image_copy_texture.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_render_pass_color_attachment.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_render_pass_depth_stencil_attachment.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_render_pass_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_render_pass_timestamp_write.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_doublesequence_gpucolordict.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_doublesequence_gpucolordict_gpuloadop.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_float_gpuloadop.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_gpuloadop_unsignedlongenforcerange.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_command_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_compute_pass_encoder.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_query_set.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_render_pass_encoder.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_supported_features.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture_view.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

WGPURenderPassColorAttachment AsDawnType(
    const GPURenderPassColorAttachment* webgpu_desc) {
  DCHECK(webgpu_desc);

  WGPURenderPassColorAttachment dawn_desc = {};
  // TODO(dawn:1269): Remove after the deprecation period.
  // clearColor needs to be set to all NaNs to signal that it is not in use.
  dawn_desc.clearColor = {NAN, NAN, NAN, NAN};

  dawn_desc.view = webgpu_desc->view()->GetHandle();
  dawn_desc.resolveTarget = webgpu_desc->hasResolveTarget()
                                ? webgpu_desc->resolveTarget()->GetHandle()
                                : nullptr;

  if (webgpu_desc->hasClearValue()) {
    dawn_desc.clearValue = AsDawnType(webgpu_desc->clearValue());
  } else {
    dawn_desc.clearValue = {};
  }

  if (webgpu_desc->hasLoadOp()) {
    dawn_desc.loadOp = AsDawnEnum(webgpu_desc->loadOp());
  } else if (webgpu_desc->hasLoadValue()) {
    // TODO(dawn:1269): Remove this branch after the deprecation period.
    switch (webgpu_desc->loadValue()->GetContentType()) {
      case V8UnionGPUColorOrGPULoadOp::ContentType::kGPULoadOp:
        dawn_desc.loadOp =
            AsDawnEnum(webgpu_desc->loadValue()->GetAsGPULoadOp());
        break;
      case V8UnionGPUColorOrGPULoadOp::ContentType::kGPUColorDict:
        dawn_desc.loadOp = WGPULoadOp_Clear;
        dawn_desc.clearValue =
            AsDawnType(webgpu_desc->loadValue()->GetAsGPUColorDict());
        break;
      case V8UnionGPUColorOrGPULoadOp::ContentType::kDoubleSequence:
        dawn_desc.loadOp = WGPULoadOp_Clear;
        dawn_desc.clearValue =
            AsDawnColor(webgpu_desc->loadValue()->GetAsDoubleSequence());
        break;
    }
  }

  if (webgpu_desc->hasStoreOp()) {
    dawn_desc.storeOp = AsDawnEnum(webgpu_desc->storeOp());
  } else {
    // TODO(dawn:1269): Remove when deprecation period is complete.
    dawn_desc.storeOp = WGPUStoreOp_Store;
  }

  return dawn_desc;
}

WGPUComputePassTimestampWrite AsDawnType(
    const GPUComputePassTimestampWrite* webgpu_desc) {
  DCHECK(webgpu_desc);
  DCHECK(webgpu_desc->querySet());

  WGPUComputePassTimestampWrite dawn_desc = {};
  dawn_desc.querySet = webgpu_desc->querySet()->GetHandle();
  dawn_desc.queryIndex = webgpu_desc->queryIndex();
  dawn_desc.location = AsDawnEnum(webgpu_desc->location());

  return dawn_desc;
}

WGPURenderPassTimestampWrite AsDawnType(
    const GPURenderPassTimestampWrite* webgpu_desc) {
  DCHECK(webgpu_desc);
  DCHECK(webgpu_desc->querySet());

  WGPURenderPassTimestampWrite dawn_desc = {};
  dawn_desc.querySet = webgpu_desc->querySet()->GetHandle();
  dawn_desc.queryIndex = webgpu_desc->queryIndex();
  dawn_desc.location = AsDawnEnum(webgpu_desc->location());

  return dawn_desc;
}

namespace {

WGPURenderPassDepthStencilAttachment AsDawnType(
    GPUDevice* device,
    const GPURenderPassDepthStencilAttachment* webgpu_desc) {
  DCHECK(webgpu_desc);

  WGPURenderPassDepthStencilAttachment dawn_desc = {};
  // TODO(dawn:1269): Remove after the deprecation period.
  // clearDepth needs to be set to NaN to signal that it is not in use.
  dawn_desc.clearDepth = NAN;

  dawn_desc.view = webgpu_desc->view()->GetHandle();

  if (webgpu_desc->hasDepthLoadOp()) {
    dawn_desc.depthLoadOp = AsDawnEnum(webgpu_desc->depthLoadOp());
    dawn_desc.depthClearValue = webgpu_desc->depthClearValue();
  } else if (webgpu_desc->hasDepthLoadValue()) {
    // TODO(dawn:1269): Remove this branch after the deprecation period.
    device->AddConsoleWarning(
        "depthLoadValue has been deprecated and will soon be removed. Use "
        "depthLoadOp and depthClearValue instead.");

    switch (webgpu_desc->depthLoadValue()->GetContentType()) {
      case V8UnionFloatOrGPULoadOp::ContentType::kGPULoadOp:
        dawn_desc.depthLoadOp =
            AsDawnEnum(webgpu_desc->depthLoadValue()->GetAsGPULoadOp());
        dawn_desc.depthClearValue = 1.0f;
        break;
      case V8UnionFloatOrGPULoadOp::ContentType::kFloat:
        dawn_desc.depthLoadOp = WGPULoadOp_Clear;
        dawn_desc.depthClearValue = webgpu_desc->depthLoadValue()->GetAsFloat();
        break;
    }
  }

  if (webgpu_desc->hasDepthStoreOp()) {
    dawn_desc.depthStoreOp = AsDawnEnum(webgpu_desc->depthStoreOp());
  }

  dawn_desc.depthReadOnly = webgpu_desc->depthReadOnly();

  if (webgpu_desc->hasStencilLoadOp()) {
    dawn_desc.stencilLoadOp = AsDawnEnum(webgpu_desc->stencilLoadOp());
    dawn_desc.clearStencil = webgpu_desc->stencilClearValue();
  } else if (webgpu_desc->hasStencilLoadValue()) {
    // TODO(dawn:1269): Remove this branch after the deprecation period.
    device->AddConsoleWarning(
        "stencilLoadValue has been deprecated and will soon be removed. Use "
        "stencilLoadOp and stencilClearValue instead.");

    switch (webgpu_desc->stencilLoadValue()->GetContentType()) {
      case V8UnionGPULoadOpOrGPUStencilValue::ContentType::kGPULoadOp:
        dawn_desc.stencilLoadOp =
            AsDawnEnum(webgpu_desc->stencilLoadValue()->GetAsGPULoadOp());
        dawn_desc.clearStencil = 0;
        break;
      case V8UnionGPULoadOpOrGPUStencilValue::ContentType::kV8GPUStencilValue:
        dawn_desc.stencilLoadOp = WGPULoadOp_Clear;
        dawn_desc.clearStencil =
            webgpu_desc->stencilLoadValue()->GetAsV8GPUStencilValue();
        break;
    }
  }

  if (webgpu_desc->hasStencilStoreOp()) {
    dawn_desc.stencilStoreOp = AsDawnEnum(webgpu_desc->stencilStoreOp());
  }

  dawn_desc.stencilReadOnly = webgpu_desc->stencilReadOnly();

  return dawn_desc;
}

WGPUImageCopyBuffer ValidateAndConvertImageCopyBuffer(
    const GPUImageCopyBuffer* webgpu_view,
    const char** error) {
  DCHECK(webgpu_view);
  DCHECK(webgpu_view->buffer());

  WGPUImageCopyBuffer dawn_view = {};
  dawn_view.nextInChain = nullptr;
  dawn_view.buffer = webgpu_view->buffer()->GetHandle();

  *error = ValidateTextureDataLayout(webgpu_view, &dawn_view.layout);
  return dawn_view;
}

WGPUCommandEncoderDescriptor AsDawnType(
    const GPUCommandEncoderDescriptor* webgpu_desc,
    std::string* label) {
  DCHECK(webgpu_desc);
  DCHECK(label);

  WGPUCommandEncoderDescriptor dawn_desc = {};
  dawn_desc.nextInChain = nullptr;
  if (webgpu_desc->hasLabel()) {
    *label = webgpu_desc->label().Utf8();
    dawn_desc.label = label->c_str();
  }

  return dawn_desc;
}

}  // anonymous namespace

// static
GPUCommandEncoder* GPUCommandEncoder::Create(
    GPUDevice* device,
    const GPUCommandEncoderDescriptor* webgpu_desc) {
  DCHECK(device);
  DCHECK(webgpu_desc);

  std::string label;
  WGPUCommandEncoderDescriptor dawn_desc = AsDawnType(webgpu_desc, &label);

  GPUCommandEncoder* encoder = MakeGarbageCollected<GPUCommandEncoder>(
      device, device->GetProcs().deviceCreateCommandEncoder(device->GetHandle(),
                                                            &dawn_desc));
  if (webgpu_desc->hasLabel())
    encoder->setLabel(webgpu_desc->label());
  return encoder;
}

GPUCommandEncoder::GPUCommandEncoder(GPUDevice* device,
                                     WGPUCommandEncoder command_encoder)
    : DawnObject<WGPUCommandEncoder>(device, command_encoder) {}

GPURenderPassEncoder* GPUCommandEncoder::beginRenderPass(
    const GPURenderPassDescriptor* descriptor,
    ExceptionState& exception_state) {
  DCHECK(descriptor);

  // Until the .attachment property is removed manual validation needs to be
  // done for every attachment point

  uint32_t color_attachment_count =
      static_cast<uint32_t>(descriptor->colorAttachments().size());

  // Check loadValue color is correctly formatted before further processing.
  for (wtf_size_t i = 0; i < color_attachment_count; ++i) {
    const auto& maybe_color_attachment = descriptor->colorAttachments()[i];
    // Check if the color attachment is null since it is a sparse array
    if (!maybe_color_attachment) {
      continue;
    }

    const GPURenderPassColorAttachment* color_attachment =
        maybe_color_attachment.Get();

    if (color_attachment->hasLoadOp()) {
      if (color_attachment->hasClearValue() &&
          color_attachment->clearValue()->IsDoubleSequence() &&
          color_attachment->clearValue()->GetAsDoubleSequence().size() != 4) {
        exception_state.ThrowRangeError("clearValue color size must be 4");
        return nullptr;
      }
    } else if (color_attachment->hasLoadValue()) {
      if (color_attachment->loadValue()->IsDoubleSequence() &&
          color_attachment->loadValue()->GetAsDoubleSequence().size() != 4) {
        exception_state.ThrowRangeError("loadValue color size must be 4");
        return nullptr;
      }

      device_->AddConsoleWarning(
          "loadValue has been deprecated and will soon be removed. Use loadOp "
          "and clearValue instead.");
    }

    if (!color_attachment->hasStoreOp()) {
      device_->AddConsoleWarning(
          "storeOp will soon be required and no longer default to 'store'.");
    }
  }

  std::string label;
  WGPURenderPassDescriptor dawn_desc = {};
  dawn_desc.colorAttachmentCount = color_attachment_count;
  dawn_desc.colorAttachments = nullptr;
  if (descriptor->hasLabel()) {
    label = descriptor->label().Utf8();
    dawn_desc.label = label.c_str();
  }

  std::unique_ptr<WGPURenderPassColorAttachment[]> color_attachments;
  if (color_attachment_count > 0) {
    color_attachments = AsDawnType(descriptor->colorAttachments());
    dawn_desc.colorAttachments = color_attachments.get();
  }

  WGPURenderPassDepthStencilAttachment depthStencilAttachment = {};
  if (descriptor->hasDepthStencilAttachment()) {
    const GPURenderPassDepthStencilAttachment* depth_stencil =
        descriptor->depthStencilAttachment();
    depthStencilAttachment = AsDawnType(device_, depth_stencil);
    dawn_desc.depthStencilAttachment = &depthStencilAttachment;
  } else {
    dawn_desc.depthStencilAttachment = nullptr;
  }

  if (descriptor->hasOcclusionQuerySet()) {
    dawn_desc.occlusionQuerySet = AsDawnType(descriptor->occlusionQuerySet());
  } else {
    dawn_desc.occlusionQuerySet = nullptr;
  }

  uint32_t timestamp_writes_count =
      static_cast<uint32_t>(descriptor->timestampWrites().size());
  dawn_desc.timestampWriteCount = timestamp_writes_count;
  std::unique_ptr<WGPURenderPassTimestampWrite[]> timestamp_writes;
  if (timestamp_writes_count > 0) {
    timestamp_writes = AsDawnType(descriptor->timestampWrites());
    dawn_desc.timestampWrites = timestamp_writes.get();
  } else {
    dawn_desc.timestampWrites = nullptr;
  }

  WGPURenderPassDescriptorMaxDrawCount max_draw_count = {};
  if (descriptor->hasMaxDrawCount()) {
    max_draw_count.chain.sType = WGPUSType_RenderPassDescriptorMaxDrawCount;
    max_draw_count.maxDrawCount = descriptor->maxDrawCount();
    dawn_desc.nextInChain =
        reinterpret_cast<WGPUChainedStruct*>(&max_draw_count);
  } else {
    dawn_desc.nextInChain = nullptr;
  }

  GPURenderPassEncoder* encoder = MakeGarbageCollected<GPURenderPassEncoder>(
      device_,
      GetProcs().commandEncoderBeginRenderPass(GetHandle(), &dawn_desc));
  if (descriptor->hasLabel())
    encoder->setLabel(descriptor->label());
  return encoder;
}

GPUComputePassEncoder* GPUCommandEncoder::beginComputePass(
    const GPUComputePassDescriptor* descriptor) {
  std::string label;
  WGPUComputePassDescriptor dawn_desc = {};
  if (descriptor->hasLabel()) {
    label = descriptor->label().Utf8();
    dawn_desc.label = label.c_str();
  }

  uint32_t timestamp_writes_count =
      static_cast<uint32_t>(descriptor->timestampWrites().size());
  dawn_desc.timestampWriteCount = timestamp_writes_count;
  std::unique_ptr<WGPUComputePassTimestampWrite[]> timestamp_writes;
  if (timestamp_writes_count > 0) {
    timestamp_writes = AsDawnType(descriptor->timestampWrites());
    dawn_desc.timestampWrites = timestamp_writes.get();
  } else {
    dawn_desc.timestampWrites = nullptr;
  }

  GPUComputePassEncoder* encoder = MakeGarbageCollected<GPUComputePassEncoder>(
      device_,
      GetProcs().commandEncoderBeginComputePass(GetHandle(), &dawn_desc));
  if (descriptor->hasLabel())
    encoder->setLabel(descriptor->label());
  return encoder;
}

void GPUCommandEncoder::copyBufferToTexture(GPUImageCopyBuffer* source,
                                            GPUImageCopyTexture* destination,
                                            const V8GPUExtent3D* copy_size) {
  WGPUExtent3D dawn_copy_size = AsDawnType(copy_size);
  WGPUImageCopyTexture dawn_destination = AsDawnType(destination);

  const char* error = nullptr;
  WGPUImageCopyBuffer dawn_source =
      ValidateAndConvertImageCopyBuffer(source, &error);
  if (error) {
    GetProcs().commandEncoderInjectValidationError(GetHandle(), error);
    return;
  }

  GetProcs().commandEncoderCopyBufferToTexture(
      GetHandle(), &dawn_source, &dawn_destination, &dawn_copy_size);
}

void GPUCommandEncoder::copyTextureToBuffer(GPUImageCopyTexture* source,
                                            GPUImageCopyBuffer* destination,
                                            const V8GPUExtent3D* copy_size) {
  WGPUExtent3D dawn_copy_size = AsDawnType(copy_size);
  WGPUImageCopyTexture dawn_source = AsDawnType(source);

  const char* error = nullptr;
  WGPUImageCopyBuffer dawn_destination =
      ValidateAndConvertImageCopyBuffer(destination, &error);
  if (error) {
    GetProcs().commandEncoderInjectValidationError(GetHandle(), error);
    return;
  }

  GetProcs().commandEncoderCopyTextureToBuffer(
      GetHandle(), &dawn_source, &dawn_destination, &dawn_copy_size);
}

void GPUCommandEncoder::copyTextureToTexture(GPUImageCopyTexture* source,
                                             GPUImageCopyTexture* destination,
                                             const V8GPUExtent3D* copy_size) {
  WGPUImageCopyTexture dawn_source = AsDawnType(source);
  WGPUImageCopyTexture dawn_destination = AsDawnType(destination);
  WGPUExtent3D dawn_copy_size = AsDawnType(copy_size);

  GetProcs().commandEncoderCopyTextureToTexture(
      GetHandle(), &dawn_source, &dawn_destination, &dawn_copy_size);
}

void GPUCommandEncoder::writeTimestamp(DawnObject<WGPUQuerySet>* querySet,
                                       uint32_t queryIndex,
                                       ExceptionState& exception_state) {
  if (!device_->features()->has("timestamp-query")) {
    exception_state.ThrowTypeError(String::Format(
        "Use of the writeTimestamp() method requires the 'timestamp-query' "
        "feature to be enabled on %s.",
        device_->formattedLabel().c_str()));
    return;
  }
  GetProcs().commandEncoderWriteTimestamp(GetHandle(), querySet->GetHandle(),
                                          queryIndex);
}

GPUCommandBuffer* GPUCommandEncoder::finish(
    const GPUCommandBufferDescriptor* descriptor) {
  std::string label;
  WGPUCommandBufferDescriptor dawn_desc = {};
  if (descriptor->hasLabel()) {
    label = descriptor->label().Utf8();
    dawn_desc.label = label.c_str();
  }

  return MakeGarbageCollected<GPUCommandBuffer>(
      device_, GetProcs().commandEncoderFinish(GetHandle(), &dawn_desc));
}

}  // namespace blink
