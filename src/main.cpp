#include "menu.hpp"
#include <cmath>
#include <cstddef>
#include <dlfcn.h>
#include <functional>
#include <glaze/json.hpp>
#include <libhat.hpp>
#include <link.h>
#include <memory>
#include <print>
#include <span>

struct Config {
    bool  enable      = true;
    float guiScale    = 2.f;
    float maxGuiScale = 5.f;
    bool  round       = true;
};

static Config config;

static glz::sv configPath = "/data/data/com.mojang.minecraftpe/guiscale_config.json";

static void loadConfig() {
    std::string buffer;
    if (auto ec = glz::read_file_json(config, configPath, buffer))
        std::println(stderr, "Failed to load GUI scale config: {}", glz::format_error(ec, buffer));
}

static void saveConfig() {
    std::string buffer;
    if (auto ec = glz::write_file_json<glz::opts{.prettify = true}>(config, configPath, buffer))
        std::println(stderr, "Failed to save GUI scale config: {}", glz::format_error(ec, buffer));
}

extern "C" [[gnu::visibility("default")]] void mod_preinit() {
    auto menuLib = dlopen("libmcpelauncher_menu.so", 0);

    addMenu     = reinterpret_cast<decltype(addMenu)>(dlsym(menuLib, "mcpelauncher_addmenu"));
    showWindow  = reinterpret_cast<decltype(showWindow)>(dlsym(menuLib, "mcpelauncher_show_window"));
    closeWindow = reinterpret_cast<decltype(closeWindow)>(dlsym(menuLib, "mcpelauncher_close_window"));

    loadConfig();
    saveConfig();

    MenuEntryABI menuSubEntries[3];

    menuSubEntries[0] = {
        .name     = "Enable",
        .selected = [](void*) { return config.enable; },
        .click =
            [](void*) {
                config.enable = !config.enable;
                saveConfig();
            },
    };

    menuSubEntries[1] = {
        .name  = "Change GUI scale",
        .click = [](void*) {
            ControlABI controls[3];

            controls[0].type             = 2;
            controls[0].data.sliderfloat = {
                .label    = "GUI Scale",
                .min      = 0.5f,
                .def      = config.guiScale,
                .max      = config.maxGuiScale,
                .user     = nullptr,
                .onChange = [](void*, float value) { config.guiScale = value; },
            };

            controls[1].type           = 1;
            controls[1].data.sliderint = {
                .label    = "Round value (0: off, 1: on)",
                .min      = 0,
                .def      = config.round,
                .max      = 1,
                .user     = nullptr,
                .onChange = [](void*, int value) { config.round = value; },
            };

            controls[2].type        = 0;
            controls[2].data.button = {
                .label   = "Save",
                .user    = nullptr,
                .onClick = [](void*) { saveConfig(); },
            };

            showWindow("GUI Scale", false, nullptr, [](void*) { saveConfig(); }, std::size(controls), controls);
        },
    };

    menuSubEntries[2] = {
        .name  = "Reload config",
        .click = [](void*) {
            closeWindow("GUI Scale");
            loadConfig();
        },
    };

    MenuEntryABI menuEntry{
        .name       = "GUI Scale",
        .length     = std::size(menuSubEntries),
        .subentries = menuSubEntries,
    };

    addMenu(1, &menuEntry);
}

struct ScreenSizeData {
    float totalScreenSizeX;
    float totalScreenSizeY;
    float screenSizeX;
    float screenSizeY;
    float guiSizeX;
    float guiSizeY;
};

struct GuiData {
    std::byte      pad0[0x40];
    ScreenSizeData screenSizeData;
    bool           screenSizeDataValid;
    float          guiScale;
    float          invGuiScale;
};

struct GuiDataPtr {
    std::shared_ptr<bool> controlBlock;
    GuiData*              ptr;
};

extern "C" [[gnu::visibility("default")]] void mod_init() {
    auto mcLib = dlopen("libminecraftpe.so", 0);

    std::span<std::byte> range1, range2;

    auto callback = [&](const dl_phdr_info& info) {
        if (auto h = dlopen(info.dlpi_name, RTLD_NOLOAD); dlclose(h), h != mcLib)
            return 0;
        range1 = {reinterpret_cast<std::byte*>(info.dlpi_addr + info.dlpi_phdr[1].p_vaddr), info.dlpi_phdr[1].p_memsz};
        range2 = {reinterpret_cast<std::byte*>(info.dlpi_addr + info.dlpi_phdr[2].p_vaddr), info.dlpi_phdr[2].p_memsz};
        return 1;
    };

    dl_iterate_phdr(
        [](dl_phdr_info* info, size_t, void* data) {
            return (*static_cast<decltype(callback)*>(data))(*info);
        },
        &callback);

    auto ClientInstance_typeinfo_name = hat::find_pattern(range1, hat::object_to_signature("14ClientInstance")).get();
    auto ClientInstance_typeinfo      = hat::find_pattern(range2, hat::object_to_signature(ClientInstance_typeinfo_name)).get() - sizeof(void*);
    auto ClientInstance_vtable        = hat::find_pattern(range2, hat::object_to_signature(ClientInstance_typeinfo)).get() + sizeof(void*);
    auto ClientInstance_update        = reinterpret_cast<bool (**)(void**, bool)>(ClientInstance_vtable) + 25;

    static auto ClientInstance_update_orig = *ClientInstance_update;

    *ClientInstance_update = [](void** self, bool b) {
        auto getGuiData           = static_cast<GuiDataPtr (**)(void*)>(*self)[245];
        auto forEachVisibleScreen = static_cast<void (**)(void*, std::function<bool(void*&)>, bool)>(*self)[282];

        auto& guiData        = *getGuiData(self).ptr;
        auto& screenSizeData = guiData.screenSizeData;

        float guiScale = config.round ? std::round(config.guiScale) : config.guiScale;

        if (config.enable && guiData.guiScale != guiScale) {
            guiData.guiScale        = guiScale;
            guiData.invGuiScale     = 1.f / guiScale;
            screenSizeData.guiSizeX = screenSizeData.screenSizeX * guiData.invGuiScale;
            screenSizeData.guiSizeY = screenSizeData.screenSizeY * guiData.invGuiScale;

            forEachVisibleScreen(
                self,
                [&](void*& screen) {
                    auto setSize = static_cast<void (**)(void*, const ScreenSizeData&)>(screen)[3];
                    setSize(&screen, screenSizeData);
                    return true;
                },
                true);
        }

        return ClientInstance_update_orig(self, b);
    };
}
