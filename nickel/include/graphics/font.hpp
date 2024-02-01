#pragma once

#include "common/handle.hpp"
#include "common/manager.hpp"
#include "graphics/texture.hpp"
#include "common/asset.hpp"

// TODO: ugly fwd, encapsolute them
typedef struct FT_FaceRec_*  FT_Face;
typedef struct FT_GlyphSlotRec_*  FT_GlyphSlot;

namespace nickel {

class Font;

using FontHandle = Handle<Font>;


class Font final : public Asset {
public:
    friend class FontManager;

    static Font Null;

    Font(const toml::table&);
    explicit Font(const std::filesystem::path& filename);
    Font() = default;

    ~Font();

    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;
    Font(Font&&) = default;
    Font& operator=(Font&&) = default;

    FT_GlyphSlot GetGlyph(uint64_t c, int size) const;
    FT_Face GetFace() const { return face_; }

    explicit operator bool() const {
        return face_ != nullptr;
    }

    toml::table Save2Toml() const override;

private:
    FT_Face face_{};
};

template <>
std::unique_ptr<Font> LoadAssetFromMeta(const toml::table&);

class FontManager final : public Manager<Font> {
public:
    static FileType GetFileType() { return FileType::Font; }

    FontHandle Load(const std::filesystem::path& filename);
};

struct Character {
    const cgmath::Vec2 size;
    const cgmath::Vec2 bearing;
    const cgmath::Vec2 advance;
    std::unique_ptr<Texture> texture;

    explicit Character(const FT_GlyphSlot&);
};

class TextCache final {
public:
    static TextCache Null;

    void Push(Character&& c) { texts_.emplace_back(std::move(c)); }

    auto& Texts() const { return texts_; }

    void Clear() { texts_.clear(); }

private:
    std::vector<Character> texts_;
};

void FontSystemInit();
void FontSystemShutdown();

}  // namespace nickel