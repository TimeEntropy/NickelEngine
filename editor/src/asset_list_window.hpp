#pragma once

#include "imgui_plugin.hpp"
#include "nickel.hpp"
#include "widget.hpp"

/**
 * @brief determine how to show selected asset
 *
 * @tparam T
 * @return true     need to show this asset
 * @return false    don't need to show this asset
 */
template <typename T>
void ShowAsset(nickel::Handle<T>, const T&) {}

template <>
void ShowAsset<nickel::Texture>(nickel::TextureHandle handle,
                                const nickel::Texture&);

template <>
void ShowAsset<nickel::Font>(nickel::FontHandle handle, const nickel::Font&);

template <>
void ShowAsset<nickel::Tilesheet>(nickel::TilesheetHandle handle,
                                  const nickel::Tilesheet&);

template <typename T>
class AssetListWindow : public PopupWindow {
public:
    using HandleType = nickel::Handle<T>;

    using SelectCallbackFn = std::function<void(HandleType handle)>;

    explicit AssetListWindow(const std::string& title) : PopupWindow(title) {}

    void SetSelectCallback(SelectCallbackFn fn) { fn_ = fn; }

protected:
    void update() override {
        if (ImGui::BeginPopupModal(GetTitle().c_str(), &show_)) {
            auto& datas = gWorld->res<nickel::AssetManager>()
                              ->SwitchManager<T>()
                              .AllDatas();
            if (datas.empty()) {
                ImGui::Text("no asset");
                ImGui::EndPopup();
                return;
            }

            HandleType selectedHandle;
            T* selectedAsset = nullptr;

            for (auto&& [handle, elem] : datas) {
                if (ImGui::Selectable(elem->RelativePath().string().c_str(),
                                      false)) {
                    if (fn_) {
                        fn_(handle);
                    }

                    Hide();
                    ImGui::CloseCurrentPopup();
                }

                if (ImGui::IsItemHovered()) {
                    selectedHandle = handle;
                    selectedAsset = elem.get();
                }
            }

            if (selectedAsset) {
                ShowAsset(selectedHandle, *selectedAsset);
            }

            ImGui::EndPopup();
        }
    }

protected:
    SelectCallbackFn fn_;
};

using TextureAssetListWindow = AssetListWindow<nickel::Texture>;
using FontAssetListWindow = AssetListWindow<nickel::Font>;
using TilesheetAssetListWindow = AssetListWindow<nickel::Tilesheet>;