#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

#include "Variant.h"

namespace Branding
{
#if DKD_VARIANT == 1
    inline constexpr const char* productName = "dkdalpha";
    inline constexpr const char* displayName = "dkdalpha";
    inline constexpr const char* subtitle    = "kick";
#elif DKD_VARIANT == 2
    inline constexpr const char* productName = "dkdomega";
    inline constexpr const char* displayName = "dkdomega";
    inline constexpr const char* subtitle    = "reverse bass";
#else
    inline constexpr const char* productName = "dkd";
    inline constexpr const char* displayName = "dkd";
    inline constexpr const char* subtitle    = "dualkickdeployment";
#endif
    inline constexpr const char* companyName = "sonicsystems";
}

// Parameter IDs are FROZEN once released: renaming one breaks saved FL projects and automation. Add new ones at the end.
namespace PID
{
    inline constexpr const char* trigger   = "trigger";
    inline constexpr const char* pattern   = "pattern";
    inline constexpr const char* tail      = "tail";
    inline constexpr const char* bassStart = "bassStart";
    inline constexpr const char* bassEnd   = "bassEnd";
    inline constexpr const char* rise      = "rise";
    inline constexpr const char* curve     = "curve";
    inline constexpr const char* kickStart = "kickStart";
    inline constexpr const char* kickSweep = "kickSweep";
    inline constexpr const char* kickDrive = "kickDrive";
    inline constexpr const char* kickType  = "kickType";
    inline constexpr const char* click     = "click";
    inline constexpr const char* kickLevel = "kickLevel";
    inline constexpr const char* bassDrive = "bassDrive";
    inline constexpr const char* bassTone  = "bassTone";
    inline constexpr const char* bassLevel = "bassLevel";
    inline constexpr const char* fine      = "fine";
    inline constexpr const char* output    = "output";
    inline constexpr const char* clipMode  = "clipMode";

    // appended with the modulation / console update: 2 LFOs, 3 macros, a 16 step bass sequencer, the DIP switches.
    // Everything defaults to "off" or "neutral", so an old project sounds exactly as before.
    inline constexpr const char* lfo1Rate  = "lfo1Rate";
    inline constexpr const char* lfo1Shape = "lfo1Shape";
    inline constexpr const char* lfo1Dest  = "lfo1Dest";
    inline constexpr const char* lfo1Depth = "lfo1Depth";
    inline constexpr const char* lfo2Rate  = "lfo2Rate";
    inline constexpr const char* lfo2Shape = "lfo2Shape";
    inline constexpr const char* lfo2Dest  = "lfo2Dest";
    inline constexpr const char* lfo2Depth = "lfo2Depth";
    inline constexpr const char* macro1    = "macro1";
    inline constexpr const char* macro2    = "macro2";
    inline constexpr const char* macro3    = "macro3";
    inline constexpr const char* lfo1On    = "lfo1On";
    inline constexpr const char* lfo2On    = "lfo2On";
    inline constexpr const char* seqOn     = "seqOn";
    inline constexpr const char* macroOn   = "macroOn";
    inline constexpr const char* clickOn   = "clickOn";
    inline constexpr const char* riseOn    = "riseOn";
    inline constexpr const char* dcOn      = "dcOn";
    inline constexpr const char* seqLevel[16] = { "seqL01", "seqL02", "seqL03", "seqL04", "seqL05", "seqL06", "seqL07", "seqL08",
                                                  "seqL09", "seqL10", "seqL11", "seqL12", "seqL13", "seqL14", "seqL15", "seqL16" };
    inline constexpr const char* seqPitch[16] = { "seqP01", "seqP02", "seqP03", "seqP04", "seqP05", "seqP06", "seqP07", "seqP08",
                                                  "seqP09", "seqP10", "seqP11", "seqP12", "seqP13", "seqP14", "seqP15", "seqP16" };
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
