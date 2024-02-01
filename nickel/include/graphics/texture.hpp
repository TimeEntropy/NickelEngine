#pragma once

#include "common/asset.hpp"
#include "common/cgmath.hpp"
#include "graphics/gogl.hpp"
#include "common/handle.hpp"
#include "common/manager.hpp"
#include "common/filetype.hpp"


/**
 * @addtogroup resource-manager
 * @{
 */

namespace nickel {

class Renderer2D;
class Texture;

using TextureHandle = Handle<Texture>;

class Texture final : public Asset {
public:
    friend class Renderer2D;
    friend class TextureManager;
    friend class Material2D;

    static Texture Null;

    explicit Texture(const toml::table& tbl);
    explicit Texture(const std::filesystem::path& filename,
                     const gogl::Sampler&,
                     gogl::Format fmt = gogl::Format::RGBA,
                     gogl::Format gpuFmt = gogl::Format::RGBA);
    Texture(void*, int w, int h, const gogl::Sampler& sampler,
            gogl::Format fmt = gogl::Format::RGBA,
            gogl::Format gpuFmt = gogl::Format::RGBA);
    Texture() = default;

    Texture(const Texture&) = delete;
    Texture(Texture&&) = default;
    Texture& operator=(Texture&&) = default;

    Texture& operator=(const Texture&) = delete;

    explicit operator bool() const { return texture_ != nullptr; }

    int Width() const { return w_; }

    int Height() const { return h_; }

    cgmath::Vec2 Size() const {
        return cgmath::Vec2{static_cast<float>(w_), static_cast<float>(h_)};
    }

    void* Raw() const {
        return texture_ ? reinterpret_cast<void*>(texture_->Id()) : 0;
    }

    auto& Sampler() const { return sampler_; }

    toml::table Save2Toml() const override;

private:
    std::unique_ptr<gogl::Texture> texture_ = nullptr;
    gogl::Sampler sampler_;
    int w_ = 0;
    int h_ = 0;
};

template <>
std::unique_ptr<Texture> LoadAssetFromMeta(const toml::table&);

class TextureManager final : public Manager<Texture> {
public:
    static FileType GetFileType() { return FileType::Image; }

    TextureHandle Load(
        const std::filesystem::path& filename,
        const gogl::Sampler& = gogl::Sampler::CreateLinearRepeat());
    TextureHandle LoadSVG(const std::filesystem::path& filename,
                          const gogl::Sampler&,
                          std::optional<cgmath::Vec2> size = std::nullopt);
    bool Replace(TextureHandle, const std::filesystem::path& filename,
                 const gogl::Sampler&);
    std::unique_ptr<Texture> CreateSolitary(
        void* data, int w, int h, const gogl::Sampler&,
        gogl::Format fmt = gogl::Format::RGBA,
        gogl::Format gpuFmt = gogl::Format::RGBA);
};

}  // namespace nickel
