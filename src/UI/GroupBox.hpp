// GroupBox.hpp — a reusable titled panel that groups related controls for DPF NanoVG UIs.
//
// It draws a rounded panel with a caption (e.g. "Oscillator") and acts as a container: child
// controls are parented to it, and each is placed in an explicit cell of the panel's grid
// (setGrid). Cells may span more than one grid row, which is what lets one column hold two stacked
// controls while the controls beside them stay full height — a plain row-wrap cannot express that.
//
//   setGrid(2, 4);                                       // 2 rows x 4 columns
//   addControl(voices,    0, 0);                         // column 0, top half
//   addControl(monoMode,  1, 0);                         // column 0, bottom half
//   addControl(glide,     0, 1, /*span=*/2);             // column 1, full height
//
// A grid row may carry a short caption (setRowLabel) for cases where the control labels alone would
// not say which row is which (two identical ADSRs in one panel). Captions sit in a narrow gutter at
// the LEFT of the panel, vertically centred on their row; the gutter is reserved only when some row
// has a caption. Captions and multi-row spans are not combined in this UI.
//
// Positions are window-absolute in DGL (a subwidget's coords are not offset by its parent),
// so relayout() places children relative to the box's own absolute position. Mouse events
// still reach the nested controls: the default Widget::onMouse forwards to subwidgets, and
// this box does not override it.

#ifndef STRUCKSYNTH_GROUPBOX_HPP
#define STRUCKSYNTH_GROUPBOX_HPP

#include "NanoVG.hpp"

#include <string>
#include <vector>

START_NAMESPACE_DGL

class GroupBox : public NanoSubWidget
{
public:
    explicit GroupBox(Widget* parent)
        : NanoSubWidget(parent)
    {
        fHasFont = loadSharedResources(); // DPF's bundled DejaVu Sans
    }

    void setTitle(const std::string& title) { fTitle = title; }
    void setAccentColor(Color color)        { fAccent = color; }
    // Scales font size AND the title-strip/padding geometry together — scaling font alone
    // (without the strip it sits in) is what causes titles to overflow into the content area.
    void setUiScale(float s)
    {
        fUiScale     = s;
        fPadding     = static_cast<int>(10 * s);
        fTitleHeight = static_cast<int>(28 * s);
    }

    // The panel's grid: every control's cell must lie inside it (see kPanelRows/kPanelCols).
    void setGrid(int rows, int cols)
    {
        fGridRows = rows;
        fGridCols = cols;
    }

    // Optional per-column width weights, one per column. Leaving this empty gives equal columns.
    void setColumnWeights(const std::vector<int>& weights) { fColWeights = weights; }

    // Register a control (already parented to this box) at a grid cell. `span` is how many grid rows
    // the cell covers.
    void addControl(SubWidget* control, int row, int col, int span = 1)
    {
        fControls.push_back({ control, row, col, span < 1 ? 1 : span });
    }

    // Optional caption for `row`, e.g. "Filter" / "Amp" for two stacked envelopes. If ANY row has a
    // caption, every row in this panel reserves the same caption band, so the knobs stay aligned.
    void setRowLabel(int row, const std::string& text)
    {
        if (row < 0)
            return;
        if (static_cast<size_t>(row) >= fRowLabels.size())
            fRowLabels.resize(static_cast<size_t>(row) + 1);
        fRowLabels[static_cast<size_t>(row)] = text;
    }

    // The inner content rectangle (window-absolute), below the title strip and inside padding.
    // For panels that lay their children out themselves rather than via relayout().
    void contentBounds(int& x, int& y, int& w, int& h) const
    {
        x = getAbsoluteX() + fPadding;
        y = getAbsoluteY() + fTitleHeight;
        w = static_cast<int>(getWidth())  - 2 * fPadding;
        h = static_cast<int>(getHeight()) - fTitleHeight - fPadding;
    }

    // Place every registered control in its grid cell.
    void relayout()
    {
        if (fControls.empty() || fGridRows <= 0 || fGridCols <= 0)
            return;

        const int contentX = getAbsoluteX() + fPadding;
        const int contentY = getAbsoluteY() + fTitleHeight;
        const int contentW = static_cast<int>(getWidth())  - 2 * fPadding;
        const int contentH = static_cast<int>(getHeight()) - fTitleHeight - fPadding;
        if (contentW <= 0 || contentH <= 0)
            return;

        bool anyCaption = false;
        for (const std::string& label : fRowLabels) {
            if (!label.empty()) { anyCaption = true; break; }
        }
        const int gutterW  = anyCaption ? static_cast<int>(kRowCaptionWidth * fUiScale) : 0;
        const int gridX    = contentX + gutterW;
        const int gridW    = contentW - gutterW;
        if (gridW <= 0)
            return;

        const int rowH = contentH / fGridRows;
        if (rowH <= 0)
            return;   // too small to lay out; leave the controls where they are

        // Column geometry. Equal widths unless the panel asked for weighted ones — a modulation
        // section needs a wide column for "Pulse Width" sitting beside a narrow one for a knob.
        std::vector<int> colX(static_cast<size_t>(fGridCols) + 1, 0);
        if (static_cast<int>(fColWeights.size()) == fGridCols) {
            int total = 0;
            for (int weight : fColWeights) total += weight;
            int acc = 0;
            for (int c = 0; c < fGridCols; ++c) {
                colX[static_cast<size_t>(c)] = acc;
                acc += (gridW * fColWeights[static_cast<size_t>(c)]) / total;
            }
            colX[static_cast<size_t>(fGridCols)] = gridW;
        } else {
            for (int c = 0; c <= fGridCols; ++c)
                colX[static_cast<size_t>(c)] = (gridW * c) / fGridCols;
        }

        fLabelDraws.clear();

        for (const Control& cell : fControls) {
            if (cell.control == nullptr)
                continue;
            const int cellX = gridX + colX[static_cast<size_t>(cell.col)];
            const int cellW = colX[static_cast<size_t>(cell.col) + 1]
                            - colX[static_cast<size_t>(cell.col)];
            if (cellW <= 0)
                continue;
            const int h = cell.span * rowH;
            cell.control->setSize(static_cast<uint>(cellW), static_cast<uint>(h));
            cell.control->setAbsolutePos(cellX, contentY + cell.row * rowH);
        }

        for (int row = 0; row < static_cast<int>(fRowLabels.size()) && row < fGridRows; ++row) {
            if (fRowLabels[static_cast<size_t>(row)].empty())
                continue;
            // Widget-local coords: onNanoDisplay draws relative to this box, not the window.
            fLabelDraws.push_back({ fPadding + gutterW / 2,
                                    fTitleHeight + row * rowH + rowH / 2,
                                    fRowLabels[static_cast<size_t>(row)] });
        }
    }

protected:
    void onNanoDisplay() override
    {
        const float w = getWidth();
        const float h = getHeight();

        // panel fill (slightly raised from the window background)
        beginPath();
        roundedRect(1.5f, 1.5f, w - 3.0f, h - 3.0f, 8.0f);
        fillColor(Color(24, 32, 50));
        fill();

        // border
        beginPath();
        roundedRect(1.5f, 1.5f, w - 3.0f, h - 3.0f, 8.0f);
        strokeColor(Color(52, 64, 88));
        strokeWidth(1.5f);
        stroke();

        if (!fHasFont)
            return;

        fontFace(NANOVG_DEJAVU_SANS_TTF);

        // title
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fontSize(13.0f * fUiScale);
        fillColor(fAccent);
        text(12.0f * fUiScale, fTitleHeight * 0.5f + 3.0f, fTitle.c_str(), nullptr);

        // per-row captions (e.g. the two envelopes sharing this panel)
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        fontSize(12.0f * fUiScale);
        fillColor(Color(146, 150, 158));
        for (const LabelDraw& label : fLabelDraws)
            text(label.x, label.y, label.text.c_str(), nullptr);
    }

private:
    struct Control {
        SubWidget* control;
        int        row;
        int        col;
        int        span;
    };

    struct LabelDraw {
        int         x;
        int         y;
        std::string text;
    };

    static constexpr float kRowCaptionWidth = 40.0f;    // logical px at 1.0 scale

    std::vector<Control>     fControls;
    std::vector<std::string> fRowLabels;
    std::vector<LabelDraw>   fLabelDraws;
    std::vector<int>         fColWeights;
    std::string              fTitle;
    Color                    fAccent { 150, 200, 210 };
    bool                     fHasFont = false;
    float                    fUiScale = 1.0f;
    int                      fGridRows = 1;
    int                      fGridCols = 1;

    int fPadding     = 10;
    int fTitleHeight = 28;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GroupBox)
};

END_NAMESPACE_DGL

#endif // STRUCKSYNTH_GROUPBOX_HPP
