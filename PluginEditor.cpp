#include "PluginEditor.h"

static const juce::Colour kBg (0xff14141c), kPanel (0xff1e1e2a), kAccent (0xffff7a2f);
static const char* kNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

SoulKutEditor::SoulKutEditor (SoulKutProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setSize (860, 500);

    auto label = [this] (juce::Label& l, const juce::String& t)
    {
        l.setText (t, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centredLeft);
        l.setColour (juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible (l);
    };

    label (fileLabel, "Glisse un sample ici ou clique sur Charger");
    label (keyLabel, "Gamme : -");
    keyLabel.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    keyLabel.setColour (juce::Label::textColourId, kAccent);
    label (bpmTitle, "BPM");
    label (divTitle, "Decoupe");
    label (offsetTitle, "Decalage");
    label (targetTitle, "Accorder");
    label (shiftLabel, "");
    label (transposeTitle, "Transpo");

    for (auto* b : { &loadBtn, &exportBtn, &halfBtn, &doubleBtn, &autoBtn, (juce::TextButton*) &dragAllBtn })
        addAndMakeVisible (b);
    addAndMakeVisible (gateBtn);
    dragAllBtn.setButtonText ("Glisser toutes les slices vers le DAW");

    auto slider = [this] (juce::Slider& s, double lo, double hi, double step, const juce::String& suffix)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 24);
        s.setRange (lo, hi, step);
        s.setTextValueSuffix (suffix);
        addAndMakeVisible (s);
    };
    slider (bpmSlider, 30.0, 300.0, 0.01, "");
    slider (offsetSlider, 0.0, SoulKutProcessor::maxOffsetMs, 0.1, " ms");
    slider (transposeSlider, -12.0, 12.0, 1.0, " st");

    bpmSlider.onValueChange = [this] { proc.setBpm (bpmSlider.getValue()); repaint(); };
    offsetSlider.onValueChange = [this] { proc.setOffsetMs (offsetSlider.getValue()); repaint(); };
    transposeSlider.onValueChange = [this] { proc.setTranspose ((int) transposeSlider.getValue()); updateShiftLabel(); };

    divBox.addItem ("1/4", 4);   divBox.addItem ("1/8", 8);   divBox.addItem ("1/16", 16);
    divBox.addItem ("1/32", 32); divBox.addItem ("1/64", 64);
    divBox.onChange = [this] { proc.setDivision (divBox.getSelectedId()); repaint(); };
    addAndMakeVisible (divBox);

    targetBox.addItem ("Original", 1);
    for (int i = 0; i < 12; ++i) targetBox.addItem (kNames[i], i + 2);
    targetBox.onChange = [this] { proc.setTargetKey (targetBox.getSelectedId() - 2); updateShiftLabel(); };
    addAndMakeVisible (targetBox);

    halfBtn.onClick   = [this] { bpmSlider.setValue (bpmSlider.getValue() / 2.0); };
    doubleBtn.onClick = [this] { bpmSlider.setValue (bpmSlider.getValue() * 2.0); };
    autoBtn.onClick   = [this] { proc.autoOffset(); };
    gateBtn.onClick   = [this] { proc.setGate (gateBtn.getToggleState()); };
    loadBtn.onClick   = [this] { chooseFile(); };
    exportBtn.onClick = [this] { chooseExportFolder(); };

    dragAllBtn.onDragStart = [this]
    {
        auto folder = tempFolder();
        folder.deleteRecursively();
        folder.createDirectory();
        juce::StringArray files;
        for (int i = 0, n = proc.getNumSlices(); i < n; ++i)
        {
            auto f = proc.getSliceFile (folder, i);
            if (proc.writeSlice (i, f)) files.add (f.getFullPathName());
        }
        if (files.size() > 0)
            juce::DragAndDropContainer::performExternalDragDropOfFiles (files, false, this);
    };

    startTimerHz (10);
    refresh();
}

SoulKutEditor::~SoulKutEditor() { stopTimer(); }

juce::File SoulKutEditor::tempFolder()
{
    return juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("SoulKut_drag");
}

void SoulKutEditor::updateShiftLabel()
{
    const int shift = proc.getKeyShift();
    shiftLabel.setText (proc.getTargetKey() >= 0 ? "auto " + juce::String (shift >= 0 ? "+" : "") + juce::String (shift) + " st" : "",
                        juce::dontSendNotification);
}

void SoulKutEditor::refresh()
{
    bpmSlider.setValue (proc.getBpm(), juce::dontSendNotification);
    offsetSlider.setValue (proc.getOffsetMs(), juce::dontSendNotification);
    divBox.setSelectedId (proc.getDivision(), juce::dontSendNotification);
    targetBox.setSelectedId (proc.getTargetKey() + 2, juce::dontSendNotification);
    transposeSlider.setValue (proc.getTranspose(), juce::dontSendNotification);
    gateBtn.setToggleState (proc.getGate(), juce::dontSendNotification);
    updateShiftLabel();
    const auto key = proc.getKeyName();
    keyLabel.setText (key.isEmpty() ? "Gamme : -" : "Gamme : " + key, juce::dontSendNotification);
    const auto f = proc.getFileName();
    if (f.isNotEmpty()) fileLabel.setText (juce::File (f).getFileName(), juce::dontSendNotification);
    repaint();
}

void SoulKutEditor::timerCallback()
{
    if (lastVersion != proc.getAnalysisVersion()) { lastVersion = proc.getAnalysisVersion(); refresh(); }
}

void SoulKutEditor::chooseFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Choisir un sample", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe = juce::Component::SafePointer<SoulKutEditor> (this)] (const juce::FileChooser& fc)
        {
            if (safe == nullptr || fc.getResult() == juce::File()) return;
            if (! safe->proc.loadSample (fc.getResult()))
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "SoulKut", "Impossible de lire ce fichier.");
        });
}

void SoulKutEditor::chooseExportFolder()
{
    chooser = std::make_unique<juce::FileChooser> ("Dossier d'export", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory));
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [safe = juce::Component::SafePointer<SoulKutEditor> (this)] (const juce::FileChooser& fc)
        {
            if (safe == nullptr || fc.getResult() == juce::File()) return;
            const int n = safe->proc.exportSlices (fc.getResult());
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "SoulKut", juce::String (n) + " slices exportees (WAV 24 bits).");
        });
}

void SoulKutEditor::filesDropped (const juce::StringArray& files, int, int)
{
    if (files.size() > 0) proc.loadSample (juce::File (files[0]));
}

void SoulKutEditor::resized()
{
    auto r = getLocalBounds().reduced (12);
    auto top = r.removeFromTop (32);
    loadBtn.setBounds (top.removeFromLeft (150)); top.removeFromLeft (8);
    exportBtn.setBounds (top.removeFromLeft (150)); top.removeFromLeft (12);
    fileLabel.setBounds (top);

    r.removeFromTop (8);
    auto row = r.removeFromTop (30);
    bpmTitle.setBounds (row.removeFromLeft (40));
    bpmSlider.setBounds (row.removeFromLeft (260)); row.removeFromLeft (6);
    halfBtn.setBounds (row.removeFromLeft (40)); row.removeFromLeft (4);
    doubleBtn.setBounds (row.removeFromLeft (40)); row.removeFromLeft (14);
    divTitle.setBounds (row.removeFromLeft (60));
    divBox.setBounds (row.removeFromLeft (80));

    r.removeFromTop (6);
    auto row2 = r.removeFromTop (30);
    offsetTitle.setBounds (row2.removeFromLeft (70));
    offsetSlider.setBounds (row2.removeFromLeft (300)); row2.removeFromLeft (6);
    autoBtn.setBounds (row2.removeFromLeft (60)); row2.removeFromLeft (14);
    keyLabel.setBounds (row2);

    r.removeFromTop (6);
    auto row3 = r.removeFromTop (30);
    targetTitle.setBounds (row3.removeFromLeft (65));
    targetBox.setBounds (row3.removeFromLeft (90));
    shiftLabel.setBounds (row3.removeFromLeft (85));
    transposeTitle.setBounds (row3.removeFromLeft (60));
    transposeSlider.setBounds (row3.removeFromLeft (190)); row3.removeFromLeft (10);
    gateBtn.setBounds (row3.removeFromLeft (100)); row3.removeFromLeft (6);
    dragAllBtn.setBounds (row3);

    r.removeFromTop (8);
    waveArea = r;
}

void SoulKutEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);
    g.setColour (kPanel);
    g.fillRoundedRectangle (waveArea.toFloat(), 6.0f);

    auto buf = proc.getSample();
    if (buf == nullptr)
    {
        g.setColour (juce::Colours::grey);
        g.drawText ("Depose un sample ici", waveArea, juce::Justification::centred);
        return;
    }

    const int total = buf->getNumSamples();
    const float w = (float) waveArea.getWidth(), h = (float) waveArea.getHeight();
    const float midY = (float) waveArea.getCentreY();
    auto xOf = [&] (double samplePos) { return (float) waveArea.getX() + (float) (samplePos / total) * w; };

    const double len = proc.getSliceLength();
    const int n = proc.getNumSlices();
    if (lastSlice >= 0 && lastSlice < n)
    {
        g.setColour (kAccent.withAlpha (0.25f));
        const float x0 = xOf (proc.getSliceStart (lastSlice)), x1 = xOf (proc.getSliceStart (lastSlice + 1));
        g.fillRect (x0, (float) waveArea.getY(), x1 - x0, h);
    }

    g.setColour (juce::Colours::white.withAlpha (0.8f));
    const auto* d = buf->getReadPointer (0);
    for (int x = 0; x < waveArea.getWidth(); ++x)
    {
        const int a = (int) ((double) x / w * total), b = juce::jmin (total, (int) ((double) (x + 1) / w * total));
        const int step = juce::jmax (1, (b - a) / 32);
        float mn = 0, mx = 0;
        for (int i = a; i < b; i += step) { mn = juce::jmin (mn, d[i]); mx = juce::jmax (mx, d[i]); }
        g.drawVerticalLine (waveArea.getX() + x, midY - mx * h * 0.45f, midY - mn * h * 0.45f);
    }

    if (len > 0.0 && n > 0 && (len / total) * w >= 2.0)
    {
        g.setColour (kAccent);
        for (int i = 0; i <= n; ++i)
            g.drawVerticalLine ((int) xOf (proc.getSliceStart (i)), (float) waveArea.getY(), (float) waveArea.getBottom());
    }

    g.setColour (juce::Colours::grey);
    g.setFont (12.0f);
    g.drawText (juce::String (n) + " slices - clic = pre-ecoute - glisse une slice vers le DAW - MIDI : note "
                + juce::String (SoulKutProcessor::firstNote) + " et suivantes",
                waveArea.reduced (8, 4), juce::Justification::bottomLeft);
}

void SoulKutEditor::mouseDown (const juce::MouseEvent& e)
{
    draggingSlice = -1;
    externalDragStarted = false;
    if (! waveArea.contains (e.getPosition()) || proc.getSampleLength() <= 0) return;
    const double pos = (double) (e.x - waveArea.getX()) / waveArea.getWidth() * proc.getSampleLength();
    const double len = proc.getSliceLength();
    if (len < 1.0) return;
    const int idx = (int) std::floor ((pos - proc.getOffsetSamples()) / len);
    if (idx >= 0 && idx < proc.getNumSlices())
    {
        lastSlice = idx;
        draggingSlice = idx;
        proc.triggerSlice (idx);
        repaint();
    }
}

void SoulKutEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingSlice < 0 || externalDragStarted || e.getDistanceFromDragStart() < 10) return;
    externalDragStarted = true;
    auto folder = tempFolder();
    folder.createDirectory();
    auto f = proc.getSliceFile (folder, draggingSlice);
    if (proc.writeSlice (draggingSlice, f))
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this);
}

void SoulKutEditor::mouseUp (const juce::MouseEvent&)
{
    draggingSlice = -1;
    externalDragStarted = false;
}
