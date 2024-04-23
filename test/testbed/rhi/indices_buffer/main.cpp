#include "graphics/rhi/rhi.hpp"
#include "nickel.hpp"

using namespace nickel::rhi;

APIPreference API = APIPreference::GL;

struct Context {
    PipelineLayout layout;
    RenderPipeline pipeline;
    Buffer vertexBuffer;
    Buffer indicesBuffer;
};

struct Vertex {
    nickel::cgmath::Vec2 position;
    nickel::cgmath::Vec3 color;
};

std::array<Vertex, 4> gVertices = {
    Vertex{nickel::cgmath::Vec2{-0.5, -0.5},
           nickel::cgmath::Vec3{1.0, 0.0, 0.0}                                  },
    Vertex{ nickel::cgmath::Vec2{0.5, -0.5},
           nickel::cgmath::Vec3{0.0, 1.0, 0.0}                                  },
    Vertex{  nickel::cgmath::Vec2{0.5, 0.5}, nickel::cgmath::Vec3{0.0, 0.0, 1.0}},
    Vertex{ nickel::cgmath::Vec2{-0.5, 0.5},
           nickel::cgmath::Vec3{0.0, 0.0, 1.0}                                  }
};

std::array<uint32_t, 9> gIndices = {
    99, 99, 99,           // some invalid data for testing indices offset
    0,  1,  2,  0, 2, 3,  // real indices data
};

void initShaders(APIPreference api, Device device,
                 RenderPipeline::Descriptor& desc) {
    ShaderModule::Descriptor shaderDesc;

    if (api == APIPreference::Vulkan) {
        shaderDesc.code =
            nickel::ReadWholeFile<std::vector<char>>(
                "test/testbed/rhi/indices_buffer/vert.spv", std::ios::binary)
                .value();
        desc.vertex.module = device.CreateShaderModule(shaderDesc);

        shaderDesc.code =
            nickel::ReadWholeFile<std::vector<char>>(
                "test/testbed/rhi/indices_buffer/frag.spv", std::ios::binary)
                .value();
        desc.fragment.module = device.CreateShaderModule(shaderDesc);
    } else if (api == APIPreference::GL) {
        shaderDesc.code =
            nickel::ReadWholeFile<std::vector<char>>(
                "test/testbed/rhi/indices_buffer/shader.glsl.vert")
                .value();
        desc.vertex.module = device.CreateShaderModule(shaderDesc);

        shaderDesc.code =
            nickel::ReadWholeFile<std::vector<char>>(
                "test/testbed/rhi/indices_buffer/shader.glsl.frag")
                .value();
        desc.fragment.module = device.CreateShaderModule(shaderDesc);
    }
}

void StartupSystem(gecs::commands cmds,
                   gecs::resource<gecs::mut<nickel::Window>> window) {
    auto& adapter =
        cmds.emplace_resource<Adapter>(window->Raw(), Adapter::Option{API});
    auto& device = cmds.emplace_resource<Device>(adapter.RequestDevice());
    auto& ctx = cmds.emplace_resource<Context>();

    RenderPipeline::Descriptor desc;

    RenderPipeline::VertexState vertexState;
    RenderPipeline::BufferState bufferState;

    bufferState.attributes.push_back({VertexFormat::Float32x2, 0, 0});
    bufferState.attributes.push_back({VertexFormat::Float32x3, 8, 1});
    bufferState.arrayStride = 5 * 4;
    desc.vertex.buffers.emplace_back(bufferState);

    desc.viewport.viewport.x = 0;
    desc.viewport.viewport.y = 0;
    desc.viewport.viewport.w = window->Size().w;
    desc.viewport.viewport.h = window->Size().h;
    desc.viewport.scissor.offset.x = 0;
    desc.viewport.scissor.offset.y = 0;
    desc.viewport.scissor.extent.width = window->Size().w;
    desc.viewport.scissor.extent.height = window->Size().h;

    initShaders(adapter.RequestAdapterInfo().api, device, desc);

    ctx.layout = device.CreatePipelineLayout({});
    desc.layout = ctx.layout;
    RenderPipeline::FragmentTarget target;
    target.format = TextureFormat::Presentation;
    desc.fragment.targets.emplace_back(target);

    Buffer::Descriptor bufferDesc;
    bufferDesc.size = sizeof(gVertices);
    bufferDesc.mappedAtCreation = true;
    bufferDesc.usage = BufferUsage::Vertex;
    ctx.vertexBuffer = device.CreateBuffer(bufferDesc);
    void* data = ctx.vertexBuffer.GetMappedRange();
    memcpy(data, gVertices.data(), sizeof(gVertices));
    ctx.vertexBuffer.Unmap();

    bufferDesc.size = sizeof(gIndices);
    bufferDesc.mappedAtCreation = true;
    bufferDesc.usage = BufferUsage::Index;
    ctx.indicesBuffer = device.CreateBuffer(bufferDesc);
    data = ctx.indicesBuffer.GetMappedRange();
    memcpy(data, gIndices.data(), sizeof(gIndices));
    ctx.indicesBuffer.Unmap();

    ctx.pipeline = device.CreateRenderPipeline(desc);
}

void UpdateSystem(gecs::resource<gecs::mut<nickel::rhi::Device>> device,
                  gecs::resource<gecs::mut<Context>> ctx) {
    device->BeginFrame();

    RenderPass::Descriptor desc;
    RenderPass::Descriptor::ColorAttachment colorAtt;
    colorAtt.loadOp = AttachmentLoadOp::Clear;
    colorAtt.storeOp = AttachmentStoreOp::Store;
    colorAtt.clearValue.fill(1);

    Texture::Descriptor textureDesc;
    textureDesc.format = TextureFormat::Presentation;
    auto texture = device->CreateTexture(textureDesc);

    auto view = texture.CreateView();
    colorAtt.view = view;
    desc.colorAttachments.emplace_back(colorAtt);

    auto encoder = device->CreateCommandEncoder();
    auto renderPass = encoder.BeginRenderPass(desc);
    renderPass.SetPipeline(ctx->pipeline);
    renderPass.SetVertexBuffer(0, ctx->vertexBuffer, 0,
                               ctx->vertexBuffer.Size());
    renderPass.SetIndexBuffer(ctx->indicesBuffer, IndexType::Uint32, 0,
                              sizeof(gIndices));
    renderPass.DrawIndexed(6, 1, 3, 0, 0);
    renderPass.End();
    auto cmd = encoder.Finish();

    Queue queue = device->GetQueue();

    queue.Submit({cmd});
    device->EndFrame();

    encoder.Destroy();
    view.Destroy();
    texture.Destroy();
}

void ShutdownSystem(gecs::commands cmds, gecs::resource<gecs::mut<Context>> ctx,
                    gecs::resource<gecs::mut<Device>> device,
                    gecs::resource<gecs::mut<Adapter>> adapter) {
    ctx->vertexBuffer.Destroy();
    ctx->indicesBuffer.Destroy();
    ctx->layout.Destroy();
    ctx->pipeline.Destroy();
}

void BootstrapSystem(gecs::world& world,
                     typename gecs::world::registry_type& reg) {
    auto& args = reg.res<nickel::CmdLineArgs>()->Args();
#ifdef NICKEL_HAS_VULKAN
    bool isVulkanBackend =
        args.size() == 1 ? true : (args[1] == "--api=gl" ? false : true);
    if (isVulkanBackend) {
        API = APIPreference::Vulkan;
    } else {
        API = APIPreference::GL;
    }
#else
    bool isVulkanBackend = false;
#endif
    nickel::Window& window = reg.commands().emplace_resource<nickel::Window>(
        "indices buffer", 1024, 720, API == APIPreference::Vulkan);

    reg
        // startup systems
        .regist_startup_system<nickel::VideoSystemInit>()
        .regist_startup_system<nickel::EventPollerInit>()
        .regist_startup_system<nickel::InputSystemInit>()
        .regist_startup_system<StartupSystem>()
        // shutdown systems
        .regist_shutdown_system<ShutdownSystem>()
        .regist_shutdown_system<nickel::EngineShutdown>()
        // update systems
        .regist_update_system<nickel::VideoSystemUpdate>()
        // other input handle event must put here(after mouse/keyboard update)
        .regist_update_system<nickel::Mouse::Update>()
        .regist_update_system<nickel::Keyboard::Update>()
        .regist_update_system<nickel::HandleInputEvents>()
        .regist_update_system<UpdateSystem>();
}