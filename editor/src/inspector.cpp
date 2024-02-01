#include "inspector.hpp"
#include "context.hpp"

void InspectorWindow::update() {
    auto& reg = *nickel::ECS::Instance().World().cur_registry();
    auto entity = reg.res<EditorContext>()->entityListWindow.GetSelected();
    auto cmds = reg.commands();

    auto& types = mirrow::drefl::all_typeinfo();

    if (!reg.alive(entity)) {
        return;
    }

    int id = 0;
    for (auto [name, typeInfo] : types) {
        if (reg.has(entity, typeInfo)) {
            auto& attrs = typeInfo->attributes();

            if (std::find(attrs.begin(), attrs.end(),
                          nickel::EditorNodisplay) != attrs.end()) {
                continue;
            }

            auto data = reg.get_mut(entity, typeInfo);

            auto& methods = ComponentShowMethods::Instance();
            auto func = methods.Find(typeInfo);

            ImGui::PushID(id++);
            if (ImGui::Button("delete")) {
                cmds.remove(entity, typeInfo);
                ImGui::PopID();
                continue;
            }
            ImGui::PopID();
            ImGui::SameLine();
            if (ImGui::CollapsingHeader(typeInfo->name().c_str())) {
                if (func) {
                    func(typeInfo, typeInfo->name(), data, reg);
                }
            }
        }
    }

    // show add componet button
    static std::vector<const char*> items;
    items.clear();

    auto& spawnMethods = SpawnComponentMethods::Instance();

    for (auto& [type, spawnFn] : spawnMethods.Methods()) {
        items.push_back(type->name().data());
    }

    ImGui::Separator();

    static int selectedItem = -1;

    typename SpawnComponentMethods::spawn_fn spawn = nullptr;

    ImGui::Combo("components", &selectedItem, items.data(), items.size());

    auto it = spawnMethods.Methods().begin();
    int i = 0;

    while (it != spawnMethods.Methods().end() && i != selectedItem) {
        it++;
        i++;
    }

    static bool replaceHintOpen = false;
    static bool shouldSpawn = false;

    if (ImGui::Button("add component") && it != spawnMethods.Methods().end()) {
        if (reg.has(entity, it->first)) {
            replaceHintOpen = true;
        } else {
            shouldSpawn = true;
        }
    }

    if (replaceHintOpen) {
        ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowFocus();
        if (ImGui::Begin("spawn hint", 0)) {
            ImGui::Text("component already exists, do you want replace it?");
            if (ImGui::Button("yes")) {
                shouldSpawn = true;
                replaceHintOpen = false;
            }

            ImGui::SameLine();
            if (ImGui::Button("cancel")) {
                replaceHintOpen = false;
            }

        }
    }

    if (shouldSpawn) {
        it->second(cmds, entity, reg);
        shouldSpawn = false;
    }
}