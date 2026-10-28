#pragma once

#include "frame_packet.hpp"

namespace aurora::gfx {

bool bind_pipeline(PipelineRef ref, const wgpu::RenderPassEncoder& pass);
void bind_pipeline(const wgpu::RenderPipeline& pipeline, const wgpu::RenderPassEncoder& pass);

namespace detail {
void encode_op(wgpu::CommandEncoder& encoder, FramePacket& frame, const FrameOp& op);
// Texture vertex fetch: upload the frame's vertex/storage data to its fetch textures. Call after
// the staging buffer is unmapped and before the frame's command buffer is submitted.
void submit_vertex_fetch_upload(const FramePacket& frame);
}

} // namespace aurora::gfx
