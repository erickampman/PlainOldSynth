// src/UI/PlainOldSynthUI.cpp — the GUI: six labelled panels of the reusable StruckSynth
// Knob/Dropdown/GroupBox kit, plus this platform's CLAP `gui` extension vtable.
//
// DPF is used here as a pure UI layer. The plugin itself is a raw CLAP plugin (src/plugin.cpp), so
// there is no dpf_add_plugin-generated UI to piggyback on: this file constructs DPF's UIExporter
// against whatever parent window the host hands us in clap_plugin_gui->set_parent(), exactly the
// way PolygraphModularUnified's src/gui_dgl.cpp does. Two consequences worth knowing:
//
//   * The UI reads and writes parameter values through the C seam in src/plugin_gui.h (read a value,
//     and begin/set/end a gesture), not through DPF's plugin side — DPF has no plugin instance here.
//     The UIExporter's edit/set callbacks are wired to that same seam, so the ordinary UI code below
//     can still use editParameter()/setParameterValue() and have it reach the host as automation.
//   * DPF's embedded-UI model needs a host-driven ~60fps timer to pump the pugl/NanoVG event loop
//     even inside a native parent view, so the gui vtable registers one on first show; the host's
//     timer callback arrives in pos_gui_on_timer() and drives UIExporter::plugin_idle().

#include "DistrhoUI.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "src/DistrhoUIInternal.hpp"   // DPF's UIExporter (DPF_ROOT_DIR/distrho is on the include path)

#include "Dropdown.hpp"
#include "EmbeddedResources.hpp"       // build-time codegen: Unlikelyware logo PNG bytes
#include "GroupBox.hpp"
#include "Knob.hpp"
#include "Logo.hpp"
#include "PopupMenu.hpp"

#include "DistrhoPluginInfo.h"
#include "PlainOldSynthParams.h"
#include "plugin_gui.h"

START_NAMESPACE_DISTRHO

using namespace plainoldsynth;

using DGL_NAMESPACE::Color;
using DGL_NAMESPACE::Dropdown;
using DGL_NAMESPACE::GroupBox;
using DGL_NAMESPACE::Knob;
using DGL_NAMESPACE::Logo;
using DGL_NAMESPACE::PopupMenu;

// One accent per panel, so the eye can group the controls the way the signal flows.
static inline Color accentFor(PanelId panel)
{
    switch (panel) {
    case kPanelOsc:       return Color(214, 168,  92);   // amber
    case kPanelFilter:    return Color(120, 190, 140);   // green
    case kPanelFilterEnv: return Color(110, 176, 178);   // teal
    case kPanelAmpEnv:    return Color(168, 140, 214);   // violet
    case kPanelLfo:       return Color(150, 166, 182);   // steel
    default:              return Color(214, 132,  96);   // copper
    }
}

// ------------------------------------------------------------------------------------------------
class PlainOldSynthUI : public UI,
                        public Knob::Callback,
                        public Dropdown::Callback,
                        public PopupMenu::Callback
{
public:
    PlainOldSynthUI()
        : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT)
    {
        setGeometryConstraints(735, 465, true, false);   // keep the design ratio, 0.75x minimum
        fHasFont = loadSharedResources();                // DPF's bundled DejaVu Sans

        // UIExporter set uiData->dspPtr (our clap_plugin_t) before calling createUI(), so the seam
        // is already usable here.
        fPlugin = static_cast<const clap_plugin_t*>(getPluginInstancePointer());

        for (uint32_t i = 0; i < kParamCount; ++i)
            fValues[i] = fPlugin != nullptr ? float(pos_param_value(fPlugin, i)) : kParams[i].def;

        fLogo = std::make_unique<Logo>(this, EmbeddedResources::Unlikelyware_Final_ColorData,
                                       EmbeddedResources::Unlikelyware_Final_ColorDataSize);

        for (uint8_t p = 0; p < kPanelCount; ++p) {
            fGroups[p] = std::make_unique<GroupBox>(this);
            fGroups[p]->setTitle(kPanelNames[p]);
            fGroups[p]->setAccentColor(accentFor(PanelId(p)));
        }

        for (uint32_t i = 0; i < kParamCount; ++i) {
            GroupBox* group = fGroups[kParams[i].panel].get();
            if (kParams[i].choices != nullptr) {
                auto dropdown = std::make_unique<Dropdown>(group, this);
                dropdown->setId(i);
                dropdown->setLabel(kParams[i].name);
                dropdown->setChoices(choicesFor(i));
                dropdown->setIndex(int(fValues[i] + 0.5f));
                group->addControl(dropdown.get());
                fDropdowns[i] = std::move(dropdown);
            } else {
                const ParamSpec& spec = kParams[i];
                auto knob = std::make_unique<Knob>(group, this);
                knob->setId(i);
                knob->setLabel(spec.name);
                knob->setAccentColor(accentFor(spec.panel));
                knob->configure(spec.min, spec.max, spec.logarithmic, spec.def);
                knob->setRealValue(fValues[i], false);
                knob->setFormatter(formatterFor(i));
                group->addControl(knob.get());
                fKnobs[i] = std::move(knob);
            }
        }

        // The factory preset selector belongs to the whole patch, so it lives in the header strip
        // rather than inside a panel. Its id is a sentinel past the real parameter range.
        fPresetDropdown = std::make_unique<Dropdown>(this, this);
        fPresetDropdown->setId(kPresetDropdownId);
        fPresetDropdown->setLabel("Preset");
        {
            std::vector<std::string> names;
            const uint32_t count = pos_preset_count();
            for (uint32_t i = 0; i < count; ++i) names.emplace_back(pos_preset_name(i));
            fPresetDropdown->setChoices(names);
        }
        fPresetDropdown->setPlaceholder("--");
        fPresetDropdown->setIndex(-1);

        fMenu = std::make_unique<PopupMenu>(this, this);   // created last: draws on top

        layout();
    }

protected:
    /* -- host → UI -------------------------------------------------------- */
    void parameterChanged(uint32_t index, float value) override
    {
        if (index >= kParamCount) return;
        fValues[index] = value;
        if (fKnobs[index] != nullptr) {
            fKnobs[index]->setRealValue(value, false);
            fKnobs[index]->repaint();
        }
        if (fDropdowns[index] != nullptr)
            fDropdowns[index]->setIndex(int(value + 0.5f));
    }

    /* -- knob → host ------------------------------------------------------ */
    void knobDragStarted(Knob* knob) override  { editParameter(knob->getId(), true); }
    void knobDragFinished(Knob* knob) override { editParameter(knob->getId(), false); }

    void knobValueChanged(Knob* knob, float value) override
    {
        const uint32_t index = knob->getId();
        fValues[index] = value;
        setParameterValue(index, value);   // → the seam → the engine (and the host, as automation)
    }

    /* -- dropdowns + their popup menu ------------------------------------- */
    void dropdownClicked(Dropdown* dropdown) override
    {
        int x = 0, y = 0, w = 0, h = 0;
        dropdown->boxRect(x, y, w, h);
        fMenu->openFor(dropdown->getId(), dropdown->choices(), dropdown->index(),
                       x, y + h, y, w, int(getHeight()));
    }

    void menuSelected(PopupMenu* menu, int index) override
    {
        const uint32_t id = menu->tag();
        if (id == kPresetDropdownId) { applyPreset(index); return; }
        if (id >= kParamCount) return;

        editParameter(id, true);
        setParameterValue(id, float(index));
        editParameter(id, false);

        fValues[id] = float(index);
        if (fDropdowns[id] != nullptr) fDropdowns[id]->setIndex(index);
    }

    /* -- presets ---------------------------------------------------------- */
    void applyPreset(int index)
    {
        if (index < 0 || index >= int(pos_preset_count())) return;
        if (fPlugin != nullptr) pos_preset_apply(fPlugin, uint32_t(index));

        // pos_preset_apply() resets every parameter to its default before applying the preset, so
        // re-read the whole table rather than only the values the preset names.
        for (uint32_t i = 0; i < kParamCount; ++i) {
            const float value = fPlugin != nullptr ? float(pos_param_value(fPlugin, i)) : kParams[i].def;
            fValues[i] = value;
            if (fKnobs[i] != nullptr) { fKnobs[i]->setRealValue(value, false); fKnobs[i]->repaint(); }
            if (fDropdowns[i] != nullptr) fDropdowns[i]->setIndex(int(value + 0.5f));
        }
        if (fPresetDropdown != nullptr) fPresetDropdown->setIndex(index);
    }

    /* -- drawing / layout ------------------------------------------------- */
    void onNanoDisplay() override
    {
        const float w = getWidth(), h = getHeight();
        beginPath(); rect(0, 0, w, h); fillColor(Color(22, 24, 29)); fill();

        if (fHasFont) {
            const float s = w / 980.0f;
            fontFace(NANOVG_DEJAVU_SANS_TTF);
            textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
            fontSize(21 * s);
            fillColor(Color(224, 190, 150));
            text(fTitleX, 21 * s, "PlainOldSynth", nullptr);
            fillColor(Color(150, 154, 162));
            fontSize(12 * s);
            text(fTitleX + 152 * s, 23 * s, "tri / saw / pulse", nullptr);
        }
    }

    void onResize(const ResizeEvent& ev) override { UI::onResize(ev); layout(); }

private:
    void layout()
    {
        const int W = getWidth(), H = getHeight();
        const float s = W / 980.0f;
        const int margin = int(14 * s);
        const int header = int(50 * s);
        const int gap    = int(9 * s);

        const int logoH = int(23 * s);
        fLogo->setHeightPreservingAspect(float(logoH));
        fLogo->setAbsolutePos(margin, int(8 * s));
        fTitleX = margin + int(fLogo->getWidth()) + int(12 * s);
        fMenu->setUiScale(s);

        const int presetW = int(210 * s);
        const int presetH = int(42 * s);
        fPresetDropdown->setUiScale(s);
        fPresetDropdown->setSize(uint(presetW), uint(presetH));
        fPresetDropdown->setAbsolutePos(W - margin - presetW, int(4 * s));

        // Six panels: 3 across, 2 down.
        const int gridY = header;
        const int gridH = H - gridY - margin;
        const int rowH  = (gridH - gap) / 2;
        const int colW  = (W - 2 * margin - 2 * gap) / 3;
        const int row0  = gridY, row1 = gridY + rowH + gap;

        auto place = [&](PanelId panel, int column, int row) {
            GroupBox* group = fGroups[panel].get();
            group->setUiScale(s);
            group->setAbsolutePos(margin + column * (colW + gap), row);
            group->setSize(uint(colW), uint(rowH));
        };
        place(kPanelOsc,       0, row0);
        place(kPanelFilter,    1, row0);
        place(kPanelFilterEnv, 2, row0);
        place(kPanelAmpEnv,    0, row1);
        place(kPanelLfo,       1, row1);
        place(kPanelGlobal,    2, row1);

        // Knobs are bounded by their panel's row height (they need room for label + value text);
        // GroupBox::relayout() then spreads them across the panel width.
        const float knobDiameter = std::min(70.0f * s, rowH * 0.42f);
        for (uint32_t i = 0; i < kParamCount; ++i) {
            if (fKnobs[i] != nullptr)     fKnobs[i]->setDiameter(knobDiameter, s);
            if (fDropdowns[i] != nullptr) fDropdowns[i]->setUiScale(s);
        }
        for (uint8_t p = 0; p < kPanelCount; ++p)
            fGroups[p]->relayout();
    }

    static std::vector<std::string> choicesFor(uint32_t index)
    {
        std::vector<std::string> out;
        const char* const* choices = kParams[index].choices;
        if (choices == nullptr) return out;
        for (int i = 0; choices[i] != nullptr; ++i) out.emplace_back(choices[i]);
        return out;
    }

    static std::function<std::string(float)> formatterFor(uint32_t index)
    {
        const std::string unit = kParams[index].unit;
        const bool integer = kParams[index].integer;
        const bool seconds = (unit == "s");
        return [unit, integer, seconds](float value) {
            char buffer[32];
            if (integer)          std::snprintf(buffer, sizeof(buffer), "%d", int(value + (value < 0.0f ? -0.5f : 0.5f)));
            else if (seconds)     value < 1.0f
                                      ? std::snprintf(buffer, sizeof(buffer), "%.0f ms", value * 1000.0f)
                                      : std::snprintf(buffer, sizeof(buffer), "%.2f s", value);
            else if (!unit.empty()) std::snprintf(buffer, sizeof(buffer), "%.2f %s", value, unit.c_str());
            else                  std::snprintf(buffer, sizeof(buffer), "%.2f", value);
            return std::string(buffer);
        };
    }

    const clap_plugin_t* fPlugin = nullptr;

    std::unique_ptr<GroupBox>  fGroups[kPanelCount];
    std::unique_ptr<Knob>      fKnobs[kParamCount];
    std::unique_ptr<Dropdown>  fDropdowns[kParamCount];
    std::unique_ptr<Dropdown>  fPresetDropdown;
    std::unique_ptr<PopupMenu> fMenu;
    std::unique_ptr<Logo>      fLogo;

    float fValues[kParamCount] = {};
    bool  fHasFont = false;
    int   fTitleX = 14;

    // Sentinel id for the preset selector, past the real parameter range so menuSelected() can tell
    // it apart from a per-parameter dropdown.
    static constexpr uint32_t kPresetDropdownId = kParamCount;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlainOldSynthUI)
};

UI* createUI()
{
    return new PlainOldSynthUI();
}

// ================================================================================================
// CLAP `gui` extension
// ================================================================================================
// Modelled on DPF's own ClapUI (DistrhoPluginCLAP.cpp) and PolygraphModularUnified's gui_dgl.cpp:
// gui_create() only allocates a holder, because UIExporter cannot be constructed until the host
// hands us a parent window in set_parent().

namespace {

struct GuiState {
    const clap_plugin_t* plugin = nullptr;
    std::unique_ptr<UIExporter> exporter;
    uintptr_t parentWindow = 0;
    uint32_t width  = DISTRHO_UI_DEFAULT_WIDTH;
    uint32_t height = DISTRHO_UI_DEFAULT_HEIGHT;
    bool timerRegistered = false;
    clap_id timerId = 0;
};

#ifdef __APPLE__
constexpr const char* kWindowApi = CLAP_WINDOW_API_COCOA;
#else
constexpr const char* kWindowApi = CLAP_WINDOW_API_X11;
#endif

bool gui_is_api_supported(const clap_plugin_t*, const char* api, bool isFloating)
{
    return !isFloating && std::strcmp(api, kWindowApi) == 0;
}

bool gui_get_preferred_api(const clap_plugin_t*, const char** api, bool* isFloating)
{
    *api = kWindowApi;
    *isFloating = false;
    return true;
}

bool gui_create(const clap_plugin_t* plugin, const char* api, bool isFloating)
{
    if (!gui_is_api_supported(plugin, api, isFloating)) return false;
    auto* gui = new GuiState();
    gui->plugin = plugin;
    pos_plugin_set_gui(plugin, gui);   // plugin.cpp owns the pointer and hands it back to us
    return true;
}

void gui_destroy(const clap_plugin_t* plugin)
{
    auto* gui = static_cast<GuiState*>(pos_plugin_get_gui(plugin));
    pos_plugin_set_gui(plugin, nullptr);
    if (gui == nullptr) return;

    if (gui->timerRegistered) {
        const clap_host_t* host = pos_plugin_host(plugin);
        const auto* timer = static_cast<const clap_host_timer_support_t*>(
            host->get_extension(host, CLAP_EXT_TIMER_SUPPORT));
        if (timer != nullptr) timer->unregister_timer(host, gui->timerId);
    }
    delete gui;   // the UIExporter is destroyed here, which tears the UI down
}

// The UIExporter's edit/set callbacks. `ptr` is whatever was passed as UIExporter's first argument —
// we pass the clap_plugin_t, so these forward straight into the seam.
void onEditParam(void* ptr, uint32_t index, bool started)
{
    const auto* plugin = static_cast<const clap_plugin_t*>(ptr);
    if (started) pos_param_begin_gesture(plugin, index);
    else         pos_param_end_gesture(plugin, index);
}

void onSetParam(void* ptr, uint32_t index, float value)
{
    pos_param_set_from_gui(static_cast<const clap_plugin_t*>(ptr), index, double(value));
}

bool gui_set_scale(const clap_plugin_t*, double) { return false; }   // logical size API (cocoa): no-op

bool gui_get_size(const clap_plugin_t* plugin, uint32_t* width, uint32_t* height)
{
    auto* gui = static_cast<GuiState*>(pos_plugin_get_gui(plugin));
    if (gui == nullptr) return false;
    if (gui->exporter) {
        // CLAP_WINDOW_API_COCOA reports logical points while UIExporter::getWidth/Height() are
        // physical pixels on Retina; dividing by the scale factor is what DPF's own ClapUI::getSize()
        // does, and skipping it makes the host allocate a container twice too large.
        const double scale = gui->exporter->getScaleFactor();
        *width  = uint32_t(gui->exporter->getWidth()  / scale);
        *height = uint32_t(gui->exporter->getHeight() / scale);
    } else {
        *width  = gui->width;
        *height = gui->height;
    }
    return true;
}

bool gui_can_resize(const clap_plugin_t*) { return true; }

bool gui_get_resize_hints(const clap_plugin_t*, clap_gui_resize_hints_t* hints)
{
    hints->can_resize_horizontally = true;
    hints->can_resize_vertically   = true;
    hints->preserve_aspect_ratio   = false;
    hints->aspect_ratio_width      = 0;
    hints->aspect_ratio_height     = 0;
    return true;
}

bool gui_adjust_size(const clap_plugin_t*, uint32_t*, uint32_t*) { return true; }   // accept as-is

bool gui_set_size(const clap_plugin_t* plugin, uint32_t width, uint32_t height)
{
    auto* gui = static_cast<GuiState*>(pos_plugin_get_gui(plugin));
    if (gui == nullptr) return false;
    gui->width  = width;
    gui->height = height;
    if (gui->exporter) {
        // Host sizes are logical points; UIExporter wants physical pixels (see gui_get_size).
        const double scale = gui->exporter->getScaleFactor();
        gui->exporter->setWindowSizeFromHost(uint(width * scale), uint(height * scale));
    }
    return true;
}

bool gui_set_parent(const clap_plugin_t* plugin, const clap_window_t* window)
{
    auto* gui = static_cast<GuiState*>(pos_plugin_get_gui(plugin));
    if (gui == nullptr || std::strcmp(window->api, kWindowApi) != 0) return false;

    // cocoa is a void*, x11 is already an integer handle — hence the different casts.
#ifdef __APPLE__
    gui->parentWindow = reinterpret_cast<uintptr_t>(window->cocoa);
#else
    gui->parentWindow = static_cast<uintptr_t>(window->x11);
#endif

    if (!gui->exporter) {
        gui->exporter.reset(new UIExporter(
            const_cast<clap_plugin_t*>(plugin),   // callbacksPtr → the edit/set callbacks above
            gui->parentWindow,                    // winId: the host's embed target
            DEFAULT_SAMPLE_RATE,                  // the UI does not need the real rate
            onEditParam, onSetParam,
            nullptr, nullptr, nullptr, nullptr,   // state / note / size / file callbacks: unused
            nullptr,                              // bundlePath
            const_cast<clap_plugin_t*>(plugin),   // dspPtr → UI::getPluginInstancePointer()
            0.0,                                  // scaleFactor: 0 = auto
            // kTypeClassic, not kTypeAuto: matches DPF's own CLAP wrapper for this embedded case.
            DGL_NAMESPACE::Application::kTypeClassic));
    }
    return true;
}

bool gui_set_transient(const clap_plugin_t*, const clap_window_t*) { return false; }   // embedded only

void gui_suggest_title(const clap_plugin_t*, const char*) {}

bool gui_show(const clap_plugin_t* plugin)
{
    auto* gui = static_cast<GuiState*>(pos_plugin_get_gui(plugin));
    if (gui == nullptr || !gui->exporter) return false;

    if (!gui->timerRegistered) {
        const clap_host_t* host = pos_plugin_host(plugin);
        const auto* timer = static_cast<const clap_host_timer_support_t*>(
            host->get_extension(host, CLAP_EXT_TIMER_SUPPORT));
        if (timer != nullptr && timer->register_timer(host, 16 /* ~60fps */, &gui->timerId))
            gui->timerRegistered = true;
    }
    return true;
}

bool gui_hide(const clap_plugin_t* plugin)
{
    auto* gui = static_cast<GuiState*>(pos_plugin_get_gui(plugin));
    if (gui == nullptr || !gui->exporter) return false;

    if (gui->timerRegistered) {
        const clap_host_t* host = pos_plugin_host(plugin);
        const auto* timer = static_cast<const clap_host_timer_support_t*>(
            host->get_extension(host, CLAP_EXT_TIMER_SUPPORT));
        if (timer != nullptr) timer->unregister_timer(host, gui->timerId);
        gui->timerRegistered = false;
    }
    return true;
}

const clap_plugin_gui_t kGui = {
    .is_api_supported  = gui_is_api_supported,
    .get_preferred_api = gui_get_preferred_api,
    .create            = gui_create,
    .destroy           = gui_destroy,
    .set_scale         = gui_set_scale,
    .get_size          = gui_get_size,
    .can_resize        = gui_can_resize,
    .get_resize_hints  = gui_get_resize_hints,
    .adjust_size       = gui_adjust_size,
    .set_size          = gui_set_size,
    .set_parent        = gui_set_parent,
    .set_transient     = gui_set_transient,
    .suggest_title     = gui_suggest_title,
    .show              = gui_show,
    .hide              = gui_hide,
};

} // namespace

extern "C" const clap_plugin_gui_t* pos_gui_extension(void) { return &kGui; }

// Host automation → an open editor. Called on the main thread by plugin.cpp.
extern "C" void pos_gui_push_param(void* guiPtr, uint32_t index, double value)
{
    auto* gui = static_cast<GuiState*>(guiPtr);
    if (gui != nullptr && gui->exporter)
        gui->exporter->parameterChanged(index, float(value));
}

// CLAP timer-support → pump the DPF/pugl event loop (see this file's header comment).
extern "C" void pos_gui_on_timer(const clap_plugin_t* plugin, clap_id)
{
    auto* gui = static_cast<GuiState*>(pos_plugin_get_gui(plugin));
    if (gui != nullptr && gui->exporter)
        gui->exporter->plugin_idle();
}

END_NAMESPACE_DISTRHO
