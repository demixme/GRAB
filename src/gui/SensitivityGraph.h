// SensitivityGraph - the FabFilter-style 5D centrepiece: one graph with every dimension's response
// curve overlaid (the selected one lit and editable, the rest dim reference lines), a row of
// dimension chips to pick one, and an inspector with that dimension's description and Min /
// Sensitivity / Max knobs. Replaces the old row of per-dimension CurveComponents.
//
// It only displays + emits edits (onChange(id, value)); the owner (MainComponent / Rise2Component)
// keeps the authoritative pending/device values and feeds live updates back by id. Curve geometry
// is shared with the legacy panel via SensitivityCurve.h.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"
#include "SensitivityCurve.h"
#include "Help.h"

#include <functional>
#include <vector>

class SensitivityGraph  : public juce::Component,
                          public juce::SettableTooltipClient,
                          private juce::Timer
{
public:
    using ChangeFn = std::function<void (juce::uint32 id, juce::int32 value)>;

    explicit SensitivityGraph (ChangeFn onChangeToUse) : change (std::move (onChangeToUse)) {}
    ~SensitivityGraph() override { stopTimer(); }

    // Mark Lift as not reported by this device (e.g. the Seaboard Block sends a fixed note-off): the
    // Lift chip dims and its inspector says so, instead of silently showing no live data.
    void setLiftUnsupported (bool b) { if (liftUnsupported != b) { liftUnsupported = b; repaint(); } }

    // A release: show a fading dot on the Lift curve at the release velocity (0..1). Lift is an event,
    // not a held value, so it pulses rather than tracking.
    void setLiftPulse (float velocity)
    {
        liftValue = juce::jlimit (0.0f, 1.0f, velocity);
        liftStartMs = juce::Time::getMillisecondCounter();
        startTimerHz (30);
        repaint();
    }

    /** Add a dimension. minId/maxId = 0 when the device exposes no editable min/max for it. */
    void addDimension (juce::uint32 id, juce::String name, juce::Colour colour,
                       int sens, int minId, int mn, int maxId, int mx)
    {
        Dim d;
        d.id = id; d.name = std::move (name); d.colour = colour;
        d.minId = minId; d.maxId = maxId;
        d.sens = sens; d.mn = mn; d.mx = mx;
        d.initSens = sens; d.initMn = mn; d.initMx = mx;
        copyMeta (d);
        dims.push_back (std::move (d));
        if (selected < 0) selected = 0;
    }

    bool isEmpty() const { return dims.empty(); }

    // Fixed velocity lives at the bottom of the inspector panel (Fred's design): a toggle, plus a
    // draggable value when it's on. The owner passes the two config ids so the graph can emit edits.
    void setFixedVelocity (juce::uint32 toggleId, bool on, juce::uint32 valueId, int value)
    {
        hasFixedVel = true; fvToggleId = toggleId; fvOn = on; fvValueId = valueId;
        fvValue = juce::jlimit (0, 127, value);
        repaint();
    }
    void setFixedVelocityOn (bool on)    { if (fvOn != on) { fvOn = on; repaint(); } }
    void setFixedVelocityValue (int v)   { v = juce::jlimit (0, 127, v); if (fvValue != v) { fvValue = v; repaint(); } }
    bool ownsFixedVel (juce::uint32 id) const { return hasFixedVel && (id == fvToggleId || id == fvValueId); }
    void updateFixedVel (juce::uint32 id, int value)   // a device-reported change to either fv id
    {
        if (! hasFixedVel) return;
        if (id == fvToggleId)      setFixedVelocityOn (value != 0);
        else if (id == fvValueId)  setFixedVelocityValue (value);
    }

    // --- by-id interface for the owner's live-update / changed / reset sites -------------------
    bool owns (juce::uint32 id) const
    {
        for (auto& d : dims)
            if (d.id == id || (d.minId != 0 && (juce::uint32) d.minId == id)
                           || (d.maxId != 0 && (juce::uint32) d.maxId == id))
                return true;
        return false;
    }
    void setValue (juce::uint32 sensId, int v)
    {
        for (auto& d : dims) if (d.id == sensId) { d.sens = juce::jlimit (0, 127, v); repaint(); return; }
    }
    void setMinMax (juce::uint32 id, int v)
    {
        for (auto& d : dims)
        {
            if (d.minId != 0 && (juce::uint32) d.minId == id) { d.mn = juce::jlimit (0, 127, v); repaint(); return; }
            if (d.maxId != 0 && (juce::uint32) d.maxId == id) { d.mx = juce::jlimit (0, 127, v); repaint(); return; }
        }
    }
    void setChanged (juce::uint32 sensId, bool c)
    {
        for (auto& d : dims) if (d.id == sensId) { if (d.changed != c) { d.changed = c; repaint(); } return; }
    }
    std::vector<juce::uint32> sensIds() const
    {
        std::vector<juce::uint32> ids; ids.reserve (dims.size());
        for (auto& d : dims) ids.push_back (d.id);
        return ids;
    }

    // One live finger's input across the 5D axes (all 0..1; glide pre-mapped to 0..1 around 0.5). The
    // graph plots a dot per finger on the selected dimension's curve - so a chord shows a dot per note.
    // The last entry is treated as the latest (it gets the dotted value guides).
    struct LiveInput { float strike = 0, glide = 0.5f, slide = 0.5f, press = 0; };
    void setLiveInputs (std::vector<LiveInput> in)
    {
        if (in.empty() && liveInputs.empty()) return;
        liveInputs = std::move (in);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        if (dims.empty()) return;
        layoutRegions();
        paintGraph (g);
        paintInspector (g);
    }

    void resized() override { layoutRegions(); }

    void timerCallback() override
    {
        if (juce::Time::getMillisecondCounter() - liftStartMs > (juce::uint32) kLiftMs) stopTimer();
        repaint();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        // Chips pick the dimension.
        for (size_t i = 0; i < chipR.size(); ++i)
            if (chipR[i].contains (e.position))
            { selected = (int) i; drag = Drag::none; repaint(); return; }

        if (dims.empty()) return;
        auto& d = dims[(size_t) selected];

        if (e.mods.isRightButtonDown()) { resetSelected(); return; }

        // Reset link in the inspector (only when there's something to revert).
        if (edited (d) && resetR.contains (e.position)) { resetSelected(); return; }

        // Fixed-velocity switch / value.
        if (hasFixedVel && fvSwitchR.contains (e.position))
        { fvOn = ! fvOn; if (change) change (fvToggleId, fvOn ? 1 : 0); repaint(); return; }
        if (hasFixedVel && fvOn && fvValueR.contains (e.position))
        { draggingFvValue = true; dragStart = fvValue; return; }

        // Knobs.
        for (auto& k : knobR)
            if (k.area.contains (e.position)) { drag = k.target; dragStart = valueFor (d, k.target); applyKnob (e); return; }

        // Clicking another dimension's curve selects it; otherwise drag the selected curve
        // (end nodes if near, else the sensitivity).
        if (plotR.toFloat().contains (e.position))
        {
            const int hit = curveAtPoint (e.position);
            if (hit >= 0 && hit != selected) { selected = hit; drag = Drag::none; repaint(); return; }
            drag = pickPlotDrag (d, e.position);
            dragStart = valueFor (d, drag);
            applyPlot (e);
        }
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (plotR.toFloat().contains (e.position)) resetSelected();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (draggingFvValue)
        {
            const int v = juce::jlimit (0, 127, dragStart - juce::roundToInt ((float) e.getDistanceFromDragStartY() * 0.6f));
            if (v != fvValue) { fvValue = v; if (change) change (fvValueId, v); repaint(); }
            return;
        }
        if (drag == Drag::none) return;
        if (drag == Drag::sens || drag == Drag::minNode || drag == Drag::maxNode) applyPlot (e);
        else applyKnob (e);
    }

    void mouseUp (const juce::MouseEvent&) override { draggingFvValue = false; drag = Drag::none; }

private:
    enum class Drag { none, sens, minNode, maxNode, knobMin, knobSens, knobMax };

    struct Dim
    {
        juce::uint32 id = 0;
        juce::String name;
        juce::Colour colour;
        int minId = 0, maxId = 0;
        int sens = 0, mn = 0, mx = 127;
        int initSens = 0, initMn = 0, initMx = 127;
        bool changed = false;
        juce::String input, output, desc;
        bool hasNodes() const { return minId != 0 && maxId != 0; }
    };

    struct Knob { juce::Rectangle<float> area; Drag target; };

    // The input axis value (0..1) a finger contributes to a given dimension's curve, or <0 to skip.
    static float inputFor (const LiveInput& li, const juce::String& name)
    {
        if (name == "Press")  return li.press;
        if (name == "Slide")  return li.slide;
        if (name == "Glide")  return li.glide;
        if (name == "Lift")   return -1.0f;   // release velocity isn't tracked live - no dot (don't mirror Strike)
        return li.strike;
    }
    // Sample a unit-space curve path's y at x (the curves are monotonic in x).
    static float yAtX (const juce::Path& path, float x)
    {
        juce::PathFlatteningIterator it (path, {}, 0.002f);
        float y = 0.0f;
        while (it.next())
            if ((x >= it.x1 && x <= it.x2) || (x >= it.x2 && x <= it.x1))
                y = it.x2 == it.x1 ? it.y2 : it.y1 + (it.y2 - it.y1) * (x - it.x1) / (it.x2 - it.x1);
        return y;
    }
    static void dashedLine (juce::Graphics& g, juce::Point<float> a, juce::Point<float> b)
    {
        const float dashes[] = { 3.0f, 3.0f };
        g.drawDashedLine (juce::Line<float> (a, b), dashes, 2, 1.0f);
    }

    // "Edited" = the selected dimension differs from the value read when the page was built (its
    // baseline). Drives the persistent chip dot and the context Reset - independent of autosave, so
    // the dot means "not the value you started with" rather than the fleeting "unsaved from device".
    static bool edited (const Dim& d)
    {
        return d.sens != d.initSens || (d.hasNodes() && (d.mn != d.initMn || d.mx != d.initMx));
    }

    // Fred's per-dimension copy (design/ui/prototype/template.html): x-axis / y-axis / description.
    static void copyMeta (Dim& d)
    {
        const auto& n = d.name;
        if      (n == "Strike") { d.input = "Strike force";    d.output = "Velocity";         d.desc = "How hard you strike a key sets its note-on velocity."; }
        else if (n == "Glide")  { d.input = "Side to side";    d.output = "Pitch bend";       d.desc = "Rocking a finger side to side bends pitch, for vibrato."; }
        else if (n == "Slide")  { d.input = "Finger position"; d.output = "Slide CC";         d.desc = "Moving a finger along the key, front to back, sends the Slide CC."; }
        else if (n == "Press")  { d.input = "Pressure";        d.output = "Pressure out";     d.desc = "Pressing into a held key sends continuous pressure."; }
        else if (n == "Lift")   { d.input = "Release speed";   d.output = "Release velocity"; d.desc = "How fast you lift off a key sets its release velocity."; }
    }

    // --- layout --------------------------------------------------------------------------------
    void layoutRegions()
    {
        auto b = getLocalBounds();
        // Fred's inspector is a fixed 240px tile; narrow only when the window can't spare it.
        const int inspW = b.getWidth() >= 700 ? 240 : juce::jlimit (190, 240, b.getWidth() / 4);
        inspR = b.removeFromRight (inspW);
        b.removeFromRight (grab::theme::space::gutter);
        graphR = b;

        // Graph internals: chip row on top, then the plot inset for the axes.
        auto gr = graphR;
        chipRow = gr.removeFromTop (30);
        gr.removeFromTop (6);
        plotR = gr.withTrimmedLeft (54).withTrimmedBottom (34).withTrimmedRight (8).withTrimmedTop (4);

        // Chip rects (measured with the body font): room for the dot, the name, its value and the
        // changed dot, so a long name like "Strike" never truncates.
        chipR.clear();
        juce::Font cf = grab::theme::type::font (grab::theme::type::Role::body);
        const int valW = juce::GlyphArrangement::getStringWidthInt (cf, "127");
        int x = chipRow.getX();
        for (auto& d : dims)
        {
            const int w = 20 + juce::GlyphArrangement::getStringWidthInt (cf, d.name) + 6 + valW + 16;
            chipR.push_back (juce::Rectangle<float> ((float) x, (float) chipRow.getY() + 2.0f, (float) w, 26.0f));
            x += w + 8;
        }

        // Knob rects in the inspector (Min / Sensitivity / Max, or just Sensitivity), inside a card.
        knobR.clear();
        if (! dims.empty())
        {
            auto& d = dims[(size_t) selected];
            auto innerB = inspR.reduced (14, 0);   // match the inspector's 14px side padding
            innerB.removeFromBottom (12);          // bottom padding

            // Fred's .kwell: a single sunken (tileInset) well pinned to the bottom of the inspector,
            // holding the knobs on top and - under a divider - the fixed-velocity row. Fixed velocity
            // is a Strike-only control (fixRow = n === 'Strike' in Fred's design).
            const bool fv = hasFixedVel && d.name == "Strike";
            const int fixH = fv ? 38 : 0;
            knobCardR = innerB.removeFromBottom (108 + fixH);

            auto win = knobCardR.reduced (8, 0);   // .kwell horizontal padding
            win.removeFromTop (8);
            win.removeFromBottom (6);
            if (fv)                // fixed-velocity row (.kfix), inside the well, under a divider
            {
                auto fvRow = win.removeFromBottom (32);
                fvDivider = { (float) knobCardR.getX() + 10.0f, (float) fvRow.getY() - 3.0f,
                              (float) knobCardR.getRight() - 10.0f, (float) fvRow.getY() - 3.0f };
                auto fr = fvRow.reduced (6, 0);
                fvSwitchR = fr.removeFromRight (34).withSizeKeepingCentre (34, 20).toFloat();
                fr.removeFromRight (8);
                if (fvOn) { fvValueR = fr.removeFromRight (32).toFloat(); fr.removeFromRight (4); }
                else        fvValueR = {};
                fvLabelR = fr.toFloat();
                win.removeFromBottom (8);           // gap above the divider
            }
            auto row = win;
            const int kw = 52;
            if (d.hasNodes())
            {
                const int gap = juce::jmax (6, (row.getWidth() - kw * 3) / 2);
                knobR.push_back ({ row.removeFromLeft (kw).toFloat(), Drag::knobMin });  row.removeFromLeft (gap);
                knobR.push_back ({ row.removeFromLeft (kw).toFloat(), Drag::knobSens }); row.removeFromLeft (gap);
                knobR.push_back ({ row.removeFromLeft (kw).toFloat(), Drag::knobMax });
            }
            else
            {
                knobR.push_back ({ row.withSizeKeepingCentre (kw, row.getHeight()).toFloat(), Drag::knobSens });
            }
        }
    }

    juce::Point<float> toPlot (juce::Point<float> p) const
    {
        auto pr = plotR.toFloat();
        return { pr.getX() + p.x * pr.getWidth(), pr.getBottom() - p.y * pr.getHeight() };
    }

    // --- painting ------------------------------------------------------------------------------
    void paintGraph (juce::Graphics& g)
    {
        using namespace grab::theme;
        auto& sel = dims[(size_t) selected];
        auto pr = plotR.toFloat();

        // The graph tile: a recessed (display) rounded tile with a 1px inner grid edge - a sibling
        // of the inspector tile, both sitting on the window gutter (Fred's graphrow).
        g.setColour (display);
        g.fillRoundedRectangle (graphR.toFloat(), radius::tile);
        g.setColour (grid);
        g.drawRoundedRectangle (graphR.toFloat().reduced (0.5f), radius::tile, 1.0f);

        // grid
        g.setColour (grid);
        for (int i = 0; i <= 4; ++i)
        {
            const float fx = pr.getX() + pr.getWidth()  * (i / 4.0f);
            const float fy = pr.getY() + pr.getHeight() * (i / 4.0f);
            g.drawVerticalLine   (juce::roundToInt (fx), pr.getY(), pr.getBottom());
            g.drawHorizontalLine (juce::roundToInt (fy), pr.getX(), pr.getRight());
        }

        const auto xform = juce::AffineTransform::scale (pr.getWidth(), -pr.getHeight())
                               .translated (pr.getX(), pr.getBottom());

        // non-selected curves: dim reference lines
        for (int i = 0; i < (int) dims.size(); ++i)
        {
            if (i == selected) continue;
            auto& d = dims[(size_t) i];
            g.setColour (d.colour.withAlpha (0.40f));
            g.strokePath (grab::sensitivityCurvePath (d.name, d.sens, (float) d.mn / 127.0f, (float) d.mx / 127.0f),
                          juce::PathStrokeType (1.5f), xform);
        }

        // selected curve: soft fill under it, then a thick stroke
        const auto path = grab::sensitivityCurvePath (sel.name, sel.sens, (float) sel.mn / 127.0f, (float) sel.mx / 127.0f);
        {
            juce::Path fill = path;
            fill.lineTo ({ 1.0f, 0.0f }); fill.lineTo ({ 0.0f, 0.0f }); fill.closeSubPath();
            g.setGradientFill (juce::ColourGradient (sel.colour.withAlpha (0.32f), pr.getCentreX(), pr.getY(),
                                                     sel.colour.withAlpha (0.0f),  pr.getCentreX(), pr.getBottom(), false));
            g.fillPath (fill, xform);
        }
        g.setColour (sel.colour);
        g.strokePath (path, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), xform);

        // Live input dots: one per held finger, riding the selected curve at that finger's input for
        // this dimension. The latest (last) also gets dotted guides to the axes with its values.
        for (size_t i = 0; i < liveInputs.size(); ++i)
        {
            const float raw = inputFor (liveInputs[i], sel.name);
            if (raw < 0.0f) continue;   // dimension has no live input (e.g. Lift)
            const float ix = juce::jlimit (0.0f, 1.0f, raw);
            const float iy = juce::jlimit (0.0f, 1.0f, yAtX (path, ix));
            const auto p = toPlot ({ ix, iy });
            const bool latest = (i + 1 == liveInputs.size());
            if (latest)
            {
                g.setColour (sel.colour.withAlpha (0.45f));
                dashedLine (g, p, { p.x, pr.getBottom() });   // down to the input axis
                dashedLine (g, p, { pr.getX(), p.y });        // across to the output axis
                g.setColour (label);
                g.setFont (grab::theme::type::font (grab::theme::type::Role::caption));
                g.drawText (juce::String (juce::roundToInt (iy * 127.0f)),
                            juce::Rectangle<float> (pr.getX() - 52.0f, p.y - 7.0f, 46.0f, 14.0f), juce::Justification::centredRight);
                g.drawText (juce::String (juce::roundToInt (ix * 127.0f)),
                            juce::Rectangle<float> (p.x - 18.0f, pr.getBottom() + 4.0f, 36.0f, 14.0f), juce::Justification::centred);
            }
            g.setColour (display);    g.fillEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre (p));
            g.setColour (sel.colour); g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (p));
        }

        // Lift pulse: a fading dot on the Lift curve at the release velocity, right after a note off.
        if (sel.name == "Lift")
        {
            const auto age = (float) (juce::Time::getMillisecondCounter() - liftStartMs);
            if (liftStartMs != 0 && age < (float) kLiftMs)
            {
                const float al = 1.0f - age / (float) kLiftMs;
                const float iy = juce::jlimit (0.0f, 1.0f, yAtX (path, liftValue));
                const auto p = toPlot ({ liftValue, iy });
                g.setColour (sel.colour.withAlpha (0.45f * al));
                dashedLine (g, p, { p.x, pr.getBottom() });
                dashedLine (g, p, { pr.getX(), p.y });
                g.setColour (display.withAlpha (al));    g.fillEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre (p));
                g.setColour (sel.colour.withAlpha (al)); g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (p));
            }
        }

        // end nodes for the selected dimension
        if (sel.hasNodes() && sel.sens > 0)
            for (auto p : { toPlot ({ 0.0f, (float) sel.mn / 127.0f }), toPlot ({ 1.0f, (float) sel.mx / 127.0f }) })
            {
                g.setColour (display);                 g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre (p));
                g.setColour (sel.colour);              g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre (p));
            }

        // axes: labels + 0 / 64 / 127 ticks
        g.setColour (label);
        g.setFont (grab::theme::type::font (grab::theme::type::Role::caption));
        g.drawText ("127", juce::Rectangle<float> (pr.getX() - 52.0f, pr.getY() - 7.0f, 46.0f, 14.0f), juce::Justification::centredRight);
        g.drawText ("0",   juce::Rectangle<float> (pr.getX() - 52.0f, pr.getBottom() - 7.0f, 46.0f, 14.0f), juce::Justification::centredRight);
        g.drawText ("0",   juce::Rectangle<float> (pr.getX() - 6.0f, pr.getBottom() + 4.0f, 24.0f, 14.0f), juce::Justification::centredLeft);
        g.drawText ("127", juce::Rectangle<float> (pr.getRight() - 24.0f, pr.getBottom() + 4.0f, 24.0f, 14.0f), juce::Justification::centredRight);
        g.drawText (sel.input, juce::Rectangle<float> (pr.getX(), pr.getBottom() + 18.0f, pr.getWidth(), 14.0f), juce::Justification::centred);
        // y-axis label, rotated
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi,
                                                             (float) graphR.getX() + 14.0f, pr.getCentreY()));
            g.drawText (sel.output, juce::Rectangle<float> ((float) graphR.getX() + 14.0f - pr.getHeight() * 0.5f,
                                                            pr.getCentreY() - 7.0f, pr.getHeight(), 14.0f),
                        juce::Justification::centred);
        }

        // chips: name + value always; only the selected one gets a lozenge background (Fred's design).
        juce::Font cf = grab::theme::type::font (grab::theme::type::Role::body);
        for (size_t i = 0; i < chipR.size(); ++i)
        {
            auto& d = dims[i];
            auto r = chipR[i];
            const bool on = (int) i == selected;
            const bool dimmed = d.name == "Lift" && liftUnsupported;   // device doesn't report lift
            if (on) { g.setColour (segmentOn); g.fillRoundedRectangle (r, r.getHeight() * 0.5f); }

            g.setColour (dimmed ? d.colour.withAlpha (0.4f) : d.colour);
            g.fillEllipse (r.getX() + 9.0f, r.getCentreY() - 4.0f, 8.0f, 8.0f);

            g.setFont (cf);
            float tx = r.getX() + 20.0f;
            const float nameW = (float) juce::GlyphArrangement::getStringWidthInt (cf, d.name);
            g.setColour (dimmed ? disabled : (on ? ink : ink2));
            g.drawText (d.name, juce::Rectangle<float> (tx, r.getY(), nameW + 2.0f, r.getHeight()), juce::Justification::centredLeft);
            tx += nameW + 6.0f;
            g.setColour (dimmed ? disabled : ink2);
            g.drawText (juce::String (d.sens), juce::Rectangle<float> (tx, r.getY(), 30.0f, r.getHeight()), juce::Justification::centredLeft);

            if (edited (d))   // persistent dot (vertically centred): differs from its page-load value
            { g.setColour (ink); g.fillEllipse (r.getRight() - 12.0f, r.getCentreY() - 2.5f, 5.0f, 5.0f); }
        }
    }

    void paintInspector (juce::Graphics& g)
    {
        using namespace grab::theme;
        if (dims.empty()) return;
        auto& d = dims[(size_t) selected];

        // The inspector is its own full-height panel (Fred's design): everything sits inside it.
        g.setColour (panel);
        g.fillRoundedRectangle (inspR.toFloat(), radius::tile);

        auto b = inspR.reduced (16, 14);

        // Fred's .insp-h: a hue dot, the name in ink (not coloured), an optional changed dot, Reset.
        auto head = b.removeFromTop (28);
        auto titleR = head.removeFromLeft (head.getWidth() - 56);
        auto dotA = titleR.removeFromLeft (10);
        titleR.removeFromLeft (8);
        g.setColour (d.colour);
        g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ (float) dotA.getCentreX(), (float) head.getCentreY() }));
        g.setColour (ink);
        g.setFont (grab::theme::type::font (grab::theme::type::Role::title));
        g.drawText (d.name, titleR, juce::Justification::centredLeft);
        if (edited (d))
        {
            const int nameW = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), d.name);
            g.fillEllipse ((float) (titleR.getX() + nameW + 8), (float) head.getCentreY() - 3.0f, 6.0f, 6.0f);
        }
        // Reset: always present (context button), lit only when the dimension has been edited.
        resetR = head.toFloat();
        g.setColour (edited (d) ? ink2 : disabled);
        g.setFont (grab::theme::type::font (grab::theme::type::Role::body));
        g.drawText ("Reset", resetR, juce::Justification::centredRight);
        b.removeFromTop (6);

        // description
        g.setColour (ink2);
        g.setFont (grab::theme::type::font (grab::theme::type::Role::body));
        g.drawFittedText (d.desc, b.removeFromTop (54), juce::Justification::topLeft, 3);

        // Lift on a device that doesn't report it: say so under the description.
        if (d.name == "Lift" && liftUnsupported)
        {
            g.setColour (disabled);
            g.setFont (grab::theme::type::font (grab::theme::type::Role::caption));
            g.drawFittedText ("Not sent by this device.", b.removeFromTop (28), juce::Justification::topLeft, 2);
        }

        // knobs, in a sunken (tileInset) well within the inspector - Fred's .kwell
        g.setColour (tileInset);
        g.fillRoundedRectangle (knobCardR.toFloat(), radius::inset);
        for (auto& k : knobR)
        {
            const int v = valueFor (d, k.target);
            const char* lab = k.target == Drag::knobMin ? "Min" : k.target == Drag::knobMax ? "Max" : "Sensitivity";
            paintKnob (g, k.area, d.colour, v, lab);
        }

        // Fixed velocity (.kfix): a divider, then label, optional value, and a switch - inside the
        // well, and only on Strike (Fred's design).
        if (hasFixedVel && d.name == "Strike")
        {
            g.setColour (divider);
            g.drawLine (fvDivider, 1.0f);
            g.setColour (fvOn ? ink : ink2);
            g.setFont (grab::theme::type::font (grab::theme::type::Role::body));
            g.drawText ("Fixed velocity", fvLabelR, juce::Justification::centredLeft);
            if (fvOn && ! fvValueR.isEmpty())
            {
                g.setColour (ink);
                g.setFont (grab::theme::type::font (grab::theme::type::Role::control));
                g.drawText (juce::String (fvValue), fvValueR, juce::Justification::centredRight);
            }
            // switch (34 x 20, Fred's .sw)
            g.setColour (fvOn ? primaryFill : switchOff);
            g.fillRoundedRectangle (fvSwitchR, fvSwitchR.getHeight() * 0.5f);
            const float kr = fvSwitchR.getHeight() - 6.0f;
            const float kx = fvOn ? fvSwitchR.getRight() - kr - 3.0f : fvSwitchR.getX() + 3.0f;
            g.setColour (fvOn ? primaryText : ink);
            g.fillEllipse (kx, fvSwitchR.getY() + 3.0f, kr, kr);
        }
    }

    void paintKnob (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour c, int v, const char* lab)
    {
        using namespace grab::theme;
        // Fred's 52px dial (authored on a 0..100 box): a dark face disc (r 32) with a wider track
        // ring around it (r 42, 4px), an outward value tick (r 14 -> 30, sitting on the face, not
        // through the centre), the value below (readout), then the caption label.
        const float kd = juce::jmin (area.getWidth(), 52.0f);
        const float s  = kd / 100.0f;
        auto dialBox = juce::Rectangle<float> (kd, kd).withCentre ({ area.getCentreX(), area.getY() + kd * 0.5f });
        const auto ctr = dialBox.getCentre();
        const float rFace = 32.0f * s;
        const float rArc  = 42.0f * s;
        const float wArc  = 7.69f * s;   // ~4px at 52
        const float a0 = juce::MathConstants<float>::pi * 1.25f;   // 225 deg: the gap sits at the bottom
        const float a1 = juce::MathConstants<float>::pi * 2.75f;   // +270 deg sweep, clockwise
        const float ang = a0 + (a1 - a0) * (float) v / 127.0f;

        g.setColour (knobFace);
        g.fillEllipse (dialBox.withSizeKeepingCentre (rFace * 2.0f, rFace * 2.0f));

        juce::Path track, val;
        track.addCentredArc (ctr.x, ctr.y, rArc, rArc, 0.0f, a0, a1,  true);
        val.addCentredArc   (ctr.x, ctr.y, rArc, rArc, 0.0f, a0, ang, true);
        g.setColour (knobTrack); g.strokePath (track, juce::PathStrokeType (wArc, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (c);         g.strokePath (val,   juce::PathStrokeType (wArc, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Outward pointer tick on the face (not through the centre).
        const float sn = std::sin (ang), cs = std::cos (ang);
        juce::Line<float> tick (ctr.x + sn * 14.0f * s, ctr.y - cs * 14.0f * s,
                                ctr.x + sn * 30.0f * s, ctr.y - cs * 30.0f * s);
        g.setColour (ink);
        g.drawLine (tick, 3.85f * s);

        // Value below the dial, then the label.
        auto rest = area.withTrimmedTop ((int) kd + 2);
        g.setColour (ink);
        g.setFont (grab::theme::type::font (grab::theme::type::Role::readout));
        g.drawText (juce::String (v), rest.removeFromTop (24), juce::Justification::centred);
        g.setColour (label);
        g.setFont (grab::theme::type::font (grab::theme::type::Role::caption));
        g.drawText (lab, rest.removeFromTop (14), juce::Justification::centredTop);
    }

    // --- interaction ---------------------------------------------------------------------------
    int valueFor (const Dim& d, Drag t) const
    {
        return (t == Drag::minNode || t == Drag::knobMin) ? d.mn
             : (t == Drag::maxNode || t == Drag::knobMax) ? d.mx
             :                                              d.sens;
    }

    // The non-selected dimension whose curve is nearest the point (within a few px), or -1.
    int curveAtPoint (juce::Point<float> pos) const
    {
        auto pr = plotR.toFloat();
        const auto xform = juce::AffineTransform::scale (pr.getWidth(), -pr.getHeight())
                               .translated (pr.getX(), pr.getBottom());
        int best = -1;
        float bestDist = 12.0f;
        for (int i = 0; i < (int) dims.size(); ++i)
        {
            if (i == selected) continue;
            auto& d = dims[(size_t) i];
            auto path = grab::sensitivityCurvePath (d.name, d.sens, (float) d.mn / 127.0f, (float) d.mx / 127.0f);
            if (path.isEmpty()) continue;
            juce::Point<float> onPath;
            path.getNearestPoint (pos, onPath, xform);
            const float dist = pos.getDistanceFrom (onPath);
            if (dist < bestDist) { bestDist = dist; best = i; }
        }
        return best;
    }

    Drag pickPlotDrag (const Dim& d, juce::Point<float> pos) const
    {
        if (d.hasNodes() && d.sens > 0)
        {
            if (pos.getDistanceFrom (toPlot ({ 0.0f, (float) d.mn / 127.0f })) < 16.0f) return Drag::minNode;
            if (pos.getDistanceFrom (toPlot ({ 1.0f, (float) d.mx / 127.0f })) < 16.0f) return Drag::maxNode;
        }
        return Drag::sens;
    }

    void applyPlot (const juce::MouseEvent& e)
    {
        auto pr = plotR.toFloat();
        int v;
        if (e.mods.isCtrlDown() || e.mods.isCommandDown())
        {
            const float dyUnits = -(float) e.getDistanceFromDragStartY() / juce::jmax (1.0f, pr.getHeight()) * 127.0f;
            v = juce::jlimit (0, 127, (int) std::lround ((float) dragStart + dyUnits * 0.25f));
        }
        else
        {
            const float f = (pr.getBottom() - e.position.y) / juce::jmax (1.0f, pr.getHeight());
            v = juce::jlimit (0, 127, (int) std::lround (f * 127.0f));
        }
        commit (drag, v);
    }

    void applyKnob (const juce::MouseEvent& e)
    {
        // Vertical drag, full travel ~ 180 px, with Ctrl/Cmd fine mode.
        const float gain = (e.mods.isCtrlDown() || e.mods.isCommandDown()) ? 0.25f : 1.0f;
        const int v = juce::jlimit (0, 127, (int) std::lround ((float) dragStart
                        + (-(float) e.getDistanceFromDragStartY() / 180.0f) * 127.0f * gain));
        commit (drag, v);
    }

    void commit (Drag t, int v)
    {
        auto& d = dims[(size_t) selected];
        if (t == Drag::minNode || t == Drag::knobMin)
        {
            if (v != d.mn) { d.mn = v; d.changed = true; repaint(); fire ((juce::uint32) d.minId, v); }
        }
        else if (t == Drag::maxNode || t == Drag::knobMax)
        {
            if (v != d.mx) { d.mx = v; d.changed = true; repaint(); fire ((juce::uint32) d.maxId, v); }
        }
        else
        {
            if (v != d.sens) { d.sens = v; d.changed = true; repaint(); fire (d.id, v); }
        }
    }

    void resetSelected()
    {
        auto& d = dims[(size_t) selected];
        bool any = false;
        if (d.sens != d.initSens) { d.sens = d.initSens; fire (d.id, d.sens); any = true; }
        if (d.hasNodes() && d.mn != d.initMn) { d.mn = d.initMn; fire ((juce::uint32) d.minId, d.mn); any = true; }
        if (d.hasNodes() && d.mx != d.initMx) { d.mx = d.initMx; fire ((juce::uint32) d.maxId, d.mx); any = true; }
        d.changed = false;
        if (any) repaint();
    }

    void fire (juce::uint32 id, int v) { if (change) change (id, (juce::int32) v); }

    ChangeFn change;
    static constexpr int kLiftMs = 700;   // how long a lift pulse stays on the Lift curve
    std::vector<Dim> dims;
    std::vector<LiveInput> liveInputs;   // live finger inputs plotted on the selected curve
    float liftValue = 0.0f;              // last release velocity (for the Lift pulse)
    juce::uint32 liftStartMs = 0;        // when the lift pulse started (0 = none)
    bool liftUnsupported = false;        // device doesn't report lift (fixed note-off)
    int selected = -1;
    Drag drag = Drag::none;
    int  dragStart = 0;

    juce::Rectangle<int> graphR, inspR, chipRow, plotR;
    std::vector<juce::Rectangle<float>> chipR;
    std::vector<Knob> knobR;
    juce::Rectangle<int> knobCardR;   // the recessed card the Min/Sensitivity/Max knobs sit in
    // Fixed velocity (optional), at the bottom of the inspector panel.
    bool hasFixedVel = false;
    juce::uint32 fvToggleId = 0, fvValueId = 0;
    bool fvOn = false; int fvValue = 0;
    juce::Rectangle<float> fvSwitchR, fvValueR, fvLabelR;
    juce::Line<float> fvDivider;
    bool draggingFvValue = false;
    juce::Rectangle<float> resetR;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SensitivityGraph)
};
