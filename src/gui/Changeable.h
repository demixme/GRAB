// Changeable — a tiny interface so a SectionCard can tally how many of its child rows have been
// changed from the device value, and show Fred's "N changed" count in the tile header, without the
// card needing to know the concrete row/graph types (which live in MainComponent.h and would make
// the include circular).

#pragma once

namespace grab
{
    struct Changeable
    {
        virtual ~Changeable() = default;
        virtual int changedCount() const = 0;   // how many editable values here differ from the device
    };
}
