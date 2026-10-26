// ThrottledWriter: coalesces live config writes. ROLI Dashboard writes every edit to the
// device as you drag; GRAB does the same, but batches rapid changes into at most one burst
// per interval (default 75 ms) so a slider drag doesn't fire a write per pixel. It runs only
// while there's pending work: nudge() starts it on the first edit, and the owner calls
// cancel() from its flush once nothing is left to write.

#pragma once

#include <juce_events/juce_events.h>

#include <functional>

class ThrottledWriter  : private juce::Timer
{
public:
    explicit ThrottledWriter (int intervalMsToUse = 75) : intervalMs (intervalMsToUse) {}
    ~ThrottledWriter() override { stopTimer(); }

    std::function<void()> onTick;      // the owner's flush; decides what (if anything) to write

    void nudge()  { if (! isTimerRunning()) startTimer (intervalMs); }  // arm after an edit
    void cancel() { stopTimer(); }                                      // stop when drained

private:
    void timerCallback() override { if (onTick) onTick(); }

    int intervalMs;
};
