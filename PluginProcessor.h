#pragma once
#include <JuceHeader.h>
#include <atomic>

class SoulKutProcessor : public juce::AudioProcessor
{
public:
    static constexpr int firstNote = 36; // la slice 1 = note MIDI 36
    static constexpr double maxOffsetMs = 4000.0;

    SoulKutProcessor();
    ~SoulKutProcessor() override = default;

    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "SoulKut"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // --- API utilisée par l'interface ---
    bool loadSample (const juce::File&);
    std::shared_ptr<juce::AudioBuffer<float>> getSample() const;
    double getSampleRate() const { return sampleRate.load(); }
    int getSampleLength() const { return sampleLength.load(); }
    juce::String getFileName() const;

    void setBpm (double b) { bpm = b; }
    double getBpm() const { return bpm.load(); }
    juce::String getKeyName() const;

    void setDivision (int d) { division = d; }   // 4, 8, 16, 32, 64
    int getDivision() const { return division.load(); }
    void setOffsetMs (double ms) { offsetMs = ms; }
    double getOffsetMs() const { return offsetMs.load(); }
    void autoOffset();                           // aligne la grille sur la 1re attaque

    // Mode gate : la slice s'arrête au relâchement de la note
    void setGate (bool g) { gate = g; }
    bool getGate() const { return gate.load(); }

    // Accord : -1 = original, 0..11 = tonique cible (C..B) ; + transposition manuelle en demi-tons
    void setTargetKey (int k) { targetKey = k; }
    int getTargetKey() const { return targetKey.load(); }
    void setTranspose (int s) { transpose = s; }
    int getTranspose() const { return transpose.load(); }
    int getKeyShift() const;
    double getTotalSemitones() const { return (double) (getKeyShift() + transpose.load()); }

    double getSliceLength() const;               // en échantillons (double, pas de dérive)
    double getOffsetSamples() const;
    int getNumSlices() const;
    int getSliceStart (int i) const;

    void triggerSlice (int i) { pendingTrigger = i; }
    juce::File getSliceFile (const juce::File& folder, int i) const;
    bool writeSlice (int i, const juce::File& file) const;
    int exportSlices (const juce::File& folder);
    int getAnalysisVersion() const { return analysisVersion.load(); }
    void bumpVersion() { ++analysisVersion; }

private:
    struct Voice
    {
        bool active = false, released = false;
        int slice = -1, note = -1, relLeft = 0, relTotal = 1;
        double start = 0, pos = 0, end = 0, rate = 1;
    };

    juce::AudioFormatManager formats;
    mutable juce::SpinLock sampleLock;
    std::shared_ptr<juce::AudioBuffer<float>> sample;
    mutable juce::CriticalSection infoLock;
    juce::String keyName, fileName;

    std::atomic<double> sampleRate { 44100.0 }, bpm { 120.0 }, offsetMs { 0.0 };
    std::atomic<int> sampleLength { 0 }, division { 16 }, pendingTrigger { -1 }, analysisVersion { 0 };
    std::atomic<int> targetKey { -1 }, transpose { 0 }, detectedTonic { -1 }, firstOnset { 0 };
    std::atomic<bool> clearVoices { false }, gate { false };
    Voice voices[32];
    int nextVoice = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoulKutProcessor)
};
