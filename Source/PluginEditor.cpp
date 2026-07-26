#include "PluginEditor.h"

//==============================================================================
// WaveformSliceView
//==============================================================================
WaveformSliceView::WaveformSliceView (SomeChopsAudioProcessor& p) : processor (p)
{
    rebuildWaveformCache();
}

void WaveformSliceView::rebuildWaveformCache()
{
    const auto& buf = processor.getSampler().getSourceBuffer();
    const int numSamples = buf.getNumSamples();
    const int numPoints = 1000;

    minCache.assign ((size_t) numPoints, 0.0f);
    maxCache.assign ((size_t) numPoints, 0.0f);

    if (numSamples <= 0 || buf.getNumChannels() == 0)
        return;

    const int samplesPerPoint = juce::jmax (1, numSamples / numPoints);

    for (int i = 0; i < numPoints; ++i)
    {
        const int start = i * samplesPerPoint;
        const int end = juce::jmin (numSamples, start + samplesPerPoint);
        if (start >= end) continue;

        float mn = 0.0f, mx = 0.0f;
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        {
            auto range = juce::FloatVectorOperations::findMinAndMax (buf.getReadPointer (ch, start), end - start);
            mn = juce::jmin (mn, range.getStart());
            mx = juce::jmax (mx, range.getEnd());
        }
        minCache[(size_t) i] = mn;
        maxCache[(size_t) i] = mx;
    }
}

int WaveformSliceView::sampleToX (int sample) const
{
    const int numSamples = juce::jmax (1, processor.getSampler().getSourceBuffer().getNumSamples());
    return (int) ((double) sample / (double) numSamples * (double) getWidth());
}

int WaveformSliceView::xToSample (int x) const
{
    const int numSamples = processor.getSampler().getSourceBuffer().getNumSamples();
    return (int) ((double) x / (double) juce::jmax (1, getWidth()) * (double) numSamples);
}

void WaveformSliceView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1b1b1f));

    auto& sampler = processor.getSampler();
    const int numSamples = sampler.getSourceBuffer().getNumSamples();

    if (numSamples <= 0)
    {
        g.setColour (juce::Colours::grey);
        g.drawText ("Load a sample to begin", getLocalBounds(), juce::Justification::centred);
        return;
    }

    const int w = getWidth();
    const int h = getHeight();
    const float midY = (float) h * 0.5f;

    g.setColour (juce::Colour (0xff59b3ff));
    for (int x = 0; x < w && x < (int) minCache.size(); ++x)
    {
        const float y1 = midY + minCache[(size_t) x] * midY;
        const float y2 = midY + maxCache[(size_t) x] * midY;
        g.drawVerticalLine (x, juce::jmin (y1, y2), juce::jmax (y1, y2));
    }

    for (int i = 0; i < sampler.getNumSlices(); ++i)
    {
        const auto& sl = sampler.getSlice (i);
        const int xStart = sampleToX (sl.startSample);
        const int xTrim = sampleToX (sl.trimmedEnd);

        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.drawVerticalLine (xStart, 0.0f, (float) h);

        g.setColour (juce::Colours::orange.withAlpha (0.85f));
        g.drawVerticalLine (xTrim, 0.0f, (float) h);

        g.setColour (juce::Colours::white.withAlpha (0.6f));
        g.drawText (juce::String (i + 1), xStart + 2, 2, 24, 14, juce::Justification::left);
    }
}

int WaveformSliceView::hitTestHandle (int x, int /*y*/, DragTarget& targetOut, int& sliceIndexOut) const
{
    auto& sampler = processor.getSampler();
    constexpr int tolerance = 5;

    for (int i = 0; i < sampler.getNumSlices(); ++i)
    {
        const auto& sl = sampler.getSlice (i);
        if (std::abs (sampleToX (sl.startSample) - x) <= tolerance)
        {
            targetOut = DragTarget::sliceStart;
            sliceIndexOut = i;
            return i;
        }
        if (std::abs (sampleToX (sl.trimmedEnd) - x) <= tolerance)
        {
            targetOut = DragTarget::sliceTrim;
            sliceIndexOut = i;
            return i;
        }
    }
    targetOut = DragTarget::none;
    sliceIndexOut = -1;
    return -1;
}

void WaveformSliceView::mouseDown (const juce::MouseEvent& e)
{
    hitTestHandle (e.x, e.y, dragTarget, dragSliceIndex);
}

void WaveformSliceView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragTarget == DragTarget::none || dragSliceIndex < 0)
        return;

    auto& sampler = processor.getSampler();
    const auto& sl = sampler.getSlice (dragSliceIndex);
    const int newSample = juce::jlimit (0, sampler.getSourceBuffer().getNumSamples(), xToSample (e.x));

    if (dragTarget == DragTarget::sliceStart)
        sampler.setSliceBounds (dragSliceIndex, newSample, sl.endSample);
    else if (dragTarget == DragTarget::sliceTrim)
        sampler.setSliceTrimmedLength (dragSliceIndex, newSample);

    repaint();
}

void WaveformSliceView::mouseUp (const juce::MouseEvent&)
{
    dragTarget = DragTarget::none;
    dragSliceIndex = -1;
}

//==============================================================================
// PadGrid
//==============================================================================
PadGrid::PadGrid (SomeChopsAudioProcessor& p) : processor (p)
{
    for (int i = 0; i < kNumPads; ++i)
    {
        auto* b = padButtons.add (new juce::TextButton ("Pad " + juce::String (i + 1)));
        addAndMakeVisible (b);
        b->onClick = [this, i] { processor.triggerPadFromUI (i); };
    }
    startTimerHz (15);
}

void PadGrid::resized()
{
    auto bounds = getLocalBounds();
    const int w = bounds.getWidth() / kNumPads;
    for (int i = 0; i < padButtons.size(); ++i)
        padButtons[i]->setBounds (bounds.getX() + i * w, bounds.getY(), w - 4, bounds.getHeight());
}

void PadGrid::timerCallback()
{
    auto& sampler = processor.getSampler();
    for (int i = 0; i < padButtons.size(); ++i)
    {
        const bool hasSlice = i < sampler.getNumSlices();
        padButtons[i]->setEnabled (hasSlice);
        if (hasSlice)
            padButtons[i]->setButtonText (sampler.getSlice (i).name);
    }
}

//==============================================================================
// SliceRangeRow
//==============================================================================
namespace
{
    juce::String formatSliceRange (int startSample, int endSample, double sourceSampleRate)
    {
        if (sourceSampleRate <= 0.0)
            return "--";

        const double startMs = (double) startSample / sourceSampleRate * 1000.0;
        const double endMs = (double) endSample / sourceSampleRate * 1000.0;

        return juce::String (startMs, 0) + "-" + juce::String (endMs, 0) + "ms";
    }
}

SliceRangeRow::SliceRangeRow (SomeChopsAudioProcessor& p) : processor (p)
{
    for (int i = 0; i < kNumPads; ++i)
    {
        auto* label = rangeLabels.add (new juce::Label());
        addAndMakeVisible (label);
        label->setJustificationType (juce::Justification::centred);
        label->setFont (juce::Font (11.0f));
        label->setColour (juce::Label::textColourId, juce::Colours::lightgrey);
        label->setText ("--", juce::dontSendNotification);

        auto* s = rangeSliders.add (new juce::Slider());
        addAndMakeVisible (s);
        s->setSliderStyle (juce::Slider::TwoValueHorizontal);
        s->setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        s->setRange (0.0, 1.0, 1.0);
        s->setEnabled (false);

        s->onDragStart = [this, i] { userDragging[(size_t) i] = true; };
        s->onDragEnd = [this, i] { userDragging[(size_t) i] = false; };

        s->onValueChange = [this, i, s]
        {
            auto& sampler = processor.getSampler();
            if (i >= sampler.getNumSlices())
                return;

            const int newStart = (int) s->getMinValue();
            const int newEnd = (int) s->getMaxValue();
            const auto sl = sampler.getSlice (i); // copy: bounds below may reallocate the reference

            // Move the slice's start (keeping its outer/detected end boundary the same)...
            sampler.setSliceBounds (i, newStart, sl.endSample);
            // ...then set the adjustable playback length/end within that boundary.
            sampler.setSliceTrimmedLength (i, newEnd);

            updateLabel (i);
            if (onSliceChanged)
                onSliceChanged();
        };
    }
    startTimerHz (15);
}

void SliceRangeRow::updateLabel (int padIndex)
{
    auto& sampler = processor.getSampler();
    if (padIndex >= sampler.getNumSlices())
    {
        rangeLabels[padIndex]->setText ("--", juce::dontSendNotification);
        return;
    }

    const auto& sl = sampler.getSlice (padIndex);
    rangeLabels[padIndex]->setText (formatSliceRange (sl.startSample, sl.trimmedEnd, sampler.getSourceSampleRate()),
                                     juce::dontSendNotification);
}

void SliceRangeRow::resized()
{
    auto bounds = getLocalBounds();
    const int w = bounds.getWidth() / kNumPads;
    const int labelH = 16;

    for (int i = 0; i < rangeSliders.size(); ++i)
    {
        auto column = bounds.withX (bounds.getX() + i * w).withWidth (w - 4);
        rangeLabels[i]->setBounds (column.removeFromTop (labelH));
        rangeSliders[i]->setBounds (column);
    }
}

void SliceRangeRow::timerCallback()
{
    auto& sampler = processor.getSampler();
    const int numSamples = sampler.getSourceBuffer().getNumSamples();

    for (int i = 0; i < rangeSliders.size(); ++i)
    {
        auto* s = rangeSliders[i];
        const bool hasSlice = i < sampler.getNumSlices() && numSamples > 0;
        s->setEnabled (hasSlice);

        if (! hasSlice)
        {
            rangeLabels[i]->setText ("--", juce::dontSendNotification);
            continue;
        }

        if (userDragging[(size_t) i])
            continue; // don't fight the user's drag; onValueChange already keeps things live

        const auto& sl = sampler.getSlice (i);
        if ((int) s->getMaximum() != numSamples)
            s->setRange (0.0, (double) numSamples, 1.0);

        s->setMinAndMaxValues (sl.startSample, sl.trimmedEnd, juce::dontSendNotification);
        updateLabel (i);
    }
}

//==============================================================================
// StepGrid
//==============================================================================
StepGrid::StepGrid (SomeChopsAudioProcessor& p) : processor (p)
{
    startTimerHz (20);
}

void StepGrid::timerCallback() { repaint(); }

void StepGrid::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff141417));

    const int rowH = getHeight() / kNumPads;
    const int colW = getWidth() / kNumSteps;
    const auto& sequencer = processor.getSequencer();
    const auto& pattern = sequencer.getCurrentPattern();

    for (int pad = 0; pad < kNumPads; ++pad)
    {
        const int laneLength = sequencer.getTrackNumSteps (pad);
        const int playingStep = sequencer.getCurrentTrackStep (pad);

        for (int step = 0; step < kNumSteps; ++step)
        {
            juce::Rectangle<int> cell (step * colW, pad * rowH, colW - 2, rowH - 2);
            const auto& stepData = pattern.tracks[(size_t) pad].steps[(size_t) step];
            const bool inActiveLane = step < laneLength;

            juce::Colour c = juce::Colour (0xff2a2a30);
            if (stepData.enabled)
            {
                const float alpha = juce::jmap (stepData.probability, 0.0f, 100.0f, 0.35f, 1.0f);
                c = juce::Colours::limegreen.withAlpha (alpha);
            }

            // Steps beyond this lane's own length won't play this loop — dim them
            // rather than hide them, since the data is kept if the lane is lengthened.
            if (! inActiveLane)
                c = c.withAlpha (c.getFloatAlpha() * 0.25f).withMultipliedBrightness (0.6f);

            if (inActiveLane && step == playingStep)
                c = c.brighter (0.4f);

            g.setColour (c);
            g.fillRect (cell);

            if (stepData.enabled && stepData.ratchet > 1)
            {
                g.setColour (juce::Colours::black.withAlpha (inActiveLane ? 1.0f : 0.4f));
                g.setFont (10.0f);
                g.drawText ("x" + juce::String (stepData.ratchet), cell, juce::Justification::bottomRight);
            }
        }
    }

    g.setColour (juce::Colours::black.withAlpha (0.3f));
    for (int step = 0; step <= kNumSteps; step += 4)
        g.drawVerticalLine (step * colW, 0.0f, (float) getHeight());
}

void StepGrid::resized() {}

void StepGrid::mouseDown (const juce::MouseEvent& e)
{
    const int rowH = getHeight() / kNumPads;
    const int colW = getWidth() / kNumSteps;
    const int pad = juce::jlimit (0, kNumPads - 1, e.y / juce::jmax (1, rowH));
    const int step = juce::jlimit (0, kNumSteps - 1, e.x / juce::jmax (1, colW));

    auto& stepData = processor.getSequencer().getCurrentPattern().tracks[(size_t) pad].steps[(size_t) step];
    stepData.enabled = ! stepData.enabled;

    if (onStepSelected)
        onStepSelected (pad, step);

    repaint();
}

//==============================================================================
// TrackLengthColumn
//==============================================================================
TrackLengthColumn::TrackLengthColumn (SomeChopsAudioProcessor& p) : processor (p)
{
    for (int i = 0; i < kNumPads; ++i)
    {
        auto* label = lengthLabels.add (new juce::Label());
        addAndMakeVisible (label);
        label->setJustificationType (juce::Justification::centred);
        label->setFont (juce::Font (11.0f));
        label->setColour (juce::Label::textColourId, juce::Colours::lightgrey);
        label->setText (juce::String (kNumSteps), juce::dontSendNotification);

        auto* s = lengthSliders.add (new juce::Slider());
        addAndMakeVisible (s);
        s->setSliderStyle (juce::Slider::LinearHorizontal);
        s->setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        s->setRange (1.0, (double) kNumSteps, 1.0);
        s->setValue (kNumSteps, juce::dontSendNotification);

        s->onDragStart = [this, i] { userDragging[(size_t) i] = true; };
        s->onDragEnd = [this, i] { userDragging[(size_t) i] = false; };

        s->onValueChange = [this, i, s]
        {
            processor.getSequencer().setTrackNumSteps (i, (int) s->getValue());
            updateLabel (i);
        };
    }
    startTimerHz (15);
}

void TrackLengthColumn::updateLabel (int padIndex)
{
    lengthLabels[padIndex]->setText (juce::String (processor.getSequencer().getTrackNumSteps (padIndex)),
                                      juce::dontSendNotification);
}

void TrackLengthColumn::resized()
{
    auto bounds = getLocalBounds();
    const int rowH = bounds.getHeight() / kNumPads;
    const int labelW = 30;

    for (int i = 0; i < lengthSliders.size(); ++i)
    {
        auto row = bounds.withY (bounds.getY() + i * rowH).withHeight (rowH - 2);
        lengthLabels[i]->setBounds (row.removeFromLeft (labelW));
        lengthSliders[i]->setBounds (row);
    }
}

void TrackLengthColumn::timerCallback()
{
    auto& sequencer = processor.getSequencer();
    for (int i = 0; i < lengthSliders.size(); ++i)
    {
        if (userDragging[(size_t) i])
            continue;

        auto* s = lengthSliders[i];
        const int numSteps = sequencer.getTrackNumSteps (i);
        if ((int) s->getValue() != numSteps)
            s->setValue (numSteps, juce::dontSendNotification);

        updateLabel (i);
    }
}

//==============================================================================
// SlicePitchRow
//==============================================================================
namespace
{
    juce::String formatPitch (float semitones)
    {
        return (semitones >= 0.0f ? "+" : "") + juce::String (semitones, 1) + "st";
    }
}

SlicePitchRow::SlicePitchRow (SomeChopsAudioProcessor& p) : processor (p)
{
    for (int i = 0; i < kNumPads; ++i)
    {
        auto* label = pitchLabels.add (new juce::Label());
        addAndMakeVisible (label);
        label->setJustificationType (juce::Justification::centred);
        label->setFont (juce::Font (11.0f));
        label->setColour (juce::Label::textColourId, juce::Colours::lightgrey);
        label->setText ("+0.0st", juce::dontSendNotification);

        auto* s = pitchSliders.add (new juce::Slider());
        addAndMakeVisible (s);
        s->setSliderStyle (juce::Slider::LinearHorizontal);
        s->setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        s->setRange (-24.0, 24.0, 0.1);
        s->setValue (0.0, juce::dontSendNotification);
        s->setEnabled (false);

        s->onDragStart = [this, i] { userDragging[(size_t) i] = true; };
        s->onDragEnd = [this, i] { userDragging[(size_t) i] = false; };

        s->onValueChange = [this, i, s]
        {
            auto& sampler = processor.getSampler();
            if (i >= sampler.getNumSlices())
                return;

            sampler.setSlicePitch (i, (float) s->getValue());
            updateLabel (i);
        };
    }
    startTimerHz (15);
}

void SlicePitchRow::updateLabel (int padIndex)
{
    auto& sampler = processor.getSampler();
    if (padIndex >= sampler.getNumSlices())
    {
        pitchLabels[padIndex]->setText ("--", juce::dontSendNotification);
        return;
    }

    pitchLabels[padIndex]->setText (formatPitch (sampler.getSlice (padIndex).basePitch), juce::dontSendNotification);
}

void SlicePitchRow::resized()
{
    auto bounds = getLocalBounds();
    const int w = bounds.getWidth() / kNumPads;
    const int labelH = 16;

    for (int i = 0; i < pitchSliders.size(); ++i)
    {
        auto column = bounds.withX (bounds.getX() + i * w).withWidth (w - 4);
        pitchLabels[i]->setBounds (column.removeFromTop (labelH));
        pitchSliders[i]->setBounds (column);
    }
}

void SlicePitchRow::timerCallback()
{
    auto& sampler = processor.getSampler();
    for (int i = 0; i < pitchSliders.size(); ++i)
    {
        auto* s = pitchSliders[i];
        const bool hasSlice = i < sampler.getNumSlices();
        s->setEnabled (hasSlice);

        if (! hasSlice)
        {
            pitchLabels[i]->setText ("--", juce::dontSendNotification);
            continue;
        }

        if (userDragging[(size_t) i])
            continue;

        const float basePitch = sampler.getSlice (i).basePitch;
        if (! juce::approximatelyEqual (s->getValue(), (double) basePitch))
            s->setValue (basePitch, juce::dontSendNotification);

        updateLabel (i);
    }
}

//==============================================================================
// SettingsPanel
//==============================================================================
namespace
{
    juce::String midiNoteName (int noteNumber)
    {
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const int clamped = juce::jlimit (0, 127, noteNumber);
        const int octave = clamped / 12 - 1;
        const int idx = clamped % 12;
        return juce::String (names[idx]) + juce::String (octave) + " (" + juce::String (clamped) + ")";
    }
}

SettingsPanel::SettingsPanel (SomeChopsAudioProcessor& p) : processor (p)
{
    addAndMakeVisible (title);
    title.setFont (juce::Font (18.0f, juce::Font::bold));
    title.setJustificationType (juce::Justification::centredLeft);

    auto setupNoteSlider = [this] (juce::Slider& s, juce::Label& label, juce::Label* noteName)
    {
        addAndMakeVisible (s);
        addAndMakeVisible (label);
        s.setRange (0.0, 127.0, 1.0);
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        if (noteName != nullptr)
        {
            addAndMakeVisible (noteName);
            noteName->setJustificationType (juce::Justification::centredLeft);
        }
    };

    setupNoteSlider (padBaseSlider, padBaseLabel, &padBaseNoteName);
    setupNoteSlider (patternBaseSlider, patternBaseLabel, &patternBaseNoteName);
    setupNoteSlider (startNoteSlider, startNoteLabel, &startNoteName);
    setupNoteSlider (stopNoteSlider, stopNoteLabel, &stopNoteName);

    addAndMakeVisible (patternCountLabel);
    addAndMakeVisible (patternCountSlider);
    patternCountSlider.setRange (1.0, (double) kNumPatterns, 1.0);
    patternCountSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    patternCountSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 45, 20);

    addAndMakeVisible (closeButton);
    closeButton.onClick = [this] { setVisible (false); };

    padBaseSlider.onValueChange = [this]
    {
        processor.midiSettings.padBaseNote = (int) padBaseSlider.getValue();
        padBaseNoteName.setText (midiNoteName (processor.midiSettings.padBaseNote), juce::dontSendNotification);
    };
    patternBaseSlider.onValueChange = [this]
    {
        processor.midiSettings.patternBaseNote = (int) patternBaseSlider.getValue();
        patternBaseNoteName.setText (midiNoteName (processor.midiSettings.patternBaseNote), juce::dontSendNotification);
    };
    patternCountSlider.onValueChange = [this]
    {
        processor.midiSettings.patternNoteCount = (int) patternCountSlider.getValue();
    };
    startNoteSlider.onValueChange = [this]
    {
        processor.midiSettings.startNote = (int) startNoteSlider.getValue();
        startNoteName.setText (midiNoteName (processor.midiSettings.startNote), juce::dontSendNotification);
    };
    stopNoteSlider.onValueChange = [this]
    {
        processor.midiSettings.stopNote = (int) stopNoteSlider.getValue();
        stopNoteName.setText (midiNoteName (processor.midiSettings.stopNote), juce::dontSendNotification);
    };

    refresh();
}

void SettingsPanel::refresh()
{
    const auto& m = processor.midiSettings;
    padBaseSlider.setValue (m.padBaseNote, juce::dontSendNotification);
    padBaseNoteName.setText (midiNoteName (m.padBaseNote), juce::dontSendNotification);
    patternBaseSlider.setValue (m.patternBaseNote, juce::dontSendNotification);
    patternBaseNoteName.setText (midiNoteName (m.patternBaseNote), juce::dontSendNotification);
    patternCountSlider.setValue (m.patternNoteCount, juce::dontSendNotification);
    startNoteSlider.setValue (m.startNote, juce::dontSendNotification);
    startNoteName.setText (midiNoteName (m.startNote), juce::dontSendNotification);
    stopNoteSlider.setValue (m.stopNote, juce::dontSendNotification);
    stopNoteName.setText (midiNoteName (m.stopNote), juce::dontSendNotification);
}

void SettingsPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xf0101014));
    g.setColour (juce::Colours::grey.withAlpha (0.4f));
    g.drawRect (getLocalBounds().reduced (20), 1);
}

void SettingsPanel::layoutRow (juce::Rectangle<int> row, juce::Label& label, juce::Slider& slider, juce::Label* noteName)
{
    label.setBounds (row.removeFromLeft (170));
    if (noteName != nullptr)
        noteName->setBounds (row.removeFromRight (110));
    slider.setBounds (row);
}

void SettingsPanel::resized()
{
    auto bounds = getLocalBounds().reduced (40);
    title.setBounds (bounds.removeFromTop (30));
    bounds.removeFromTop (16);

    const int rowH = 32;
    layoutRow (bounds.removeFromTop (rowH), padBaseLabel, padBaseSlider, &padBaseNoteName);
    bounds.removeFromTop (10);
    layoutRow (bounds.removeFromTop (rowH), patternBaseLabel, patternBaseSlider, &patternBaseNoteName);
    bounds.removeFromTop (10);
    patternCountLabel.setBounds (bounds.removeFromTop (rowH).withWidth (170));
    patternCountSlider.setBounds (patternCountLabel.getBounds().withX (patternCountLabel.getRight() + 10)
                                       .withWidth (bounds.getWidth() - 180));
    bounds.removeFromTop (10);
    layoutRow (bounds.removeFromTop (rowH), startNoteLabel, startNoteSlider, &startNoteName);
    bounds.removeFromTop (10);
    layoutRow (bounds.removeFromTop (rowH), stopNoteLabel, stopNoteSlider, &stopNoteName);

    bounds.removeFromTop (20);
    closeButton.setBounds (bounds.removeFromTop (30).removeFromLeft (100));
}

//==============================================================================
// SomeChopsAudioProcessorEditor
//==============================================================================
SomeChopsAudioProcessorEditor::SomeChopsAudioProcessorEditor (SomeChopsAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p),
      waveformView (p), slicePitchRow (p), sliceRangeRow (p), padGrid (p), stepGrid (p),
      trackLengthColumn (p),
      settingsPanel (p)
{
    setSize (1200, 862);

    addAndMakeVisible (loadButton);
    addAndMakeVisible (autoSliceButton);
    addAndMakeVisible (sensitivitySlider);
    addAndMakeVisible (sensitivityLabel);
    addAndMakeVisible (savePresetButton);
    addAndMakeVisible (loadPresetButton);
    addAndMakeVisible (playStopButton);
    addAndMakeVisible (chokeModeToggle);
    addAndMakeVisible (bpmSlider);
    addAndMakeVisible (bpmLabel);
    addAndMakeVisible (settingsButton);
    addChildComponent (settingsPanel); // hidden until toggled
    addAndMakeVisible (waveformView);
    addAndMakeVisible (slicePitchRow);
    addAndMakeVisible (sliceRangeRow);
    addAndMakeVisible (padGrid);
    addAndMakeVisible (patternSelector);
    addAndMakeVisible (randomizeButton);
    addAndMakeVisible (clearButton);
    addAndMakeVisible (quantizePatternChangeToggle);
    addAndMakeVisible (densitySlider);
    addAndMakeVisible (densityLabel);
    addAndMakeVisible (pitchRangeSlider);
    addAndMakeVisible (pitchRangeLabel);
    addAndMakeVisible (maxRatchetSlider);
    addAndMakeVisible (maxRatchetLabel);
    addAndMakeVisible (stepGrid);
    addAndMakeVisible (trackLengthColumn);
    addAndMakeVisible (selectedStepLabel);
    addAndMakeVisible (stepRatchetSlider);
    addAndMakeVisible (stepRatchetLabel);
    addAndMakeVisible (stepPitchSlider);
    addAndMakeVisible (stepPitchLabel);
    addAndMakeVisible (stepProbabilitySlider);
    addAndMakeVisible (stepProbLabel);
    addAndMakeVisible (stepNudgeSlider);
    addAndMakeVisible (stepNudgeLabel);

    sensitivitySlider.setRange (0.5, 4.0, 0.05);
    sensitivitySlider.setValue (1.5);
    sensitivitySlider.setSliderStyle (juce::Slider::LinearHorizontal);

    bpmSlider.setRange (40.0, 300.0, 1.0);
    bpmSlider.setValue (processor.manualBpm, juce::dontSendNotification);
    bpmSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    bpmSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 55, 20);

    densitySlider.setRange (0.0, 1.0, 0.01);
    densitySlider.setValue (0.5);
    densitySlider.setSliderStyle (juce::Slider::LinearHorizontal);

    pitchRangeSlider.setRange (0.0, 24.0, 1.0);
    pitchRangeSlider.setValue (12.0);
    pitchRangeSlider.setSliderStyle (juce::Slider::LinearHorizontal);

    maxRatchetSlider.setRange (1.0, 8.0, 1.0);
    maxRatchetSlider.setValue (4.0);
    maxRatchetSlider.setSliderStyle (juce::Slider::LinearHorizontal);

    // Ratchet/probability made longer with visible text boxes so exact values are easy to read.
    stepRatchetSlider.setRange (1.0, 8.0, 1.0);
    stepRatchetSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    stepRatchetSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 45, 20);
    stepPitchSlider.setRange (-24.0, 24.0, 0.1);
    stepPitchSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    stepPitchSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 20);
    stepProbabilitySlider.setRange (0.0, 100.0, 1.0);
    stepProbabilitySlider.setSliderStyle (juce::Slider::LinearHorizontal);
    stepProbabilitySlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 45, 20);
    stepNudgeSlider.setRange (-50.0, 50.0, 0.5);
    stepNudgeSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    stepNudgeSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 20);

    refreshPatternSelector();

    bpmSlider.onValueChange = [this] { processor.manualBpm = bpmSlider.getValue(); };

    settingsButton.onClick = [this]
    {
        settingsPanel.refresh();
        settingsPanel.setVisible (true);
        settingsPanel.toFront (true);
    };

    // --- callbacks ---
    loadButton.onClick = [this]
    {
        fileChooser = std::make_unique<juce::FileChooser> ("Load sample", juce::File(), "*.wav;*.aif;*.aiff;*.mp3;*.flac");
        fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file.existsAsFile())
                {
                    processor.loadSampleFromFile (file);
                    waveformView.rebuildWaveformCache();
                    waveformView.repaint();
                }
            });
    };

    autoSliceButton.onClick = [this]
    {
        processor.getSampler().autoSlice ((float) sensitivitySlider.getValue());
        waveformView.repaint();
    };

    savePresetButton.onClick = [this]
    {
        fileChooser = std::make_unique<juce::FileChooser> ("Save preset", juce::File(), "*.dchp");
        fileChooser->launchAsync (juce::FileBrowserComponent::saveMode,
            [this] (const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file != juce::File())
                {
                    if (! file.hasFileExtension (".dchp"))
                        file = file.withFileExtension (".dchp");

                    processor.getPresetManager().savePreset (file, processor.getSampler(), processor.getSequencer(),
                        processor.getSequencer().getCurrentPatternIndex(), processor.currentBpmForSave, processor.midiSettings);
                }
            });
    };

    loadPresetButton.onClick = [this]
    {
        fileChooser = std::make_unique<juce::FileChooser> ("Load preset", juce::File(), "*.dchp");
        fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file.existsAsFile())
                {
                    int patternIndex = 0;
                    double bpm = 120.0;
                    if (processor.getPresetManager().loadPreset (file, processor.getSampler(), processor.getSequencer(), patternIndex, bpm, processor.midiSettings))
                    {
                        processor.getSequencer().setCurrentPatternIndex (patternIndex);
                        refreshPatternSelector();
                        chokeModeToggle.setToggleState (processor.getSampler().getChokeMode(), juce::dontSendNotification);
                        quantizePatternChangeToggle.setToggleState (processor.midiSettings.quantizePatternChanges, juce::dontSendNotification);
                        processor.manualBpm = bpm;
                        bpmSlider.setValue (bpm, juce::dontSendNotification);
                        settingsPanel.refresh();
                        waveformView.rebuildWaveformCache();
                        waveformView.repaint();
                    }
                }
            });
    };

    chokeModeToggle.setToggleState (processor.getSampler().getChokeMode(), juce::dontSendNotification);
    chokeModeToggle.onClick = [this]
    {
        processor.getSampler().setChokeMode (chokeModeToggle.getToggleState());
    };

    // Shared with MIDI-triggered pattern switches (processBlock reads processor.midiSettings
    // directly), so this toggle just mirrors that state rather than owning its own.
    quantizePatternChangeToggle.setToggleState (processor.midiSettings.quantizePatternChanges, juce::dontSendNotification);
    quantizePatternChangeToggle.onClick = [this]
    {
        processor.midiSettings.quantizePatternChanges = quantizePatternChangeToggle.getToggleState();
    };

    sliceRangeRow.onSliceChanged = [this] { waveformView.repaint(); };

    playStopButton.onClick = [this]
    {
        processor.manualPlayActive = ! processor.manualPlayActive;
        if (processor.manualPlayActive)
            processor.getSequencer().resetPosition();
        playStopButton.setButtonText (processor.manualPlayActive ? "Stop" : "Play");
    };

    patternSelector.onChange = [this]
    {
        const int newIndex = patternSelector.getSelectedId() - 1;
        const bool immediate = ! quantizePatternChangeToggle.getToggleState();
        processor.getSequencer().requestPatternChange (newIndex, immediate);
        stepGrid.repaint();
    };

    randomizeButton.onClick = [this]
    {
        processor.getSequencer().randomizeAllTracks ((float) densitySlider.getValue(),
            (float) pitchRangeSlider.getValue(), (int) maxRatchetSlider.getValue());
        stepGrid.repaint();
    };

    clearButton.onClick = [this]
    {
        processor.getSequencer().clearPattern (processor.getSequencer().getCurrentPatternIndex());
        stepGrid.repaint();
    };

    stepGrid.onStepSelected = [this] (int pad, int step)
    {
        selectedPad = pad;
        selectedStep = step;
        updateSelectedStepControls();
    };

    stepRatchetSlider.onValueChange = [this]
    {
        if (selectedPad >= 0)
            processor.getSequencer().getCurrentPattern().tracks[(size_t) selectedPad].steps[(size_t) selectedStep].ratchet
                = (int) stepRatchetSlider.getValue();
    };
    stepPitchSlider.onValueChange = [this]
    {
        if (selectedPad >= 0)
            processor.getSequencer().getCurrentPattern().tracks[(size_t) selectedPad].steps[(size_t) selectedStep].pitchSemitones
                = (float) stepPitchSlider.getValue();
    };
    stepProbabilitySlider.onValueChange = [this]
    {
        if (selectedPad >= 0)
            processor.getSequencer().getCurrentPattern().tracks[(size_t) selectedPad].steps[(size_t) selectedStep].probability
                = (float) stepProbabilitySlider.getValue();
    };
    stepNudgeSlider.onValueChange = [this]
    {
        if (selectedPad >= 0)
            processor.getSequencer().getCurrentPattern().tracks[(size_t) selectedPad].steps[(size_t) selectedStep].nudge
                = (float) stepNudgeSlider.getValue();
    };

    updateSelectedStepControls();
}

void SomeChopsAudioProcessorEditor::refreshPatternSelector()
{
    patternSelector.clear (juce::dontSendNotification);
    for (int i = 0; i < kNumPatterns; ++i)
        patternSelector.addItem ("Pattern " + juce::String (i + 1), i + 1);
    patternSelector.setSelectedId (processor.getSequencer().getCurrentPatternIndex() + 1, juce::dontSendNotification);
}

void SomeChopsAudioProcessorEditor::updateSelectedStepControls()
{
    const bool hasSelection = selectedPad >= 0 && selectedStep >= 0;
    stepRatchetSlider.setEnabled (hasSelection);
    stepPitchSlider.setEnabled (hasSelection);
    stepProbabilitySlider.setEnabled (hasSelection);
    stepNudgeSlider.setEnabled (hasSelection);

    if (! hasSelection)
    {
        selectedStepLabel.setText ("No step selected", juce::dontSendNotification);
        return;
    }

    auto& stepData = processor.getSequencer().getCurrentPattern().tracks[(size_t) selectedPad].steps[(size_t) selectedStep];
    selectedStepLabel.setText ("Pad " + juce::String (selectedPad + 1) + " / Step " + juce::String (selectedStep + 1),
                                juce::dontSendNotification);
    stepRatchetSlider.setValue (stepData.ratchet, juce::dontSendNotification);
    stepPitchSlider.setValue (stepData.pitchSemitones, juce::dontSendNotification);
    stepProbabilitySlider.setValue (stepData.probability, juce::dontSendNotification);
    stepNudgeSlider.setValue (stepData.nudge, juce::dontSendNotification);
}

void SomeChopsAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0e0e10));
}

void SomeChopsAudioProcessorEditor::resized()
{
    auto full = getLocalBounds();
    settingsPanel.setBounds (full); // full overlay regardless of visibility

    auto r = full.reduced (8);

    auto topBar = r.removeFromTop (28);

    // Pin BPM to the top-right first, so it stays there regardless of how much
    // room the left-aligned controls need.
    auto bpmArea = topBar.removeFromRight (170);
    bpmLabel.setBounds (bpmArea.removeFromLeft (40));
    bpmSlider.setBounds (bpmArea);
    topBar.removeFromRight (10);
    settingsButton.setBounds (topBar.removeFromRight (90));
    topBar.removeFromRight (10);

    loadButton.setBounds (topBar.removeFromLeft (100));
    topBar.removeFromLeft (4);
    autoSliceButton.setBounds (topBar.removeFromLeft (90));
    topBar.removeFromLeft (4);
    sensitivityLabel.setBounds (topBar.removeFromLeft (70));
    sensitivitySlider.setBounds (topBar.removeFromLeft (140));
    topBar.removeFromLeft (10);
    savePresetButton.setBounds (topBar.removeFromLeft (100));
    topBar.removeFromLeft (4);
    loadPresetButton.setBounds (topBar.removeFromLeft (100));
    topBar.removeFromLeft (10);
    playStopButton.setBounds (topBar.removeFromLeft (90));
    topBar.removeFromLeft (10);
    chokeModeToggle.setBounds (topBar.removeFromLeft (130));

    r.removeFromTop (6);
    waveformView.setBounds (r.removeFromTop (160));

    r.removeFromTop (4);
    slicePitchRow.setBounds (r.removeFromTop (54));

    r.removeFromTop (4);
    sliceRangeRow.setBounds (r.removeFromTop (54));

    r.removeFromTop (4);
    padGrid.setBounds (r.removeFromTop (50));

    // Pattern selection/switching controls on their own row...
    r.removeFromTop (8);
    auto patternBar = r.removeFromTop (28);
    patternSelector.setBounds (patternBar.removeFromLeft (120));
    patternBar.removeFromLeft (10);
    quantizePatternChangeToggle.setBounds (patternBar.removeFromLeft (150));

    // ...and randomization gets its own row below, so its sliders can be much longer.
    r.removeFromTop (6);
    auto randomizeBar = r.removeFromTop (28);
    randomizeButton.setBounds (randomizeBar.removeFromLeft (110));
    randomizeBar.removeFromLeft (4);
    clearButton.setBounds (randomizeBar.removeFromLeft (100));
    randomizeBar.removeFromLeft (14);
    densityLabel.setBounds (randomizeBar.removeFromLeft (55));
    densitySlider.setBounds (randomizeBar.removeFromLeft (220));
    randomizeBar.removeFromLeft (14);
    pitchRangeLabel.setBounds (randomizeBar.removeFromLeft (85));
    pitchRangeSlider.setBounds (randomizeBar.removeFromLeft (220));
    randomizeBar.removeFromLeft (14);
    maxRatchetLabel.setBounds (randomizeBar.removeFromLeft (85));
    maxRatchetSlider.setBounds (randomizeBar.removeFromLeft (220));

    r.removeFromTop (6);
    auto stepEditorRow2 = r.removeFromBottom (28); // Probability + Nudge
    r.removeFromBottom (4);
    auto stepEditorRow1 = r.removeFromBottom (28); // label + Ratchet + Pitch

    selectedStepLabel.setBounds (stepEditorRow1.removeFromLeft (150));
    stepEditorRow1.removeFromLeft (6);
    // Ratchet/Pitch/Probability/Nudge sliders all kept the same width, per request.
    stepRatchetLabel.setBounds (stepEditorRow1.removeFromLeft (55));
    stepRatchetSlider.setBounds (stepEditorRow1.removeFromLeft (260));
    stepEditorRow1.removeFromLeft (12);
    stepPitchLabel.setBounds (stepEditorRow1.removeFromLeft (45));
    stepPitchSlider.setBounds (stepEditorRow1.removeFromLeft (260));

    stepEditorRow2.removeFromLeft (156); // align under row 1's sliders, past where the label sat
    stepProbLabel.setBounds (stepEditorRow2.removeFromLeft (75));
    stepProbabilitySlider.setBounds (stepEditorRow2.removeFromLeft (260));
    stepEditorRow2.removeFromLeft (12);
    stepNudgeLabel.setBounds (stepEditorRow2.removeFromLeft (55));
    stepNudgeSlider.setBounds (stepEditorRow2.removeFromLeft (260));

    r.removeFromBottom (6);
    trackLengthColumn.setBounds (r.removeFromLeft (110));
    r.removeFromLeft (4);
    stepGrid.setBounds (r);
}
