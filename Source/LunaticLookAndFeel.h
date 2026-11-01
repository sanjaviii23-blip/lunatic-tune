#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace lunatic_ui
{
namespace col
{
    inline const juce::Colour bg     { 0xff0a0a10 };
    inline const juce::Colour panel  { 0xff12121b };
    inline const juce::Colour line   { 0xff262633 };
    inline const juce::Colour text   { 0xffe9e9f2 };
    inline const juce::Colour dim    { 0xff7d7d90 };
    inline const juce::Colour accent { 0xff8f7bff }; // violeta lunar
    inline const juce::Colour cyan   { 0xff5ce1e6 };
}

inline juce::Font uiFont (float size, bool bold = false, float kerning = 0.0f)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain).withKerningFactor (kerning));
}

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, col::bg);
        setColour (juce::PopupMenu::backgroundColourId, col::panel);
        setColour (juce::PopupMenu::textColourId, col::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accent.withAlpha (0.25f));
        setColour (juce::PopupMenu::highlightedTextColourId, col::text);
        setColour (juce::ComboBox::textColourId, col::text);
        setColour (juce::ComboBox::backgroundColourId, col::panel);
        setColour (juce::Label::textColourId, col::text);
        setColour (juce::Slider::textBoxTextColourId, col::text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::TextButton::textColourOffId, col::dim);
        setColour (juce::TextButton::textColourOnId, col::text);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (8.0f);
        const float d = juce::jmin (b.getWidth(), b.getHeight());
        const auto c = b.getCentre();
        const float r = d * 0.5f;
        const juce::PathStrokeType stroke (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

        juce::Path track;
        track.addCentredArc (c.x, c.y, r, r, 0.0f, startAngle, endAngle, true);
        g.setColour (col::line);
        g.strokePath (track, stroke);

        float zero = 0.0f;
        if (s.getMinimum() < 0.0 && s.getMaximum() > 0.0)
            zero = (float) s.valueToProportionOfLength (0.0);
        const float a0 = startAngle + zero * (endAngle - startAngle);
        const float a1 = startAngle + pos * (endAngle - startAngle);

        if (std::abs (a1 - a0) > 0.001f)
        {
            juce::Path val;
            val.addCentredArc (c.x, c.y, r, r, 0.0f, juce::jmin (a0, a1), juce::jmax (a0, a1), true);
            g.setColour (col::accent.withAlpha (0.25f));
            g.strokePath (val, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (col::accent);
            g.strokePath (val, stroke);
        }

        const float pr = r * 0.58f;
        const float px = c.x + pr * std::sin (a1), py = c.y - pr * std::cos (a1);
        g.setColour (col::text);
        g.fillEllipse (px - 2.5f, py - 2.5f, 5.0f, 5.0f);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&) override
    {
        const auto b = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f);
        g.setColour (col::panel);
        g.fillRoundedRectangle (b, 9.0f);
        g.setColour (col::line);
        g.drawRoundedRectangle (b, 9.0f, 1.0f);
        juce::Path arrow;
        const float ax = (float) w - 16.0f, ay = (float) h * 0.5f;
        arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
        g.setColour (col::dim);
        g.fillPath (arrow);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return uiFont (13.0f); }
    juce::Font getPopupMenuFont() override { return uiFont (13.0f); }
    juce::Font getLabelFont (juce::Label&) override { return uiFont (12.0f); }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool) override
    {
        const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        g.setColour (on ? col::accent.withAlpha (0.16f) : col::panel);
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (on ? col::accent : (highlighted ? col::dim : col::line));
        g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
        g.setColour (on ? col::text : col::dim);
        g.setFont (uiFont (11.0f, true, 0.12f));
        g.drawText (b.getButtonText(), r.toNearestInt(), juce::Justification::centred);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                               bool highlighted, bool down) override
    {
        const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (down ? col::accent.withAlpha (0.25f) : col::panel);
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (highlighted ? col::accent : col::line);
        g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int) override { return uiFont (11.0f, true, 0.12f); }
};
} // namespace lunatic_ui
