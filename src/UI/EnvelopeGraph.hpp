// EnvelopeGraph.hpp — a graphical ADSR editor: draws the envelope's shape and lets you drag its
// points instead of turning four knobs. Ported from ModicumFM's EnvelopeGraph (same look, same
// handling), without its relative and disabled modes, which nothing here uses. The Delay point is
// kept so the widget stays close to the original; PlainOldSynth leaves it unbound.
//
// Time axis: one shared axis for all stages, compressed with u = log(1 + t / 10 ms) so a 5 ms
// attack and a 10 s release are both visible, fitted to the widget, with a minimum width per
// stage so every point stays grabbable. A fixed-width plateau stands for the sustain.
//
// The curves are ModicumFM's stylised ones (linear attack, exponential decay into the sustain and
// release to zero), not a trace of ADSRHelper: its RC attack and its decay toward zero, which
// reaches a high sustain almost at once, drawn literally read as a spike and a notch.
//
// Points (drag):
//   Delay    end of the flat start           sideways = delay time   (only if bound)
//   Attack   the peak                        sideways = attack time
//   Decay    the corner into the sustain     sideways = decay time, up/down = sustain level
//   Release  the end                         sideways = release time
// Ctrl = fine (1/10 speed), like the knobs. Shift-click or double-click resets the point to its
// default(s). Dragging is relative (pixels → change in compressed time), so the axis re-fitting
// under the mouse as times change never makes a point jump. A readout names the point and its
// value while hovering or dragging. Edits go through ParamEditCallback (begin/change/end per
// param, so hosts record automation like any knob).

#ifndef PLAINOLDSYNTH_ENVELOPEGRAPH_HPP
#define PLAINOLDSYNTH_ENVELOPEGRAPH_HPP

#include "NanoVG.hpp"
#include "ParamEdit.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

START_NAMESPACE_DGL

class EnvelopeGraph : public NanoSubWidget
{
public:
    enum Slot { kDelay = 0, kAttack, kDecay, kSustain, kRelease, kSlotCount };
    static constexpr uint32_t kNoParam = 0xffffffffu;

    EnvelopeGraph(Widget* parent, plainoldsynth::ParamEditCallback* cb)
        : NanoSubWidget(parent), fCb(cb)
    {
        fHasFont = loadSharedResources();
    }

    // Bind a slot to a plugin param (leave kDelay unbound for a plain ADSR).
    void setSlot(Slot s, uint32_t paramId, float min, float max, float def)
    {
        fSlots[s] = {paramId, min, max, def, def};
    }
    // Host / preset update; returns false if `paramId` isn't one of ours.
    bool setParamValue(uint32_t paramId, float v)
    {
        for (auto& sl : fSlots)
            if (sl.id == paramId) { sl.value = clamp(v, sl.min, sl.max); repaint(); return true; }
        return false;
    }
    void setAccentColor(Color c) { fAccent = c; }
    void setUiScale(float s)     { fUiScale = s; }

protected:
    void onNanoDisplay() override
    {
        const Geometry g = geometry();
        const float w = getWidth(), h = getHeight();

        beginPath(); roundedRect(0.5f, 0.5f, w - 1.0f, h - 1.0f, 4.0f);
        fillColor(Color(20, 22, 27)); fill();
        strokeColor(Color(46, 50, 58)); strokeWidth(1.0f); stroke();

        // Envelope shape: (delay) flat → attack → decay → sustain → release. Stroked, then
        // re-traced and closed along the baseline for the fill.
        traceShape(g);
        strokeColor(withA(fAccent, 0.95f)); strokeWidth(1.6f); stroke();
        traceShape(g);
        closePath();
        fillColor(withA(fAccent, 0.16f)); fill();

        // Points.
        for (int p = 0; p < kPointCount; ++p)
        {
            if (p == kPtDelay && !hasDelay()) continue;
            float px, py;
            pointPos(g, p, px, py);
            const bool hot = (p == fDragPoint) || (fDragPoint < 0 && p == fHoverPoint);
            const bool idleDelay = p == kPtDelay && fSlots[kDelay].value <= 0.0f && !hot;
            beginPath(); circle(px, py, (hot ? 5.0f : 3.6f) * fUiScale);
            fillColor(hot ? Color(244, 247, 250) : (idleDelay ? withA(fAccent, 0.35f) : fAccent));
            fill();
        }

        // Readout for the hovered / dragged point.
        const int shown = fDragPoint >= 0 ? fDragPoint : fHoverPoint;
        if (fHasFont && shown >= 0)
        {
            fontFace(NANOVG_DEJAVU_SANS_TTF);
            fontSize(11.0f * fUiScale);
            textAlign(ALIGN_RIGHT | ALIGN_TOP);
            fillColor(Color(224, 228, 234));
            text(w - 5.0f * fUiScale, 3.0f * fUiScale, readout(shown).c_str(), nullptr);
        }
    }

    bool onMouse(const MouseEvent& ev) override
    {
        if (ev.button != 1)
            return false;
        if (ev.press)
        {
            if (!contains(ev.pos))
                return false;
            const int p = hitTest(geometry(), ev.pos.getX(), ev.pos.getY());
            if (p < 0)
                return true; // our area, but not on a point
            const bool dbl = p == fLastClickPoint && fLastClickTime > 0 && ev.time > fLastClickTime
                          && ev.time - fLastClickTime <= 300;
            fLastClickTime = ev.time; fLastClickPoint = p;
            if (dbl || (ev.mod & kModifierShift))
            {
                resetPoint(p);
                fLastClickTime = 0;
                return true;
            }
            fDragPoint = p;
            fLastX = ev.pos.getX(); fLastY = ev.pos.getY();
            forEachSlot(p, [&](Slot s) { fCb->paramEditBegin(fSlots[s].id); });
            repaint();
            return true;
        }
        if (fDragPoint >= 0)
        {
            forEachSlot(fDragPoint, [&](Slot s) { fCb->paramEditEnd(fSlots[s].id); });
            fDragPoint = -1;
            repaint();
            return true;
        }
        return false;
    }

    bool onMotion(const MotionEvent& ev) override
    {
        if (fDragPoint < 0)
        {
            // Hover: highlight the point under the mouse; never consume the event.
            const int p = contains(ev.pos) ? hitTest(geometry(), ev.pos.getX(), ev.pos.getY()) : -1;
            if (p != fHoverPoint) { fHoverPoint = p; repaint(); }
            return false;
        }
        const float speed = (ev.mod & kModifierControl) ? 0.1f : 1.0f;
        const float dx = static_cast<float>(ev.pos.getX() - fLastX);
        const float dy = static_cast<float>(ev.pos.getY() - fLastY);
        fLastX = ev.pos.getX(); fLastY = ev.pos.getY();

        const Slot timeSlot = pointTimeSlot(fDragPoint);
        if (dx != 0.0f)
        {
            // Move the time in compressed space.
            const float u = warp(fSlots[timeSlot].value) + dx * kWarpPerPixel / fUiScale * speed;
            setSlotValue(timeSlot, unwarp(u));
        }
        if (fDragPoint == kPtDecay && dy != 0.0f)
        {
            const Geometry g = geometry();
            const float span = std::fmax(10.0f, g.bottom - g.top);
            setSlotValue(kSustain, clamp(fSlots[kSustain].value - dy / span * speed, 0.0f, 1.0f));
        }
        return true;
    }

private:
    struct Param { uint32_t id = kNoParam; float min = 0, max = 1, def = 0, value = 0; };
    enum Point { kPtDelay = 0, kPtAttack, kPtDecay, kPtRelease, kPointCount };
    struct Geometry { float x0, xDelay, xAttack, xDecay, xSustain, xRelease, top, bottom; };

    static constexpr float kTau          = 0.010f;  // warp knee: 10 ms
    static constexpr float kWarpPerPixel = 0.035f;  // ~230 px across 1 ms .. 30 s
    static constexpr int   kCurveSteps   = 16;

    plainoldsynth::ParamEditCallback* fCb;
    Param  fSlots[kSlotCount];
    Color  fAccent { 60, 200, 90 };
    float  fUiScale = 1.0f;
    bool   fHasFont = false;
    int    fDragPoint = -1, fHoverPoint = -1, fLastClickPoint = -1;
    double fLastX = 0, fLastY = 0;
    uint   fLastClickTime = 0;

    bool hasDelay() const { return fSlots[kDelay].id != kNoParam; }

    static float clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
    static float warp(float t)   { return std::log1p(std::fmax(0.0f, t) / kTau); }
    static float unwarp(float u) { return kTau * std::expm1(std::fmax(0.0f, u)); }
    // Exponential fall from 1 at f = 0 to exactly 0 at f = 1.
    static float expCurve(float f)
    {
        const float end = std::exp(-5.0f);
        return (std::exp(-5.0f * f) - end) / (1.0f - end);
    }
    static Color withA(Color c, float a) { c.alpha = a; return c; }

    Geometry geometry() const
    {
        const float s    = fUiScale;
        const float pad  = 7.0f * s;
        const float band = 15.0f * s;   // readout strip above the plot
        Geometry g;
        g.top    = band;
        g.bottom = getHeight() - pad;
        const float left = pad, width = getWidth() - 2.0f * pad;
        const float susW = 0.16f * width;
        const float minW = 7.0f * s;

        const int   n = hasDelay() ? 4 : 3;
        const float u[4] = { hasDelay() ? warp(fSlots[kDelay].value) : 0.0f,
                             warp(fSlots[kAttack].value), warp(fSlots[kDecay].value),
                             warp(fSlots[kRelease].value) };
        const float uSum  = u[0] + u[1] + u[2] + u[3] + 1e-6f;
        const float avail = std::fmax(0.0f, width - susW - n * minW);
        const auto  segW  = [&](int i) { return minW + avail * u[i] / uSum; };

        g.x0       = left;
        g.xDelay   = hasDelay() ? g.x0 + segW(0) : g.x0;
        g.xAttack  = g.xDelay + segW(1);
        g.xDecay   = g.xAttack + segW(2);
        g.xSustain = g.xDecay + susW;
        g.xRelease = g.xSustain + segW(3);
        return g;
    }

    void traceShape(const Geometry& g)
    {
        const float sus = fSlots[kSustain].value;
        const auto  yAt = [&](float level) { return g.bottom - level * (g.bottom - g.top); };
        beginPath();
        moveTo(g.x0, g.bottom);
        lineTo(g.xDelay, g.bottom);
        lineTo(g.xAttack, g.top);
        for (int i = 1; i <= kCurveSteps; ++i)
        {
            const float f = float(i) / kCurveSteps;
            lineTo(g.xAttack + f * (g.xDecay - g.xAttack), yAt(sus + (1.0f - sus) * expCurve(f)));
        }
        lineTo(g.xSustain, yAt(sus));
        for (int i = 1; i <= kCurveSteps; ++i)
        {
            const float f = float(i) / kCurveSteps;
            lineTo(g.xSustain + f * (g.xRelease - g.xSustain), yAt(sus * expCurve(f)));
        }
    }

    void pointPos(const Geometry& g, int p, float& x, float& y) const
    {
        const float susY = g.bottom - fSlots[kSustain].value * (g.bottom - g.top);
        switch (p)
        {
        case kPtDelay:   x = g.xDelay;   y = g.bottom; break;
        case kPtAttack:  x = g.xAttack;  y = g.top;    break;
        case kPtDecay:   x = g.xDecay;   y = susY;     break;
        default:         x = g.xRelease; y = g.bottom; break;
        }
    }

    // Nearest point within reach; ties (points drawn on top of each other) go to the later stage,
    // which is the one that can move away.
    int hitTest(const Geometry& g, double mx, double my) const
    {
        const float reach = 11.0f * fUiScale;
        int best = -1; float bestD = reach * reach;
        for (int p = 0; p < kPointCount; ++p)
        {
            if (p == kPtDelay && !hasDelay()) continue;
            float px, py; pointPos(g, p, px, py);
            const float d = float((px - mx) * (px - mx) + (py - my) * (py - my));
            if (d <= bestD) { bestD = d; best = p; }
        }
        return best;
    }

    static Slot pointTimeSlot(int p)
    {
        switch (p)
        {
        case kPtDelay:  return kDelay;
        case kPtAttack: return kAttack;
        case kPtDecay:  return kDecay;
        default:        return kRelease;
        }
    }

    template <typename F> void forEachSlot(int p, F f) const
    {
        f(pointTimeSlot(p));
        if (p == kPtDecay) f(kSustain);
    }

    void setSlotValue(Slot s, float v)
    {
        Param& sl = fSlots[s];
        v = clamp(v, sl.min, sl.max);
        if (v == sl.value) return;
        sl.value = v;
        fCb->paramEditChange(sl.id, v);
        repaint();
    }

    void resetPoint(int p)
    {
        forEachSlot(p, [&](Slot s) {
            fCb->paramEditBegin(fSlots[s].id);
            setSlotValue(s, fSlots[s].def);
            fCb->paramEditEnd(fSlots[s].id);
        });
    }

    static std::string fmtTime(float t)
    {
        char b[24];
        t < 1.0f ? std::snprintf(b, sizeof(b), "%.0f ms", t * 1000.0f)
                 : std::snprintf(b, sizeof(b), "%.2f s", t);
        return b;
    }

    std::string readout(int p) const
    {
        switch (p)
        {
        case kPtDelay:  return "Delay " + fmtTime(fSlots[kDelay].value);
        case kPtAttack: return "Attack " + fmtTime(fSlots[kAttack].value);
        case kPtDecay:
        {
            char b[16]; std::snprintf(b, sizeof(b), "%.2f", fSlots[kSustain].value);
            return "Decay " + fmtTime(fSlots[kDecay].value) + "  Sustain " + b;
        }
        default:        return "Release " + fmtTime(fSlots[kRelease].value);
        }
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EnvelopeGraph)
};

END_NAMESPACE_DGL

#endif // PLAINOLDSYNTH_ENVELOPEGRAPH_HPP
