#include "SomeChopsLookAndFeel.h"

SomeChopsLookAndFeel::SomeChopsLookAndFeel()
{
    palette = paletteFor (theme);
    applyPalette();
}

ThemePalette SomeChopsLookAndFeel::paletteFor (UiTheme t)
{
    ThemePalette p;

    switch (t)
    {
        case UiTheme::Minimal:
            // Brushed-metal restraint: muted blue-grey accent, thin bevels, low gloss.
            p.panelBackground = juce::Colour (0xffe6e6e9);
            p.grooveDark      = juce::Colour (0xffc7c7cc);
            p.grooveLight     = juce::Colour (0xffdcdce0);
            p.accent          = juce::Colour (0xff5a7d9a);
            p.knobBase        = juce::Colour (0xfff3f3f5);
            p.buttonBase      = juce::Colour (0xffd6d6da);
            p.textColour      = juce::Colour (0xff2a2a2e);
            p.borderColour    = juce::Colour (0xffb4b4b9);
            p.cornerRadius    = 4.0f;
            p.glossStrength   = 0.25f;
            break;

        case UiTheme::Cute:
            // Candy pastels, rounded, glossy.
            p.panelBackground = juce::Colour (0xfffdeef7);
            p.grooveDark      = juce::Colour (0xffe9b8d3);
            p.grooveLight     = juce::Colour (0xfff6d9ea);
            p.accent          = juce::Colour (0xffff6fa5);
            p.knobBase        = juce::Colour (0xfffff5fa);
            p.buttonBase      = juce::Colour (0xffffd6e8);
            p.textColour      = juce::Colour (0xff7a2f5e);
            p.borderColour    = juce::Colour (0xffe9a8cd);
            p.cornerRadius    = 11.0f;
            p.glossStrength   = 0.65f;
            break;

        case UiTheme::OldSchool:
            // Beige/brown hardware, amber LED accent, boxy bevels.
            p.panelBackground = juce::Colour (0xffcfc3a5);
            p.grooveDark      = juce::Colour (0xff4a3d2c);
            p.grooveLight     = juce::Colour (0xff6b5a42);
            p.accent          = juce::Colour (0xffd98c2b);
            p.knobBase        = juce::Colour (0xffe8dfc8);
            p.buttonBase      = juce::Colour (0xffbeaf8c);
            p.textColour      = juce::Colour (0xff2e2416);
            p.borderColour    = juce::Colour (0xff3a2f20);
            p.cornerRadius    = 3.0f;
            p.glossStrength   = 0.3f;
            break;

        case UiTheme::Futuristic:
        case UiTheme::NumThemes: // not a real theme — sentinel/count value, never actually selected. in that case we failback to Futuristic.
        default:
            // Dark chrome + neon cyan glow.
            p.panelBackground = juce::Colour (0xff0c0c13);
            p.grooveDark      = juce::Colour (0xff14141d);
            p.grooveLight     = juce::Colour (0xff23232f);
            p.accent          = juce::Colour (0xff00e5ff);
            p.knobBase        = juce::Colour (0xff4a4a58);
            p.buttonBase      = juce::Colour (0xff23232f);
            p.textColour      = juce::Colour (0xffd8f8ff);
            p.borderColour    = juce::Colour (0xff35a7bd);
            p.cornerRadius    = 6.0f;
            p.glossStrength   = 0.5f;
            break;
    }

    return p;
}

void SomeChopsLookAndFeel::setTheme (UiTheme newTheme)
{
    theme = newTheme;
    palette = paletteFor (theme);
    applyPalette();
}

void SomeChopsLookAndFeel::applyPalette()
{
    setColour (juce::ResizableWindow::backgroundColourId, palette.panelBackground);

    setColour (juce::Label::textColourId, palette.textColour);

    setColour (juce::Slider::textBoxTextColourId, palette.textColour);
    setColour (juce::Slider::textBoxOutlineColourId, palette.borderColour);
    setColour (juce::Slider::textBoxBackgroundColourId, palette.panelBackground.darker (0.08f));

    setColour (juce::TextButton::textColourOnId, palette.textColour);
    setColour (juce::TextButton::textColourOffId, palette.textColour);
    setColour (juce::ToggleButton::textColourId, palette.textColour);
    setColour (juce::ComboBox::textColourId, palette.textColour);
    setColour (juce::ComboBox::backgroundColourId, palette.buttonBase);
    setColour (juce::ComboBox::outlineColourId, palette.borderColour);

    setColour (juce::TextEditor::textColourId, palette.textColour);
    setColour (juce::TextEditor::backgroundColourId, palette.panelBackground.darker (0.08f));
    setColour (juce::TextEditor::outlineColourId, palette.borderColour);
    setColour (juce::TextEditor::focusedOutlineColourId, palette.accent);

    setColour (juce::PopupMenu::backgroundColourId, palette.panelBackground);
    setColour (juce::PopupMenu::textColourId, palette.textColour);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, palette.accent);
    setColour (juce::PopupMenu::highlightedTextColourId, palette.knobBase);

    // Custom IDs for our own hand-painted components.
    setColour (waveformBackgroundColourId, palette.panelBackground.darker (0.45f));
    setColour (waveformWaveColourId, palette.accent);
    setColour (sliceMarkerStartColourId, palette.knobBase);
    setColour (sliceMarkerTrimColourId, palette.accent);
    setColour (stepGridBackgroundColourId, palette.panelBackground.darker (0.4f));
    setColour (stepCellOffColourId, palette.grooveDark);
    setColour (stepCellOnColourId, palette.accent);
    setColour (stepCellInactiveLaneColourId, palette.grooveDark.darker (0.2f));
    setColour (panelBorderColourId, palette.borderColour);
    setColour (settingsPanelBackgroundColourId, palette.panelBackground.darker (0.25f));
}

//==============================================================================
void SomeChopsLookAndFeel::drawGlossyKnob (juce::Graphics& g, float cx, float cy, float radius, bool enabled)
{
    juce::Rectangle<float> bounds (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

    g.setColour (juce::Colours::black.withAlpha (0.28f));
    g.fillEllipse (bounds.translated (0.0f, juce::jmax (1.0f, radius * 0.12f)));

    const juce::Colour base = enabled ? palette.knobBase : palette.knobBase.withMultipliedSaturation (0.15f);
    juce::ColourGradient grad (base.brighter (0.25f + palette.glossStrength * 0.35f), bounds.getX(), bounds.getY(),
                                base.darker (0.25f), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill (grad);
    g.fillEllipse (bounds);

    g.setColour (base.darker (0.55f));
    g.drawEllipse (bounds, 1.0f);

    auto highlight = bounds.reduced (radius * 0.3f).withHeight (bounds.getHeight() * 0.38f)
                           .translated (0.0f, -radius * 0.32f);
    g.setColour (juce::Colours::white.withAlpha (0.15f + 0.35f * palette.glossStrength));
    g.fillEllipse (highlight);

    if (theme == UiTheme::Futuristic)
    {
        g.setColour (palette.accent.withAlpha (0.85f));
        g.drawEllipse (bounds.reduced (1.0f), 1.2f);
    }
}

void SomeChopsLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                              float sliderPos, float minSliderPos, float maxSliderPos,
                                              const juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearHorizontal && style != juce::Slider::TwoValueHorizontal)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
        return;
    }

    const bool enabled = slider.isEnabled();
    const float trackH = juce::jmin (10.0f, (float) height * 0.4f);
    juce::Rectangle<float> track ((float) x + 4.0f, (float) y + ((float) height - trackH) * 0.5f,
                                   (float) width - 8.0f, trackH);

    // Recessed groove.
    juce::ColourGradient grooveGrad (palette.grooveDark, track.getX(), track.getY(),
                                      palette.grooveLight, track.getX(), track.getBottom(), false);
    g.setGradientFill (grooveGrad);
    g.fillRoundedRectangle (track, palette.cornerRadius);
    g.setColour (palette.grooveDark.darker (0.4f));
    g.drawRoundedRectangle (track, palette.cornerRadius, 1.0f);

    const bool isTwoValue = (style == juce::Slider::TwoValueHorizontal);
    const float fillMinX = isTwoValue ? minSliderPos : track.getX();
    const float fillMaxX = isTwoValue ? maxSliderPos : sliderPos;

    if (fillMaxX > fillMinX)
    {
        juce::Rectangle<float> fill (fillMinX, track.getY(), fillMaxX - fillMinX, track.getHeight());
        const juce::Colour accentColour = enabled ? palette.accent : palette.accent.withMultipliedSaturation (0.25f);
        juce::ColourGradient fillGrad (accentColour.brighter (palette.glossStrength * 0.5f), fill.getX(), fill.getY(),
                                        accentColour.darker (0.2f), fill.getX(), fill.getBottom(), false);
        g.setGradientFill (fillGrad);
        g.fillRoundedRectangle (fill, palette.cornerRadius);

        if (theme == UiTheme::Futuristic && enabled)
        {
            for (int i = 3; i > 0; --i)
            {
                g.setColour (accentColour.withAlpha (0.07f * (float) i));
                g.drawRoundedRectangle (fill.expanded ((float) i * 1.4f, (float) i * 1.2f),
                                         palette.cornerRadius + (float) i, 1.4f);
            }
        }
    }

    const float knobRadius = juce::jmin ((float) height * 0.5f, 11.0f);
    const float knobY = (float) y + (float) height * 0.5f;

    if (isTwoValue)
    {
        drawGlossyKnob (g, minSliderPos, knobY, knobRadius, enabled);
        drawGlossyKnob (g, maxSliderPos, knobY, knobRadius, enabled);
    }
    else
    {
        drawGlossyKnob (g, sliderPos, knobY, knobRadius, enabled);
    }
}

void SomeChopsLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                                   bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    const bool toggled = button.getToggleState() && button.getClickingTogglesState();

    juce::Colour base = toggled ? palette.accent : palette.buttonBase;
    if (shouldDrawButtonAsHighlighted)
        base = base.brighter (0.08f);
    if (! button.isEnabled())
        base = base.withMultipliedSaturation (0.2f);

    juce::ColourGradient grad = shouldDrawButtonAsDown
        ? juce::ColourGradient (base.darker (0.25f), bounds.getX(), bounds.getY(),
                                 base.brighter (0.05f), bounds.getX(), bounds.getBottom(), false)
        : juce::ColourGradient (base.brighter (0.15f + palette.glossStrength * 0.2f), bounds.getX(), bounds.getY(),
                                 base.darker (0.15f), bounds.getX(), bounds.getBottom(), false);

    g.setGradientFill (grad);
    g.fillRoundedRectangle (bounds, palette.cornerRadius);

    g.setColour (base.darker (0.6f));
    g.drawRoundedRectangle (bounds, palette.cornerRadius, 1.0f);

    if (! shouldDrawButtonAsDown)
    {
        auto sliver = bounds.reduced (2.0f).withHeight (bounds.getHeight() * 0.4f);
        g.setColour (juce::Colours::white.withAlpha (0.05f + 0.2f * palette.glossStrength));
        g.fillRoundedRectangle (sliver, palette.cornerRadius * 0.7f);
    }

    if (theme == UiTheme::Futuristic && toggled)
    {
        g.setColour (palette.accent.withAlpha (0.7f));
        g.drawRoundedRectangle (bounds.expanded (1.5f), palette.cornerRadius + 1.5f, 1.2f);
    }
}

void SomeChopsLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                              bool shouldDrawButtonAsHighlighted, bool /*shouldDrawButtonAsDown*/)
{
    auto bounds = button.getLocalBounds().toFloat();

    const float switchW = 32.0f, switchH = 17.0f;
    juce::Rectangle<float> switchBounds (2.0f, (bounds.getHeight() - switchH) * 0.5f, switchW, switchH);

    const bool on = button.getToggleState();
    juce::Colour trackColour = on ? palette.accent : palette.grooveDark;
    if (shouldDrawButtonAsHighlighted)
        trackColour = trackColour.brighter (0.1f);
    if (! button.isEnabled())
        trackColour = trackColour.withMultipliedSaturation (0.2f);

    juce::ColourGradient trackGrad (trackColour.darker (0.15f), switchBounds.getX(), switchBounds.getY(),
                                     trackColour.darker (0.35f), switchBounds.getX(), switchBounds.getBottom(), false);
    g.setGradientFill (trackGrad);
    g.fillRoundedRectangle (switchBounds, switchH * 0.5f);
    g.setColour (trackColour.darker (0.55f));
    g.drawRoundedRectangle (switchBounds, switchH * 0.5f, 1.0f);

    const float knobD = switchH - 4.0f;
    const float knobX = on ? switchBounds.getRight() - knobD - 2.0f : switchBounds.getX() + 2.0f;
    drawGlossyKnob (g, knobX + knobD * 0.5f, switchBounds.getCentreY(), knobD * 0.5f, button.isEnabled());

    g.setColour (button.isEnabled() ? palette.textColour : palette.textColour.withAlpha (0.5f));
    g.setFont (juce::FontOptions (13.0f));
    const int textX = (int) switchBounds.getRight() + 8;
    g.drawFittedText (button.getButtonText(), textX, 0, juce::jmax (0, (int) bounds.getWidth() - textX - 2),
                       (int) bounds.getHeight(), juce::Justification::centredLeft, 1);
}

void SomeChopsLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool /*isButtonDown*/,
                                          int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox& box)
{
    juce::Rectangle<float> bounds (0.0f, 0.0f, (float) width, (float) height);
    bounds = bounds.reduced (1.0f);

    const juce::Colour base = box.isEnabled() ? palette.buttonBase : palette.buttonBase.withMultipliedSaturation (0.2f);
    juce::ColourGradient grad (base.brighter (0.1f + palette.glossStrength * 0.15f), bounds.getX(), bounds.getY(),
                                base.darker (0.12f), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (bounds, palette.cornerRadius);
    g.setColour (base.darker (0.5f));
    g.drawRoundedRectangle (bounds, palette.cornerRadius, 1.0f);

    juce::Rectangle<float> arrowZone ((float) buttonX, (float) buttonY, (float) buttonW, (float) buttonH);
    juce::Path arrow;
    arrow.addTriangle (arrowZone.getCentreX() - 5.0f, arrowZone.getCentreY() - 3.0f,
                        arrowZone.getCentreX() + 5.0f, arrowZone.getCentreY() - 3.0f,
                        arrowZone.getCentreX(), arrowZone.getCentreY() + 4.0f);
    g.setColour (palette.textColour);
    g.fillPath (arrow);
}

void SomeChopsLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int width, int height, juce::TextEditor& editor)
{
    juce::Rectangle<float> bounds (0.0f, 0.0f, (float) width, (float) height);
    const juce::Colour base = editor.findColour (juce::TextEditor::backgroundColourId);

    // Recessed/inset look, matching the slider groove styling.
    juce::ColourGradient grad (base.darker (0.15f), bounds.getX(), bounds.getY(),
                                base.brighter (0.05f), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (bounds.reduced (1.0f), palette.cornerRadius * 0.6f);
}

void SomeChopsLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int width, int height, juce::TextEditor& editor)
{
    if (editor.isEnabled())
    {
        juce::Rectangle<float> bounds (0.0f, 0.0f, (float) width, (float) height);
        const bool focused = editor.hasKeyboardFocus (true);
        g.setColour (focused ? palette.accent : palette.borderColour);
        g.drawRoundedRectangle (bounds.reduced (1.0f), palette.cornerRadius * 0.6f, focused ? 1.5f : 1.0f);
    }
}
