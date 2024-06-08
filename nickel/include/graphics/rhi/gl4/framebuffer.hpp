#pragma once

#include "graphics/rhi/impl/framebuffer.hpp"
#include "graphics/rhi/framebuffer.hpp"
#include "graphics/rhi/gl4/glpch.hpp"

namespace nickel::rhi::gl4 {

class FramebufferImpl: public rhi::FramebufferImpl {
public:
    explicit FramebufferImpl(const RenderPass::Descriptor&);
    ~FramebufferImpl();

    void Bind() const;
    void Unbind() const;

    GLuint id = 0;

    auto& GetAttachmentIDs() const { return attachments_; }

private:
    std::vector<uint32_t> attachments_;
};

}