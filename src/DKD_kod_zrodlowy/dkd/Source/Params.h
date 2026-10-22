#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace Branding
{
    inline constexpr const char* productName = "dkd";
    inline constexpr const char* companyName = "sonicsystems";
    inline constexpr const char* displayName = "dkd";
    inline constexpr const char* subtitle    = "dualkickdeployment";
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
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
