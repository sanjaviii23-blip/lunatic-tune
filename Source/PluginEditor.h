#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

#include "LunaticLookAndFeel.h"
#include "PluginProcessor.h"

#include <memory>
#include <vector>

namespace lunatic_ui
{
// Rueda con título y valor editable
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& title,
          const juce::String& suffix, int decimals);
    void resized() override;

private:
    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// Gráfico de tono en tiempo real: voz detectada vs. voz corregida sobre la rejilla de notas
class PitchGraph : public juce::Component
{
public:
    explicit PitchGraph (LunaticTuneProcessor& p);
    void push (float detected, float output);
    void paint (juce::Graphics&) override;

private:
    LunaticTuneProcessor& proc;
    std::vector<float> det, out;
    int head = 0;
    float centre = 57.0f;
    static constexpr int kN = 300;
};

// Doce teclas: activar / desactivar cada nota de la escala
class NoteStrip : public juce::Component
{
public:
    explicit NoteStrip (LunaticTuneProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    LunaticTuneProcessor& proc;
};
} // namespace lunatic_ui

class LunaticTuneEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit LunaticTuneEditor (LunaticTuneProcessor&);
    ~LunaticTuneEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setParam (const juce::String& id, float plainValue);

    LunaticTuneProcessor& proc;
    lunatic_ui::LookAndFeel laf;

    juce::ComboBox keyBox, scaleBox, rangeBox;
    juce::ToggleButton midiBtn { "MIDI" }, bypassBtn { "BYPASS" };
    juce::TextButton naturalBtn { "NATURAL" }, hardBtn { "HARD TUNE" };
    lunatic_ui::PitchGraph graph;
    lunatic_ui::NoteStrip strip;
    std::vector<std::unique_ptr<lunatic_ui::Knob>> knobs;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAtt, scaleAtt, rangeAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> midiAtt, bypassAtt;

    juce::String latencyText;
    int latencyTick = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LunaticTuneEditor)
};
