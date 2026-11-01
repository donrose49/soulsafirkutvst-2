#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// Bouton qui, une fois glissé hors de la fenêtre, déclenche un glisser-déposer de fichiers vers le DAW.
class DragButton : public juce::TextButton
{
public:
    std::function<void()> onDragStart;

    void mouseDown (const juce::MouseEvent& e) override { dragged = false; juce::TextButton::mouseDown (e); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! dragged && e.getDistanceFromDragStart() > 8)
        {
            dragged = true;
            if (onDragStart) onDragStart();
        }
        juce::TextButton::mouseDrag (e);
    }
    void mouseUp (const juce::MouseEvent& e) override { dragged = false; juce::TextButton::mouseUp (e); }
private:
    bool dragged = false;
};

class SoulKutEditor : public juce::AudioProcessorEditor,
                      public juce::FileDragAndDropTarget,
                      private juce::Timer
{
public:
    explicit SoulKutEditor (SoulKutProcessor&);
    ~SoulKutEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    void timerCallback() override;
    void refresh();
    void updateShiftLabel();
    void chooseFile();
    void chooseExportFolder();
    static juce::File tempFolder();

    SoulKutProcessor& proc;
    juce::TextButton loadBtn { "Charger un sample" }, exportBtn { "Exporter les slices" },
                     halfBtn { "/2" }, doubleBtn { "x2" }, autoBtn { "Auto" };
    juce::ToggleButton gateBtn { "Mode gate" };
    DragButton dragAllBtn;
    juce::Slider bpmSlider, offsetSlider, transposeSlider;
    juce::ComboBox divBox, targetBox;
    juce::Label fileLabel, keyLabel, bpmTitle, divTitle, offsetTitle, targetTitle, shiftLabel, transposeTitle;
    juce::Rectangle<int> waveArea;
    std::unique_ptr<juce::FileChooser> chooser;
    int lastVersion = -1, lastSlice = -1, draggingSlice = -1;
    bool externalDragStarted = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoulKutEditor)
};
