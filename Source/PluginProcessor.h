#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

#include "DSP/AutoTuneEngine.h"

#include <array>
#include <atomic>

class LunaticTuneProcessor : public juce::AudioProcessor
{
public:
    LunaticTuneProcessor();
    ~LunaticTuneProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "LUNATIC TUNE"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    // Medidores para la interfaz (notas MIDI fraccionarias, -1 = sin voz)
    float getDetectedMidi() const { return engines[0].meterDetected.load(); }
    float getOutputMidi() const { return engines[0].meterOutput.load(); }
    float getTargetMidi() const { return engines[0].meterTarget.load(); }

    // Notas permitidas actualmente (escala + teclas), para dibujar la rejilla
    std::array<bool, 12> getAllowedNotes() const;

private:
    lunatic::TuneParams readParams() const;
    void configureEngines (int rangeIndex);

    std::array<lunatic::AutoTuneEngine, 2> engines;
    std::array<int, 12> midiCounts {};
    int currentRange = -1;
    double sampleRateHz = 44100.0;

    juce::SmoothedValue<float> mixSmoothed, gainSmoothed;

    std::atomic<float>* pRetune = nullptr;
    std::atomic<float>* pHumanize = nullptr;
    std::atomic<float>* pFlex = nullptr;
    std::atomic<float>* pVibrato = nullptr;
    std::atomic<float>* pFormant = nullptr;
    std::atomic<float>* pTranspose = nullptr;
    std::atomic<float>* pDetune = nullptr;
    std::atomic<float>* pRef = nullptr;
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pOutput = nullptr;
    std::atomic<float>* pKey = nullptr;
    std::atomic<float>* pScale = nullptr;
    std::atomic<float>* pRange = nullptr;
    std::atomic<float>* pMidi = nullptr;
    std::atomic<float>* pBypass = nullptr;
    std::array<std::atomic<float>*, 12> pNotes {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LunaticTuneProcessor)
};
