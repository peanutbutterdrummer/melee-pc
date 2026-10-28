#pragma once

#include "types.hpp"

namespace aurora::gfx {
inline constexpr bool UseTextureBuffer = true;
/* Melee's busiest Classic frames land right on 24 MiB of per-draw uniforms
 * and occasionally tip over it (observed: 25167920 requested). These pools
 * are non-owning views onto the mapped staging buffer, so overflowing is a
 * hard abort, not a realloc. MELEE_GFX_STATS=1 prints the peak each second
 * if this needs sizing again. */
#if defined(__ANDROID__) || defined(AURORA_COMPACT_GPU_POOLS)
// Mobile optimization: clamp GPU buffer pools to conserve memory on 2-3GB mobile devices
// while retaining sufficient headroom for Melee draw calls.
inline constexpr uint64_t UniformBufferSize = 25165824; // 24 MiB (ample for Melee peak 24MB, instead of 48MB)
inline constexpr uint64_t VertexBufferSize = 4194304;   // 4 MiB
inline constexpr uint64_t IndexBufferSize = 1048576;    // 1 MiB
inline constexpr uint64_t StorageBufferSize = 4194304;  // 4 MiB
inline constexpr uint64_t TextureUploadSize = 12582912; // 12 MiB
#else
inline constexpr uint64_t UniformBufferSize = 50331648; // 48 MiB
inline constexpr uint64_t VertexBufferSize = 5242880;   // 5 MiB
inline constexpr uint64_t IndexBufferSize = 2097152;    // 2 MiB
inline constexpr uint64_t StorageBufferSize = 8388608;  // 8 MiB
inline constexpr uint64_t TextureUploadSize = 25165824; // 24 MiB
#endif

// Per-frame resource slots (enabled with webgpu::g_vertexFetchTextures, i.e. Mali GLES): each
// frame uses its own vertex/uniform/index/storage buffers and fetch textures, filled once before
// the frame's commands (submit_frame_upload), so nothing is written while an in-flight draw may
// still read it. Mali otherwise copies the whole buffer or texture on the CPU at the next draw.
inline constexpr size_t ResourceSlots = 3;

// Texture vertex fetch: the vertex, storage and index data are read from r32uint textures this many
// texels (u32 words) wide.
inline constexpr uint32_t VertexFetchRowTexels = 4096;
inline constexpr uint32_t VertexFetchRowBytes = VertexFetchRowTexels * 4;
static_assert(VertexBufferSize % VertexFetchRowBytes == 0 && StorageBufferSize % VertexFetchRowBytes == 0 &&
              IndexBufferSize % VertexFetchRowBytes == 0);

namespace detail {
struct Resources {
  wgpu::Buffer vertexBuffer;
  wgpu::Buffer uniformBuffer;
  wgpu::Buffer indexBuffer;
  wgpu::Buffer storageBuffer;
  wgpu::Texture vertexFetchTexture;  // only with webgpu::g_vertexFetchTextures
  wgpu::Texture storageFetchTexture; // only with webgpu::g_vertexFetchTextures
  wgpu::Texture indexFetchTexture;   // only with webgpu::g_vertexFetchTextures
  wgpu::BindGroupLayout staticBindGroupLayout;
  wgpu::BindGroup staticBindGroup;
  wgpu::BindGroupLayout uniformBindGroupLayout;
  wgpu::BindGroup uniformBindGroup;
  wgpu::Limits limits;
  AuroraStats stats{};
};

// The calling thread's resource slot (slot 0 unless per-frame slots are enabled).
Resources& resources() noexcept;
// Select the slot for frame `frameId` on the calling thread (recording or encoding that frame).
void select_resource_slot(uint64_t frameId) noexcept;
} // namespace detail
} // namespace aurora::gfx
