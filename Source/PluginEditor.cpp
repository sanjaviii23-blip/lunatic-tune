#include "PluginEditor.h"

using namespace lunatic_ui;

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& title,
            const juce::String& suffix, int decimals)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 78, 18);
    slider.setTextValueSuffix (suffix);
    slider.setNumDecimalPlacesToDisplay (decimals);
    addAndMakeVisible (slider);

    label.setText (title.toUpperCase(), juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, col::dim);
    label.setFont (uiFont (10.5f, true, 0.12f));
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramId, slider);
    if (auto* p = apvts.getParameter (paramId))
        slider.setDoubleClickReturnValue (true, (double) p->convertFrom0to1 (p->getDefaultValue()));
}

void Knob::resized()
{
    auto b = getLocalBounds();
    label.setBounds (b.removeFromTop (16));
    slider.setBounds (b);
}

//==============================================================================
PitchGraph::PitchGraph (LunaticTuneProcessor& p) : proc (p), det ((size_t) kN, -1.0f), out ((size_t) kN, -1.0f) {}

void PitchGraph::push (float detected, float output)
{
    det[(size_t) head] = detected;
    out[(size_t) head] = output;
    head = (head + 1) % kN;
    if (detected > 0.0f)
        centre += 0.04f * (detected - centre);
    repaint();
}

void PitchGraph::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setColour (col::panel);
    g.fillRoundedRectangle (b, 14.0f);

    const float span = 16.0f;
    const float top = b.getY() + 12.0f, height = b.getHeight() - 24.0f;
    auto yOf = [&] (float m) { return top + height * (1.0f - (m - (centre - span * 0.5f)) / span); };

    g.saveState();
    g.reduceClipRegion (b.reduced (1.0f).toNearestInt());

    // Rejilla: sólo las notas activas
    const auto allowed = proc.getAllowedNotes();
    const int lo = (int) std::floor (centre - span * 0.5f) - 1, hi = (int) std::ceil (centre + span * 0.5f) + 1;
    for (int n = lo; n <= hi; ++n)
    {
        const int pc = ((n % 12) + 12) % 12;
        if (! allowed[(size_t) pc])
            continue;
        const float y = yOf ((float) n);
        g.setColour (pc == 0 ? col::accent.withAlpha (0.28f) : col::line);
        g.drawHorizontalLine ((int) y, b.getX() + 52.0f, b.getRight() - 14.0f);
        g.setColour (pc == 0 ? col::text.withAlpha (0.8f) : col::dim);
        g.setFont (uiFont (10.5f, pc == 0));
        g.drawText (juce::String (lunatic::noteName (pc)) + juce::String (n / 12 - 1),
                    juce::Rectangle<float> (b.getX() + 14.0f, y - 7.0f, 36.0f, 14.0f), juce::Justification::centredLeft);
    }

    // Trazas
    const float x0 = b.getX() + 58.0f, x1 = b.getRight() - 18.0f;
    auto buildPath = [&] (const std::vector<float>& v)
    {
        juce::Path p;
        bool pen = false;
        for (int i = 0; i < kN; ++i)
        {
            const float m = v[(size_t) ((head + i) % kN)];
            if (m < 0.0f) { pen = false; continue; }
            const float x = x0 + (x1 - x0) * (float) i / (float) (kN - 1);
            const float y = yOf (m);
            if (! pen) { p.startNewSubPath (x, y); pen = true; }
            else p.lineTo (x, y);
        }
        return p;
    };
    const auto pd = buildPath (det), po = buildPath (out);
    g.setColour (col::cyan.withAlpha (0.55f));
    g.strokePath (pd, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (col::accent.withAlpha (0.20f));
    g.strokePath (po, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (col::accent);
    g.strokePath (po, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.restoreState();

    // Lectura de la nota actual
    const float tgt = proc.getTargetMidi(), dt = proc.getDetectedMidi();
    juce::String big = "--", small = "";
    if (tgt >= 0.0f)
    {
        const int n = (int) std::lround (tgt);
        big = juce::String (lunatic::noteName (n)) + juce::String (n / 12 - 1);
        if (dt > 0.0f)
        {
            const int cents = (int) std::lround ((dt - tgt) * 100.0f);
            small = (cents > 0 ? "+" : "") + juce::String (cents) + juce::String::fromUTF8 (" ¢");
        }
    }
    else if (dt > 0.0f)
    {
        const int n = (int) std::lround (dt);
        big = juce::String (lunatic::noteName (n)) + juce::String (n / 12 - 1);
    }
    g.setColour (col::text);
    g.setFont (uiFont (30.0f, true, 0.05f));
    g.drawText (big, juce::Rectangle<float> (b.getRight() - 150.0f, b.getY() + 10.0f, 136.0f, 34.0f), juce::Justification::centredRight);
    g.setColour (col::dim);
    g.setFont (uiFont (12.0f));
    g.drawText (small, juce::Rectangle<float> (b.getRight() - 150.0f, b.getY() + 42.0f, 136.0f, 16.0f), juce::Justification::centredRight);
}

//==============================================================================
void NoteStrip::paint (juce::Graphics& g)
{
    const auto allowed = proc.getAllowedNotes();
    const float dt = proc.getDetectedMidi();
    const int live = dt > 0.0f ? ((int) std::lround (dt) % 12 + 12) % 12 : -1;
    const float w = (float) getWidth() / 12.0f;
    for (int i = 0; i < 12; ++i)
    {
        const auto r = juce::Rectangle<float> (w * (float) i, 0.0f, w, (float) getHeight()).reduced (3.0f, 0.0f);
        const bool on = allowed[(size_t) i];
        const bool sharp = juce::String (lunatic::noteName (i)).contains ("#");
        g.setColour (on ? col::accent.withAlpha (sharp ? 0.20f : 0.30f) : col::panel);
        g.fillRoundedRectangle (r, 9.0f);
        g.setColour (on ? col::accent : col::line);
        g.drawRoundedRectangle (r.reduced (0.5f), 9.0f, 1.0f);
        g.setColour (on ? col::text : col::dim);
        g.setFont (uiFont (12.0f, ! sharp));
        g.drawText (lunatic::noteName (i), r.toNearestInt(), juce::Justification::centred);
        if (i == live)
        {
            g.setColour (col::cyan);
            g.fillEllipse (r.getCentreX() - 2.5f, r.getBottom() - 8.0f, 5.0f, 5.0f);
        }
    }
}

void NoteStrip::mouseDown (const juce::MouseEvent& e)
{
    const int i = juce::jlimit (0, 11, (int) (e.position.x / ((float) getWidth() / 12.0f)));
    if (auto* p = proc.apvts.getParameter ("note" + juce::String (i)))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f);
        p->endChangeGesture();
    }
}

//==============================================================================
LunaticTuneEditor::LunaticTuneEditor (LunaticTuneProcessor& p)
    : AudioProcessorEditor (&p), proc (p), graph (p), strip (p)
{
    setLookAndFeel (&laf);
    setSize (800, 520);
    setResizable (false, false);

    auto fillChoices = [&] (juce::ComboBox& box, const juce::String& id)
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id)))
            box.addItemList (choice->choices, 1);
    };
    fillChoices (keyBox, "key");
    fillChoices (scaleBox, "scale");
    fillChoices (rangeBox, "range");
    keyAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "key", keyBox);
    scaleAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "scale", scaleBox);
    rangeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "range", rangeBox);
    midiAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "midi", midiBtn);
    bypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "bypass", bypassBtn);

    for (auto* c : { &keyBox, &scaleBox, &rangeBox })
        addAndMakeVisible (*c);
    addAndMakeVisible (midiBtn);
    addAndMakeVisible (bypassBtn);
    addAndMakeVisible (naturalBtn);
    addAndMakeVisible (hardBtn);
    addAndMakeVisible (graph);
    addAndMakeVisible (strip);

    naturalBtn.onClick = [this]
    {
        setParam ("retune", 60.0f);
        setParam ("humanize", 40.0f);
        setParam ("flex", 12.0f);
        setParam ("vibrato", 100.0f);
    };
    hardBtn.onClick = [this]
    {
        setParam ("retune", 0.0f);
        setParam ("humanize", 0.0f);
        setParam ("flex", 0.0f);
        setParam ("vibrato", 0.0f);
    };

    auto addKnob = [&] (const juce::String& id, const juce::String& title, const juce::String& suffix, int dec)
    {
        knobs.push_back (std::make_unique<Knob> (proc.apvts, id, title, suffix, dec));
        addAndMakeVisible (*knobs.back());
    };
    addKnob ("retune", "Velocidad", " ms", 0);
    addKnob ("humanize", "Humanizar", " %", 0);
    addKnob ("flex", "Flex", juce::String::fromUTF8 (" ¢"), 0);
    addKnob ("vibrato", "Vibrato", " %", 0);
    addKnob ("formant", "Formante", " st", 1);
    addKnob ("transpose", "Transponer", " st", 0);
    addKnob ("detune", "Fino", juce::String::fromUTF8 (" ¢"), 0);
    addKnob ("mix", "Mezcla", " %", 0);
    addKnob ("output", "Salida", " dB", 1);

    startTimerHz (30);
}

LunaticTuneEditor::~LunaticTuneEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void LunaticTuneEditor::setParam (const juce::String& id, float plainValue)
{
    if (auto* p = proc.apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
        p->endChangeGesture();
    }
}

void LunaticTuneEditor::timerCallback()
{
    graph.push (proc.getDetectedMidi(), proc.getOutputMidi());
    strip.repaint();
    if (latencyTick++ % 15 == 0)
    {
        const double sr = proc.getSampleRate() > 0 ? proc.getSampleRate() : 44100.0;
        const auto text = juce::String::fromUTF8 ("LATENCIA  ") + juce::String (1000.0 * proc.getLatencySamples() / sr, 1) + " ms";
        if (text != latencyText)
        {
            latencyText = text;
            repaint (getLocalBounds().removeFromBottom (44));
        }
    }
}

void LunaticTuneEditor::paint (juce::Graphics& g)
{
    using namespace lunatic_ui;
    g.fillAll (col::bg);

    // Luna + marca
    g.setColour (col::accent);
    g.fillEllipse (24.0f, 17.0f, 24.0f, 24.0f);
    g.setColour (col::bg);
    g.fillEllipse (32.0f, 12.0f, 23.0f, 23.0f);
    g.setColour (col::text);
    g.setFont (uiFont (20.0f, true, 0.32f));
    g.drawText ("LUNATIC", juce::Rectangle<int> (64, 14, 140, 28), juce::Justification::centredLeft);
    g.setColour (col::accent);
    g.setFont (uiFont (20.0f, false, 0.32f));
    g.drawText ("TUNE", juce::Rectangle<int> (176, 14, 80, 28), juce::Justification::centredLeft);

    g.setColour (col::dim);
    g.setFont (uiFont (10.5f, true, 0.14f));
    g.drawText (latencyText, juce::Rectangle<int> (getWidth() - 24 - 200, getHeight() - 40, 200, 28), juce::Justification::centredRight);
}

void LunaticTuneEditor::resized()
{
    auto area = getLocalBounds().reduced (24, 0);

    auto header = area.removeFromTop (56);
    header.removeFromLeft (240);
    auto hc = header.withSizeKeepingCentre (header.getWidth(), 30);
    rangeBox.setBounds (hc.removeFromRight (176));
    hc.removeFromRight (8);
    scaleBox.setBounds (hc.removeFromRight (150));
    hc.removeFromRight (8);
    keyBox.setBounds (hc.removeFromRight (64));

    area.removeFromTop (8);
    graph.setBounds (area.removeFromTop (226));
    area.removeFromTop (10);
    strip.setBounds (area.removeFromTop (34));
    area.removeFromTop (12);

    auto knobRow = area.removeFromTop (128);
    const int kw = knobRow.getWidth() / (int) knobs.size();
    for (auto& k : knobs)
        k->setBounds (knobRow.removeFromLeft (kw));

    area.removeFromTop (6);
    auto footer = area.removeFromTop (28);
    naturalBtn.setBounds (footer.removeFromLeft (92));
    footer.removeFromLeft (8);
    hardBtn.setBounds (footer.removeFromLeft (100));
    footer.removeFromLeft (16);
    midiBtn.setBounds (footer.removeFromLeft (64));
    footer.removeFromLeft (8);
    bypassBtn.setBounds (footer.removeFromLeft (80));
}
