// SensitivityCurve - the response-curve geometry for one 5D dimension, shared by the overlay
// graph (SensitivityGraph) and the legacy per-dimension panel (CurveComponent).
//
// The device exposes a single *sensitivity* 0-127 plus optional min/max - not multi-point curve
// data. The shapes are drawn the way ROLI Dashboard 4.7.2 draws them (measured from its RISE page):
//   Strike / Press / Lift  quadratic from (0, min) to (s, max), flat after that
//   Slide                  S-curve around x = 0.5, about s wide
//   Glide                  the same on its side: steep at both edges
// Towards 127 every curve straightens to the diagonal. Coordinates are unit (x right, y up).

#pragma once

#include <juce_graphics/juce_graphics.h>

namespace grab
{
    enum class CurveKind { velocity, slide, glide };   // velocity: Strike, Press, Lift

    inline CurveKind curveKindFor (const juce::String& name)
    {
        return name == "Slide" ? CurveKind::slide : name == "Glide" ? CurveKind::glide : CurveKind::velocity;
    }

    /** The response curve in unit coordinates (x right 0..1, y up 0..1). lo/hi are min/max 0..1. */
    inline juce::Path sensitivityCurvePath (const juce::String& name, int sens, float lo, float hi)
    {
        using P = juce::Point<float>;
        const CurveKind kind = curveKindFor (name);
        juce::Path path;
        const float s = (float) sens / 127.0f;
        const float straight = juce::jlimit (0.0f, 1.0f, ((float) sens - 104.0f) / 17.0f);
        const auto lerp = [] (P a, P b, float f) { return a + (b - a) * f; };

        if (kind == CurveKind::velocity)
        {
            if (sens == 0)
            {
                if (name == "Press") return path;
                const float y = name == "Lift" ? lo : hi;
                path.startNewSubPath ({ 0.0f, y });
                path.lineTo ({ 1.0f, y });
                return path;
            }
            const float end = juce::jmax (0.01f, s);
            path.startNewSubPath ({ 0.0f, lo });
            path.quadraticTo ({ end * (0.55f + 0.45f * straight), hi }, { end, hi });
            path.lineTo ({ 1.0f, hi });
            return path;
        }

        if (kind == CurveKind::slide && sens == 0)
            return path;

        const bool slide = kind == CurveKind::slide;
        const float v = (float) juce::jmax (1, sens);
        const float w = slide ? 0.47f * s : juce::jmin (0.5f, 0.02f + v / 160.0f);
        const float a = slide ? 0.89f * w : w * (1.7f - 0.006f * v);

        const P p0 = slide ? P (0.5f - w, 0.0f) : P (0.0f, 0.5f - w);
        const P p3 = slide ? P (0.5f + w, 1.0f) : P (1.0f, 0.5f + w);
        const P p1 = lerp (slide ? P (p0.x + a, 0.0f) : P (0.0f, p0.y + a), lerp (p0, p3, 1.0f / 3.0f), straight);
        const P p2 = lerp (slide ? P (p3.x - a, 1.0f) : P (1.0f, p3.y - a), lerp (p0, p3, 2.0f / 3.0f), straight);

        path.startNewSubPath ({ 0.0f, 0.0f });
        path.lineTo (p0);
        path.cubicTo (p1, p2, p3);
        path.lineTo ({ 1.0f, 1.0f });
        return path;
    }
}
