// dawntest.cpp - smoke test: Dawn GLES backend -> fbegl shim -> Mali fbdev.
// Clears the screen through red/green/blue for ~3 s, printing adapter info.
#include <webgpu/webgpu_cpp.h>
#include <dawn/native/DawnNative.h>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    wgpu::InstanceFeatureName feats[] = {wgpu::InstanceFeatureName::TimedWaitAny};
    wgpu::InstanceDescriptor idesc{};
    idesc.requiredFeatureCount = 1;
    idesc.requiredFeatures = feats;
    wgpu::Instance instance = wgpu::CreateInstance(&idesc);
    if (!instance) { puts("no instance"); return 1; }

    wgpu::SurfaceSourceXlibWindow xsrc{};
    xsrc.display = reinterpret_cast<void*>(1);
    xsrc.window = 1;
    wgpu::SurfaceDescriptor sdesc{};
    sdesc.nextInChain = &xsrc;
    wgpu::Surface surface = instance.CreateSurface(&sdesc);
    if (!surface) { puts("no surface"); return 1; }

    wgpu::RequestAdapterOptions opts{};
    opts.backendType = wgpu::BackendType::OpenGLES;
    opts.featureLevel = wgpu::FeatureLevel::Compatibility;
    opts.compatibleSurface = surface;
    wgpu::Adapter adapter;
    instance.WaitAny(instance.RequestAdapter(&opts, wgpu::CallbackMode::WaitAnyOnly,
        [&](wgpu::RequestAdapterStatus st, wgpu::Adapter a, wgpu::StringView msg) {
            if (st != wgpu::RequestAdapterStatus::Success)
                printf("adapter failed: %.*s\n", (int)msg.length, msg.data);
            adapter = std::move(a);
        }), UINT64_MAX);
    if (!adapter) return 1;
    wgpu::AdapterInfo info{};
    adapter.GetInfo(&info);
    printf("adapter: %.*s | %.*s | %.*s\n", (int)info.vendor.length, info.vendor.data,
           (int)info.device.length, info.device.data, (int)info.description.length, info.description.data);
    wgpu::Limits lim{};
    adapter.GetLimits(&lim);
    printf("limits: tex2d=%u storageBuf/stage=%u uniformBuf/stage=%u\n", lim.maxTextureDimension2D,
           lim.maxStorageBuffersPerShaderStage, lim.maxUniformBuffersPerShaderStage);

    wgpu::DeviceDescriptor ddesc{};
    ddesc.SetUncapturedErrorCallback([](const wgpu::Device&, wgpu::ErrorType, wgpu::StringView m) {
        printf("device error: %.*s\n", (int)m.length, m.data);
    });
    wgpu::Device device;
    instance.WaitAny(adapter.RequestDevice(&ddesc, wgpu::CallbackMode::WaitAnyOnly,
        [&](wgpu::RequestDeviceStatus st, wgpu::Device d, wgpu::StringView msg) {
            if (st != wgpu::RequestDeviceStatus::Success)
                printf("device failed: %.*s\n", (int)msg.length, msg.data);
            device = std::move(d);
        }), UINT64_MAX);
    if (!device) return 1;

    wgpu::SurfaceCapabilities caps{};
    surface.GetCapabilities(adapter, &caps);
    printf("surface formats: %zu, first=%d\n", caps.formatCount, caps.formatCount ? (int)caps.formats[0] : -1);
    wgpu::SurfaceConfiguration cfg{};
    cfg.device = device;
    cfg.format = caps.formats[0];
    cfg.width = 720;
    cfg.height = 480;
    cfg.presentMode = wgpu::PresentMode::Fifo;
    surface.Configure(&cfg);

    const double cols[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    for (int f = 0; f < 180; f++) {
        wgpu::SurfaceTexture st;
        surface.GetCurrentTexture(&st);
        if (!st.texture) { puts("no texture"); return 1; }
        wgpu::RenderPassColorAttachment ca{};
        ca.view = st.texture.CreateView();
        ca.loadOp = wgpu::LoadOp::Clear;
        ca.storeOp = wgpu::StoreOp::Store;
        const double* c = cols[f / 60];
        ca.clearValue = {c[0], c[1], c[2], 1};
        wgpu::RenderPassDescriptor rp{};
        rp.colorAttachmentCount = 1;
        rp.colorAttachments = &ca;
        wgpu::CommandEncoder enc = device.CreateCommandEncoder();
        enc.BeginRenderPass(&rp).End();
        wgpu::CommandBuffer cb = enc.Finish();
        device.GetQueue().Submit(1, &cb);
        surface.Present();
    }
    puts("DAWNTEST-OK");
    return 0;
}
