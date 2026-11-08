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
                          public juce::SettableTooltipClient
{
public:
    using ChangeFn = std::function<void (juce::uint32 id, juce::int32 value)>;

    explicit SensitivityGraph (ChangeFn onChangeToUse) : change (std::move (onChangeToUse)) {}

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

    void paint (juce::Graphics& g) override
    {
        if (dims.empty()) return;
        layoutRegions();
        paintGraph (g);
        paintInspector (g);
    }

    void resized() override { layoutRegions(); }

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
        if (drag == Drag::none) return;
        if (drag == Drag::sens || drag == Drag::minNode || drag == Drag::maxNode) applyPlot (e);
        else applyKnob (e);
    }

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
        const int inspW = juce::jlimit (190, 260, b.getWidth() / 4);
        inspR = b.removeFromRight (inspW);
        b.removeFromRight (grab::theme::space::gutter);
        graphR = b;

        // Graph internals: chip row on top, then the plot inset for the axes.
        auto gr = graphR;
        chipRow = gr.removeFromTop (30);
        gr.removeFromTop (6);
        plotR = gr.withTrimmedLeft (54).withTrimmedBottom (34).withTrimmedRight (8).withTrimmedTop (4);

        // Chip rects (measured with the body font).
        chipR.clear();
        juce::Font cf = grab::theme::type::font (grab::theme::type::Role::body);
        int x = chipRow.getX();
        for (auto& d : dims)
        {
            const int w = 22 + juce::GlyphArrangement::getStringWidthInt (cf, d.name) + 16;
            chipR.push_back (juce::Rectangle<float> ((float) x, (float) chipRow.getY() + 2.0f, (float) w, 26.0f));
            x += w + 8;
        }

        // Knob rects in the inspector (Min / Sensitivity / Max, or just Sensitivity).
        knobR.clear();
        if (! dims.empty())
        {
            auto& d = dims[(size_t) selected];
            auto row = inspR.reduced (16, 0).removeFromBottom (78).withTrimmedBottom (6);
            const int kw = 56;
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

        // chips
        juce::Font cf = grab::theme::type::font (grab::theme::type::Role::body);
        for (size_t i = 0; i < chipR.size(); ++i)
        {
            auto& d = dims[i];
            auto r = chipR[i];
            const bool on = (int) i == selected;
            g.setColour (on ? segmentOn : raised);
            g.fillRoundedRectangle (r, 13.0f);
            g.setColour (d.colour);
            g.fillEllipse (r.getX() + 9.0f, r.getCentreY() - 4.0f, 8.0f, 8.0f);
            g.setColour (on ? ink : ink2);
            g.setFont (cf);
            auto tr = r.withTrimmedLeft (22).withTrimmedRight (8);
            g.drawText (on ? d.name + "  " + juce::String (d.sens) : d.name, tr, juce::Justification::centredLeft);
            if (edited (d))   // persistent dot: this dimension differs from its page-load value
            { g.setColour (ink); g.fillEllipse (r.getRight() - 10.0f, r.getY() + 4.0f, 5.0f, 5.0f); }
        }
    }

    void paintInspector (juce::Graphics& g)
    {
        using namespace grab::theme;
        if (dims.empty()) return;
        auto& d = dims[(size_t) selected];
        auto b = inspR.reduced (16, 12);

        // name + Reset
        auto head = b.removeFromTop (22);
        g.setColour (d.colour);
        g.setFont (grab::theme::type::font (grab::theme::type::Role::title));
        g.drawText (d.name, head.removeFromLeft (head.getWidth() - 56), juce::Justification::centredLeft);
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

        // knobs
        for (auto& k : knobR)
        {
            const int v = valueFor (d, k.target);
            const char* lab = k.target == Drag::knobMin ? "Min" : k.target == Drag::knobMax ? "Max" : "Sensitivity";
            paintKnob (g, k.area, d.colour, v, lab);
        }
    }

    void paintKnob (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour c, int v, const char* lab)
    {
        using namespace grab::theme;
        auto face = area.removeFromTop (area.getWidth()).reduced (4.0f);
        const auto centre = face.getCentre();
        const float radius = face.getWidth() * 0.5f;
        const float a0 = juce::MathConstants<float>::pi * 1.25f;
        const float a1 = juce::MathConstants<float>::pi * 2.75f;
        const float ang = a0 + (a1 - a0) * (float) v / 127.0f;

        juce::Path track, val;
        track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, a0, a1, true);
        val.addCentredArc   (centre.x, centre.y, radius, radius, 0.0f, a0, ang, true);
        g.setColour (knobTrack); g.strokePath (track, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (c);         g.strokePath (val,   juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // White index mark at the pointer (Fred's design): a short radial tick straddling the arc.
        const float sn = std::sin (ang), cs = std::cos (ang);
        juce::Line<float> tick (centre.x + sn * (radius - 4.5f), centre.y - cs * (radius - 4.5f),
                                centre.x + sn * (radius + 3.0f), centre.y - cs * (radius + 3.0f));
        g.setColour (ink);
        g.drawLine (tick, 2.0f);

        g.setColour (ink);
        g.setFont (grab::theme::type::font (grab::theme::type::Role::readout));
        g.drawText (juce::String (v), face, juce::Justification::centred);

        g.setColour (label);
        g.setFont (grab::theme::type::font (grab::theme::type::Role::caption));
        g.drawText (lab, area, juce::Justification::centredTop);
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
    std::vector<Dim> dims;
    int selected = -1;
    Drag drag = Drag::none;
    int  dragStart = 0;

    juce::Rectangle<int> graphR, inspR, chipRow, plotR;
    std::vector<juce::Rectangle<float>> chipR;
    std::vector<Knob> knobR;
    juce::Rectangle<float> resetR;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SensitivityGraph)
};
