#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Four selectable visual themes. Minimal is the default — it's the most
// restrained/subtle of the four (thin bevels, muted colours), not a literal
// flat/anti-skeuomorphic style; every theme here still uses the same
// gradient/bevel/glossy-highlight drawing approach, just with different
// palettes and intensity.
enum class UiTheme
{
    Minimal = 0,
    Cute,
    OldSchool,
    Futuristic,
    NumThemes
};

// Custom colour IDs for our own hand-painted components (WaveformSliceView,
// StepGrid, SettingsPanel) so they follow the active theme too, alongside the
// standard JUCE colour IDs (Slider, Button, etc.) that SomeChopsLookAndFeel
// also sets per-theme. Set via LookAndFeel::setColour() in applyPalette(),
// read via Component::findColour() at paint time.
enum SomeChopsColourIds
{
    waveformBackgroundColourId = 0x2000001,
    waveformWaveColourId,
    sliceMarkerStartColourId,
    sliceMarkerTrimColourId,
    stepGridBackgroundColourId,
    stepCellOffColourId,
    stepCellOnColourId,
    stepCellInactiveLaneColourId,
    panelBorderColourId,
    settingsPanelBackgroundColourId,
};

// One theme's full palette. Every theme is drawn with the same routines in
// SomeChopsLookAndFeel — they differ by these values, not by separate paint
// code paths per theme.
struct ThemePalette
{
    juce::Colour panelBackground;
    juce::Colour grooveDark, grooveLight;   // slider track "recessed" gradient
    juce::Colour accent;                     // filled slider portion, toggle-on, highlights
    juce::Colour knobBase;                   // slider thumb / toggle knob base colour
    juce::Colour buttonBase;                 // TextButton / ComboBox base colour
    juce::Colour textColour;
    juce::Colour borderColour;
    float cornerRadius = 5.0f;
    float glossStrength = 0.35f; // 0..1, how strong the glossy highlight/gradient is
};

class SomeChopsLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SomeChopsLookAndFeel();

    void setTheme (UiTheme newTheme);
    UiTheme getTheme() const { return theme; }

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                            float sliderPos, float minSliderPos, float maxSliderPos,
                            const juce::Slider::SliderStyle style, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                                bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                            bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                        int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;

    void fillTextEditorBackground (juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int width, int height, juce::TextEditor&) override;

private:
    UiTheme theme = UiTheme::Minimal;
    ThemePalette palette;

    static ThemePalette paletteFor (UiTheme t);
    void applyPalette();
    void drawGlossyKnob (juce::Graphics&, float centreX, float centreY, float radius, bool enabled);
};
