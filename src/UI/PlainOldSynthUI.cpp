// src/UI/PlainOldSynthUI.cpp — the GUI: labelled panels of the reusable StruckSynth
// Knob/Dropdown/GroupBox kit (the envelopes are ModicumFM's draggable EnvelopeGraph), plus this
// platform's CLAP `gui` extension vtable.
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
#include "EnvelopeGraph.hpp"
#include "GroupBox.hpp"
#include "Knob.hpp"
#include "Logo.hpp"
#include "PopupMenu.hpp"

#include "DistrhoPluginInfo.h"
#include "Effects/ModDelay.hpp"      // formatFxTime(): the Time readout depends on the effect Mode
#include "PlainOldSynthParams.h"
#include "plugin_gui.h"

START_NAMESPACE_DISTRHO

using namespace plainoldsynth;

using DGL_NAMESPACE::Color;
using DGL_NAMESPACE::Dropdown;
using DGL_NAMESPACE::EnvelopeGraph;
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
    case kPanelEnv:       return Color(110, 176, 178);   // teal
    case kPanelLfo:       return Color(150, 166, 182);   // steel
    default:              return Color(214, 132,  96);   // copper
    }
}

// ------------------------------------------------------------------------------------------------
class PlainOldSynthUI : public UI,
                        public Knob::Callback,
                        public Dropdown::Callback,
                        public PopupMenu::Callback,
                        public ParamEditCallback
{
public:
    PlainOldSynthUI()
        : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT)
    {
        setGeometryConstraints(900, 563, true, false);   // keep the design ratio, 0.75x minimum
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
            // Every control's cell is declared in the param table (kPanelRows/kPanelCols + the
            // per-param PanelCell), which is what lets Global stack two dropdowns in one column.
            fGroups[p]->setGrid(kPanelRows[p], kPanelCols[p]);
            // A modulation section's columns hold: two source menus, two level knobs, and then the
            // two widest menus in the UI ("Multiply", "Pulse Width"). Give the menu column extra
            // width so those labels are not clipped.
            if (p >= kPanelMod1 && p <= kPanelMod3)
                fGroups[p]->setColumnWeights({ 3, 3, 4 });
        }

        // Both ADSRs share kPanelEnv, one envelope per row, each drawn as a draggable EnvelopeGraph
        // (ModicumFM's) filling its row, so the panel is one column wide on screen. kPanelCols
        // still says 4: that is the panel's width weight in its band. The rows carry captions to
        // say which envelope is which.
        fGroups[kPanelEnv]->setGrid(kPanelRows[kPanelEnv], 1);
        fGroups[kPanelEnv]->setRowLabel(0, "1");
        fGroups[kPanelEnv]->setRowLabel(1, "2/Amp");
        fEnvGraphs[0] = makeEnvelope(0, kParamFilterAttack, kParamFilterDecay, kParamFilterSustain,
                                     kParamFilterRelease);
        fEnvGraphs[1] = makeEnvelope(1, kParamAmpAttack, kParamAmpDecay, kParamAmpSustain,
                                     kParamAmpRelease);

        for (uint32_t i = 0; i < kParamCount; ++i) {
            const ParamSpec& spec = kParams[i];
            if (fEnvFor[i] != nullptr) continue;   // edited on an EnvelopeGraph, not a knob
            GroupBox* group = fGroups[spec.panel].get();
            const PanelCell& cell = spec.cell;
            if (spec.choices != nullptr) {
                auto dropdown = std::make_unique<Dropdown>(group, this);
                dropdown->setId(i);
                dropdown->setLabel(spec.name);
                dropdown->setChoices(choicesFor(i));
                dropdown->setIndex(int(fValues[i] + 0.5f));
                group->addControl(dropdown.get(), cell.row, cell.col, cell.span);
                fDropdowns[i] = std::move(dropdown);
            } else {
                auto knob = std::make_unique<Knob>(group, this);
                knob->setId(i);
                knob->setLabel(spec.name);
                knob->setAccentColor(accentFor(spec.panel));
                knob->configure(spec.min, spec.max, spec.logarithmic, spec.def);
                knob->setRealValue(fValues[i], false);
                knob->setFormatter(i == kParamFxTime ? fxTimeFormatter() : formatterFor(i));
                group->addControl(knob.get(), cell.row, cell.col, cell.span);
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
        if (fEnvFor[index] != nullptr)
            fEnvFor[index]->setParamValue(index, value);
        if (index == kParamFxMode) refreshFxTimeReadout();
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

    /* -- envelope graphs → host ------------------------------------------- */
    void paramEditBegin(uint32_t id) override { editParameter(id, true); }
    void paramEditEnd(uint32_t id) override   { editParameter(id, false); }
    void paramEditChange(uint32_t id, float value) override
    {
        fValues[id] = value;
        setParameterValue(id, value);
    }

    /* -- dropdowns + their popup menu ------------------------------------- */
    void dropdownClicked(Dropdown* dropdown) override
    {
        int x = 0, y = 0, w = 0, h = 0;
        dropdown->boxRect(x, y, w, h);
        fMenu->openFor(dropdown->getId(), dropdown->choices(), dropdown->index(),
                       x, y + h, y, w, int(getHeight()));
    }

    // A click outside the plugin window never reaches us, so treat losing focus to the host (or
    // another app) as "clicked elsewhere" and close any open menu. Clicks inside the window are
    // handled by the menu itself.
    void uiFocus(bool focus, DGL_NAMESPACE::CrossingMode mode) override
    {
        UI::uiFocus(focus, mode);
        if (!focus && fMenu != nullptr)
            fMenu->dismiss();
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
        if (id == kParamFxMode) refreshFxTimeReadout();
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
            if (fEnvFor[i] != nullptr) fEnvFor[i]->setParamValue(i, value);
        }
        if (fPresetDropdown != nullptr) fPresetDropdown->setIndex(index);
        refreshFxTimeReadout();
    }

    // The Time knob's value is 0..1 whose meaning depends on the effect Mode, so a Mode change has
    // to redraw its readout even though the knob itself did not move.
    void refreshFxTimeReadout()
    {
        if (fKnobs[kParamFxTime] != nullptr) fKnobs[kParamFxTime]->repaint();
    }

    std::function<std::string(float)> fxTimeFormatter()
    {
        return [this](float value) {
            char buffer[32];
            formatFxTime(int(fValues[kParamFxMode] + 0.5f), value, buffer, sizeof(buffer));
            return std::string(buffer);
        };
    }

    /* -- drawing / layout ------------------------------------------------- */
    void onNanoDisplay() override
    {
        const float w = getWidth(), h = getHeight();
        beginPath(); rect(0, 0, w, h); fillColor(Color(16, 22, 36)); fill();

        if (fHasFont) {
            const float s = w / 1200.0f;
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
        const float s = W / 1200.0f;
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

        // The panels are deliberately NOT a uniform grid: they sit in three bands, because they do
        // not all need the same height. Band 0 holds the two-knob-row panels (Oscillator, Envelopes,
        // LFO), band 1 the single-row ones (Filter, Global), band 2 the three modulation sections.
        // The Effect panel is a column down the right of bands 1 and 2 (kPanelBandSpanning).
        // Which band a panel is in and how many rows it needs are declared in the param table
        // (kPanelBand / kPanelRowUnits), so this function does not name panels one at a time.
        const int gridY  = header;
        const int gridH  = H - gridY - margin;
        const int titleH = int(28 * s);
        const int bandRows = totalRowUnits();

        // Rows are capped rather than stretched to fill: filling is what left each knob floating in
        // excess vertical space. The leftover widens the gaps between bands instead.
        const int rowHFill = (gridH - kBandCount * titleH - (kBandCount - 1) * gap) / bandRows;
        const int rowH     = std::min(rowHFill, int(150 * s));
        const int slack    = gridH - (bandRows * rowH + kBandCount * titleH + (kBandCount - 1) * gap);
        const int bandGap  = std::max(int(3 * s), gap + slack / (kBandCount - 1));

        auto place = [&](PanelId panel, int x, int y, int w, int h) {
            GroupBox* group = fGroups[panel].get();
            group->setUiScale(s);
            group->setAbsolutePos(x, y);
            group->setSize(uint(w), uint(h));
        };

        // Lay one band out left to right, dividing its width by the given weights. A panel's weight
        // is its column count, so a panel of four narrow controls gets more room than one of two and
        // every control ends up roughly the same size.
        struct BandPanel { PanelId panel; int weight; };
        auto placeBand = [&](int x0, int bandW, int y, int h, const BandPanel* panels, int count) {
            int totalWeight = 0;
            for (int i = 0; i < count; ++i) totalWeight += panels[i].weight;
            const int avail = bandW - (count - 1) * gap;
            int x = x0, used = 0;
            for (int i = 0; i < count; ++i) {
                // Last panel takes the remainder, so rounding can never leave a gap at the right edge.
                const int w = (i == count - 1) ? (avail - used)
                                               : (avail * panels[i].weight) / totalWeight;
                place(panels[i].panel, x, y, w, h);
                x += w + gap;
                used += w;
            }
        };

        // The Effect column is exactly as wide as a Mod section would be in a row of four, so the
        // three real Mod sections keep the width they had; bands 1 and 2 share what is left of the
        // window to its left.
        const int fullW = W - 2 * margin;
        const int fxW   = (fullW - 3 * gap) / 4;
        const int leftW = fullW - fxW - gap;
        int fxTop = 0;

        int bandY = gridY;
        for (int band = 0; band < kBandCount; ++band) {
            const int h = rowUnitsInBand(band) * rowH + titleH;
            if (band == 0) {
                const BandPanel panels[] = { {kPanelOsc, kPanelCols[kPanelOsc]},
                                             {kPanelEnv, kPanelCols[kPanelEnv]},
                                             {kPanelLfo, kPanelCols[kPanelLfo]} };
                placeBand(margin, fullW, bandY, h, panels, 3);
            } else if (band == 1) {
                fxTop = bandY;
                const BandPanel panels[] = { {kPanelFilter, kPanelCols[kPanelFilter]},
                                             {kPanelGlobal, kPanelCols[kPanelGlobal]} };
                placeBand(margin, leftW, bandY, h, panels, 2);
            } else {
                // The modulation sections are peers: equal width regardless of their column counts.
                const BandPanel panels[] = { {kPanelMod1, 1}, {kPanelMod2, 1}, {kPanelMod3, 1} };
                placeBand(margin, leftW, bandY, h, panels, 3);
                place(kPanelFx, margin + leftW + gap, fxTop, fxW, bandY + h - fxTop);
            }
            bandY += h + bandGap;
        }

        // The knob sphere is sized from the row height minus the label + readout bands it shares the
        // cell with, NOT as a fraction of the row: that keeps the sphere at full size when rows are
        // tightened, so a shorter panel means less empty space rather than smaller knobs. Knob itself
        // clamps to the cell, so a narrow cell shrinks it instead of letting it overflow.
        const float knobDiameter = std::min(70.0f * s, float(rowH) - 46.0f * s);
        for (uint32_t i = 0; i < kParamCount; ++i) {
            if (fKnobs[i] != nullptr)     fKnobs[i]->setDiameter(knobDiameter, s);
            if (fDropdowns[i] != nullptr) fDropdowns[i]->setUiScale(s);
        }
        for (auto& env : fEnvGraphs) env->setUiScale(s);
        for (uint8_t p = 0; p < kPanelCount; ++p)
            fGroups[p]->relayout();
        // The graphs fill their grid rows edge to edge; trim each so the two stand apart (ModicumFM
        // leaves the same breathing room around its own).
        const int envGap = int(8 * s);
        for (auto& env : fEnvGraphs)
            if (int(env->getHeight()) > 2 * envGap)
                env->setHeight(env->getHeight() - uint(envGap));
    }

    std::unique_ptr<EnvelopeGraph> makeEnvelope(int row, uint32_t attack, uint32_t decay,
                                                uint32_t sustain, uint32_t release)
    {
        GroupBox* group = fGroups[kPanelEnv].get();
        auto env = std::make_unique<EnvelopeGraph>(group, this);
        env->setAccentColor(accentFor(kPanelEnv));
        const auto bind = [&](EnvelopeGraph::Slot slot, uint32_t id) {
            env->setSlot(slot, id, kParams[id].min, kParams[id].max, kParams[id].def);
            env->setParamValue(id, fValues[id]);
            fEnvFor[id] = env.get();
        };
        bind(EnvelopeGraph::kAttack, attack);
        bind(EnvelopeGraph::kDecay, decay);
        bind(EnvelopeGraph::kSustain, sustain);
        bind(EnvelopeGraph::kRelease, release);
        group->addControl(env.get(), row, 0);
        return env;
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
    std::unique_ptr<EnvelopeGraph> fEnvGraphs[2];               // filter (adsr1), amp (adsr2)
    EnvelopeGraph*                 fEnvFor[kParamCount] = {};   // param → the graph that edits it

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
    // The editor size in the HOST's units — logical points on macOS (CLAP cocoa), pixels on X11.
    // This, not the UIExporter's pixel size, is the source of truth: the pixel size is derived from
    // it with the live scale (applySize), so it can be re-derived when the window changes screens.
    uint32_t width  = DISTRHO_UI_DEFAULT_WIDTH;
    uint32_t height = DISTRHO_UI_DEFAULT_HEIGHT;
    double appliedScale = 0.0;   // the scale the UIExporter's pixel size was last derived with
    bool timerRegistered = false;
    clap_id timerId = 0;
};

#ifdef __APPLE__
extern "C" double pos_gui_backing_scale(uintptr_t nsView);   // BackingScale_mac.mm
#endif

// Host units → UIExporter pixels. On macOS this is the backing scale of the window the editor is in
// RIGHT NOW, deliberately not UIExporter::getScaleFactor(): DPF caches that once, possibly from the
// wrong screen, and never updates it, while pugl maps mouse positions with the live value — see
// BackingScale_mac.mm. On X11, CLAP sizes are already physical pixels, so there is nothing to convert
// (matching DPF's own CLAP wrapper, which scales on macOS only).
double liveScale(const GuiState* gui)
{
#ifdef __APPLE__
    return pos_gui_backing_scale(gui->parentWindow);
#else
    (void)gui;
    return 1.0;
#endif
}

// Size the UIExporter to gui->width x gui->height host units at the live scale. pugl re-reads the
// screen's scale while doing this and re-sends the pixel size, so it also re-syncs DPF after a move
// between screens even when the size in points did not change.
void applySize(GuiState* gui)
{
    if (!gui->exporter) return;
    const double scale = liveScale(gui);
    gui->appliedScale = scale;
    gui->exporter->setWindowSizeFromHost(uint(std::lround(gui->width  * scale)),
                                         uint(std::lround(gui->height * scale)));
}

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
    // Host units, straight from the source of truth. Converting the UIExporter's pixel size back
    // with DPF's cached scale is what made the host allocate a container of the wrong size.
    *width  = gui->width;
    *height = gui->height;
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
    applySize(gui);
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
        // The UIExporter starts at DISTRHO_UI_DEFAULT_WIDTH x HEIGHT *pixels*; put it at the host-unit
        // size instead, at the scale of the window it is actually in.
        applySize(gui);
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
    if (gui == nullptr || !gui->exporter) return;
    // The host window moved to a screen with a different backing scale (or the size was set before
    // it was in a window): re-derive the pixel size so drawing and mouse agree again.
    if (liveScale(gui) != gui->appliedScale) applySize(gui);
    gui->exporter->plugin_idle();
}

END_NAMESPACE_DISTRHO
