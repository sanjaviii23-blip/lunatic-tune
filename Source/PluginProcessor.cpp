#include "PluginProcessor.h"
#include "PluginEditor.h"

using juce::AudioParameterBool;
using juce::AudioParameterChoice;
using juce::AudioParameterFloat;
using juce::AudioParameterFloatAttributes;
using juce::NormalisableRange;
using juce::ParameterID;

juce::AudioProcessorValueTreeState::ParameterLayout LunaticTuneProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    constexpr int v = 1;

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "retune", v }, "Velocidad",
                                                        NormalisableRange<float> (0.0f, 400.0f, 0.1f, 0.4f), 25.0f,
                                                        AudioParameterFloatAttributes().withLabel ("ms")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "humanize", v }, "Humanizar",
                                                        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f,
                                                        AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "flex", v }, "Flex",
                                                        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f,
                                                        AudioParameterFloatAttributes().withLabel ("cents")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "vibrato", v }, "Vibrato",
                                                        NormalisableRange<float> (0.0f, 200.0f, 0.1f), 100.0f,
                                                        AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "formant", v }, "Formante",
                                                        NormalisableRange<float> (-5.0f, 5.0f, 0.01f), 0.0f,
                                                        AudioParameterFloatAttributes().withLabel ("st")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "transpose", v }, "Transponer",
                                                        NormalisableRange<float> (-7.0f, 7.0f, 1.0f), 0.0f,
                                                        AudioParameterFloatAttributes().withLabel ("st")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "detune", v }, "Fino",
                                                        NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f,
                                                        AudioParameterFloatAttributes().withLabel ("cents")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "ref", v }, "Referencia A4",
                                                        NormalisableRange<float> (415.0f, 466.0f, 0.1f), 440.0f,
                                                        AudioParameterFloatAttributes().withLabel ("Hz")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "mix", v }, "Mezcla",
                                                        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
                                                        AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "output", v }, "Salida",
                                                        NormalisableRange<float> (-18.0f, 12.0f, 0.1f), 0.0f,
                                                        AudioParameterFloatAttributes().withLabel ("dB")));

    juce::StringArray keys;
    for (int i = 0; i < 12; ++i)
        keys.add (lunatic::noteName (i));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "key", v }, "Tonalidad", keys, 0));

    juce::StringArray scaleNames;
    for (const auto& s : lunatic::scaleTable())
        scaleNames.add (juce::String::fromUTF8 (s.name));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "scale", v }, "Escala", scaleNames, 0));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "range", v }, "Rango de voz",
        juce::StringArray { juce::String::fromUTF8 ("Grave  ≥ 65 Hz"), juce::String::fromUTF8 ("Normal  ≥ 90 Hz"),
                            juce::String::fromUTF8 ("Agudo  ≥ 130 Hz"), juce::String::fromUTF8 ("Instrumento  ≥ 220 Hz") },
        1));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "midi", v }, "Notas por MIDI", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "bypass", v }, "Bypass", false));

    for (int i = 0; i < 12; ++i)
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { "note" + juce::String (i), v },
                                                          juce::String ("Nota ") + lunatic::noteName (i), true));
    return layout;
}

LunaticTuneProcessor::LunaticTuneProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "LUNATIC_TUNE_STATE", createLayout())
{
    auto get = [this] (const juce::String& id) { return apvts.getRawParameterValue (id); };
    pRetune = get ("retune");     pHumanize = get ("humanize");  pFlex = get ("flex");
    pVibrato = get ("vibrato");   pFormant = get ("formant");    pTranspose = get ("transpose");
    pDetune = get ("detune");     pRef = get ("ref");            pMix = get ("mix");
    pOutput = get ("output");     pKey = get ("key");            pScale = get ("scale");
    pRange = get ("range");       pMidi = get ("midi");          pBypass = get ("bypass");
    for (int i = 0; i < 12; ++i)
        pNotes[(size_t) i] = get ("note" + juce::String (i));
}

bool LunaticTuneProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    if (in != out)
        return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void LunaticTuneProcessor::configureEngines (int rangeIndex)
{
    currentRange = rangeIndex;
    for (auto& e : engines)
        e.prepare (sampleRateHz, rangeIndex);
    setLatencySamples (engines[0].latencySamples());
}

void LunaticTuneProcessor::prepareToPlay (double sampleRate, int)
{
    sampleRateHz = sampleRate;
    configureEngines ((int) pRange->load());
    mixSmoothed.reset (sampleRate, 0.02);
    gainSmoothed.reset (sampleRate, 0.02);
    mixSmoothed.setCurrentAndTargetValue (pBypass->load() > 0.5f ? 0.0f : pMix->load() * 0.01f);
    gainSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
    midiCounts.fill (0);
}

std::array<bool, 12> LunaticTuneProcessor::getAllowedNotes() const
{
    std::array<bool, 12> a {};
    const int key = (int) pKey->load(), scale = (int) pScale->load();
    for (int i = 0; i < 12; ++i)
        a[(size_t) i] = pNotes[(size_t) i]->load() > 0.5f && lunatic::scaleAllows (scale, key, i);
    return a;
}

lunatic::TuneParams LunaticTuneProcessor::readParams() const
{
    lunatic::TuneParams p;
    p.retuneMs = pRetune->load();
    p.humanize = pHumanize->load() * 0.01f;
    p.flexCents = pFlex->load();
    p.vibrato = pVibrato->load() * 0.01f;
    p.formantSemis = pFormant->load();
    p.transposeSemis = pTranspose->load();
    p.detuneCents = pDetune->load();
    p.refA4 = pRef->load();
    p.key = (int) pKey->load();
    p.scale = (int) pScale->load();
    p.midiMode = pMidi->load() > 0.5f;
    for (int i = 0; i < 12; ++i)
    {
        p.noteEnabled[(size_t) i] = pNotes[(size_t) i]->load() > 0.5f;
        p.midiHeld[(size_t) i] = midiCounts[(size_t) i] > 0;
    }
    return p;
}

void LunaticTuneProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    const int range = (int) pRange->load();
    if (range != currentRange)
        configureEngines (range); // cambio de rango: recalcula latencia (poco frecuente)

    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn())
            ++midiCounts[(size_t) (msg.getNoteNumber() % 12)];
        else if (msg.isNoteOff())
            midiCounts[(size_t) (msg.getNoteNumber() % 12)] = juce::jmax (0, midiCounts[(size_t) (msg.getNoteNumber() % 12)] - 1);
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            midiCounts.fill (0);
    }

    const auto params = readParams();
    for (int ch = 0; ch < numCh; ++ch)
        engines[(size_t) ch].setParams (params);

    mixSmoothed.setTargetValue (pBypass->load() > 0.5f ? 0.0f : pMix->load() * 0.01f);
    gainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));

    const int n = buffer.getNumSamples();
    float* data[2] = { buffer.getWritePointer (0), numCh > 1 ? buffer.getWritePointer (1) : nullptr };
    for (int i = 0; i < n; ++i)
    {
        const float mix = mixSmoothed.getNextValue();
        const float gain = gainSmoothed.getNextValue();
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto& e = engines[(size_t) ch];
            const float wet = e.process (data[ch][i]);
            const float dry = e.dry();
            data[ch][i] = (dry + mix * (wet - dry)) * gain;
        }
    }
}

juce::AudioProcessorEditor* LunaticTuneProcessor::createEditor()
{
    return new LunaticTuneEditor (*this);
}

void LunaticTuneProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void LunaticTuneProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LunaticTuneProcessor();
}
