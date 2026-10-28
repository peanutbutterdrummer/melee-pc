#pragma once

#include "frame_packet.hpp"

namespace aurora::gfx {

bool bind_pipeline(PipelineRef ref, const wgpu::RenderPassEncoder& pass);
void bind_pipeline(const wgpu::RenderPipeline& pipeline, const wgpu::RenderPassEncoder& pass);

namespace detail {
void encode_op(wgpu::CommandEncoder& encoder, FramePacket& frame, const FrameOp& op);
// Per-frame resource slots: upload the frame's pools into its slot (select it first). Call after
// the staging buffer is unmapped and before the frame's command buffer is submitted.
void submit_frame_upload(const FramePacket& frame);
}

} // namespace aurora::gfx
