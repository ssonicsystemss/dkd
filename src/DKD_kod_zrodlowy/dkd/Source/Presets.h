// rutherford factory presets (named like isotopes), picked from the PRESET box in the header.
// Each preset lists only the parameters it changes (real values: index for choices); every other parameter goes back
// to its default. Editor-only on purpose (getNumPrograms() stays 1), so a host program change can never overwrite a
// saved sound. The render test checks every ID exists and every preset is clean.
#pragma once
#include "Params.h"
#include <utility>
#include <vector>

struct FactoryPreset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values;
};

inline const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> presets {
        { "U-150 Classic", {} },   // the factory defaults
        { "Raw-238 Rolling", { { "pattern", 1.0f }, { "tail", 0.22f }, { "bassStart", 0.50f }, { "bassEnd", 0.94f }, { "rise", 8.0f },
                               { "kickDrive", 32.0f }, { "bassDrive", 20.0f }, { "curve", 2.0f }, { "bassTone", 2200.0f }, { "bassLevel", 0.5f } } },
        { "Euphoric-145", { { "tail", 0.35f }, { "bassStart", 0.45f }, { "rise", 7.0f }, { "kickType", 0.0f }, { "kickDrive", 18.0f },
                            { "kickStart", 1500.0f }, { "bassDrive", 8.0f }, { "bassTone", 1200.0f }, { "curve", 3.0f } } },
        { "Zaag-200", { { "pattern", 1.0f }, { "tail", 0.20f }, { "bassStart", 0.45f }, { "bassEnd", 0.92f }, { "kickType", 2.0f },
                        { "kickDrive", 30.0f }, { "kickStart", 2600.0f }, { "kickSweep", 35.0f }, { "bassDrive", 24.0f }, { "bassTone", 3000.0f },
                        { "rise", 5.0f }, { "output", -4.0f } } },
        { "Deep-140", { { "tail", 0.32f }, { "bassStart", 0.42f }, { "rise", 3.0f }, { "curve", 1.8f }, { "kickType", 0.0f },
                        { "kickDrive", 14.0f }, { "bassDrive", 6.0f }, { "bassTone", 800.0f }, { "bassLevel", 0.65f } } },
        { "Neutron-16", { { "pattern", 2.0f }, { "tail", 0.18f }, { "bassStart", 0.80f }, { "bassEnd", 0.97f }, { "rise", 4.0f },
                          { "curve", 1.6f }, { "bassDrive", 18.0f }, { "bassTone", 2500.0f }, { "bassLevel", 0.45f } } },
    };
    return presets;
}

// Message thread only (the editor). Remembers the name in the state so the box shows it again.
inline void applyFactoryPreset (juce::AudioProcessorValueTreeState& s, int index)
{
    const auto& all = factoryPresets();
    const auto& p = all[(size_t) juce::jlimit (0, (int) all.size() - 1, index)];
    for (auto* prm : s.processor.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
        {
            if (r->paramID.startsWith ("own")) continue;   // OWN SOUND belongs to the user, not to the preset
            if (r->paramID == PID::trigger) continue;   // how notes are played, not part of the sound
            float v = r->getDefaultValue();
            for (const auto& [id, real] : p.values)
                if (r->paramID == id) v = r->convertTo0to1 (real);
            r->beginChangeGesture();
            r->setValueNotifyingHost (v);
            r->endChangeGesture();
        }
    s.state.setProperty ("preset", juce::String (p.name), nullptr);
}
