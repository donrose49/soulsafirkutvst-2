#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Analyzer.h"

SoulKutProcessor::SoulKutProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    formats.registerBasicFormats();
}

bool SoulKutProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

std::shared_ptr<juce::AudioBuffer<float>> SoulKutProcessor::getSample() const
{
    juce::SpinLock::ScopedLockType sl (sampleLock);
    return sample;
}

juce::String SoulKutProcessor::getKeyName() const { juce::ScopedLock sl (infoLock); return keyName; }
juce::String SoulKutProcessor::getFileName() const { juce::ScopedLock sl (infoLock); return fileName; }

double SoulKutProcessor::getOffsetSamples() const { return offsetMs.load() * 0.001 * sampleRate.load(); }

void SoulKutProcessor::autoOffset()
{
    offsetMs = juce::jmin (maxOffsetMs, (double) firstOnset.load() / sampleRate.load() * 1000.0);
    ++analysisVersion;
}

int SoulKutProcessor::getKeyShift() const
{
    const int t = targetKey.load(), d0 = detectedTonic.load();
    if (t < 0 || d0 < 0) return 0;
    int d = (((t - d0) % 12) + 12) % 12;
    if (d > 6) d -= 12;                 // chemin le plus court (-5..+6 demi-tons)
    return d;
}

double SoulKutProcessor::getSliceLength() const
{
    const double b = bpm.load();
    if (b <= 0.0) return 0.0;
    // 1 temps (noire) = 60/bpm s ; une mesure = 4 temps ; 1/N de mesure = 4/N temps
    return 60.0 / b * 4.0 / (double) division.load() * sampleRate.load();
}

int SoulKutProcessor::getNumSlices() const
{
    const double len = getSliceLength();
    if (len < 1.0) return 0;
    return juce::jmax (0, (int) std::floor (((double) sampleLength.load() - getOffsetSamples()) / len));
}

int SoulKutProcessor::getSliceStart (int i) const
{
    return (int) std::lround (getOffsetSamples() + (double) i * getSliceLength());
}

bool SoulKutProcessor::loadSample (const juce::File& f)
{
    std::unique_ptr<juce::AudioFormatReader> r (formats.createReaderFor (f));
    if (r == nullptr || r->lengthInSamples <= 0 || r->lengthInSamples > (juce::int64) (r->sampleRate * 600.0))
        return false;

    auto buf = std::make_shared<juce::AudioBuffer<float>> ((int) r->numChannels, (int) r->lengthInSamples);
    r->read (buf.get(), 0, (int) r->lengthInSamples, 0, true, true);

    std::vector<float> mono;
    Analyzer::toMono (*buf, mono);
    const double detected = Analyzer::detectBpm (mono, r->sampleRate);
    bool minor = false;
    const int tonic = Analyzer::detectKey (mono, r->sampleRate, minor);
    const int onset = Analyzer::detectFirstOnset (mono, r->sampleRate);

    static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const juce::String key = minor
        ? juce::String (names[tonic]) + " min  (rel. " + names[(tonic + 3) % 12] + " maj)"
        : juce::String (names[tonic]) + " maj  (rel. " + names[(tonic + 9) % 12] + " min)";

    {
        juce::SpinLock::ScopedLockType sl (sampleLock);
        sample = buf;
        sampleRate = r->sampleRate;
        sampleLength = buf->getNumSamples();
    }
    {
        juce::ScopedLock sl (infoLock);
        keyName = key;
        fileName = f.getFullPathName();
    }
    if (detected > 0.0) bpm = std::round (detected * 100.0) / 100.0;
    detectedTonic = tonic;
    firstOnset = onset;
    offsetMs = juce::jmin (maxOffsetMs, (double) onset / r->sampleRate * 1000.0);
    clearVoices = true;
    ++analysisVersion;
    return true;
}

juce::File SoulKutProcessor::getSliceFile (const juce::File& folder, int i) const
{
    const auto base = juce::File (getFileName()).getFileNameWithoutExtension();
    const auto key = getKeyName().upToFirstOccurrenceOf (" ", false, false);
    return folder.getChildFile (base + "_" + juce::String ((int) std::round (getBpm())) + "bpm_" + key
                                + "_1-" + juce::String (getDivision()) + "_"
                                + juce::String (i + 1).paddedLeft ('0', 3) + ".wav");
}

bool SoulKutProcessor::writeSlice (int i, const juce::File& file) const
{
    auto buf = getSample();
    if (buf == nullptr || i < 0 || i >= getNumSlices()) return false;
    const int s = getSliceStart (i), e = juce::jmin (buf->getNumSamples(), getSliceStart (i + 1));
    if (e <= s) return false;

    file.deleteFile();
    std::unique_ptr<juce::FileOutputStream> fos (file.createOutputStream());
    if (fos == nullptr) return false;
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (fos.get(), getSampleRate(),
                                                                     (unsigned) buf->getNumChannels(), 24, {}, 0));
    if (w == nullptr) return false;
    fos.release(); // le writer en devient propriétaire
    return w->writeFromAudioSampleBuffer (*buf, s, e - s);
}

int SoulKutProcessor::exportSlices (const juce::File& folder)
{
    int written = 0;
    for (int i = 0, n = getNumSlices(); i < n; ++i)
        if (writeSlice (i, getSliceFile (folder, i))) ++written;
    return written;
}

void SoulKutProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    juce::SpinLock::ScopedTryLockType lock (sampleLock);
    if (! lock.isLocked() || sample == nullptr) return;
    if (clearVoices.exchange (false)) for (auto& v : voices) v.active = false;

    const auto& data = *sample;
    const int srcCh = data.getNumChannels(), total = data.getNumSamples(), nch = buffer.getNumChannels();
    const int relLen = juce::jmax (48, (int) (sampleRate.load() * 0.01));
    constexpr int fade = 48;

    auto startVoice = [&] (int slice, int note)
    {
        if (slice < 0 || slice >= getNumSlices()) return;
        const int s = getSliceStart (slice), e = juce::jmin (total, getSliceStart (slice + 1));
        if (e <= s) return;
        int idx = -1;
        for (int i = 0; i < 32; ++i) if (voices[i].active && voices[i].slice == slice) { idx = i; break; }
        if (idx < 0) for (int i = 0; i < 32; ++i) if (! voices[i].active) { idx = i; break; }
        if (idx < 0) { idx = nextVoice; nextVoice = (nextVoice + 1) % 32; }
        auto& v = voices[idx];
        v.active = true; v.released = false; v.slice = slice; v.note = note;
        v.start = s; v.pos = s; v.end = e;
        v.rate = std::pow (2.0, getTotalSemitones() / 12.0);
    };

    auto releaseNote = [&] (int note)
    {
        if (! gate.load()) return;
        for (auto& v : voices)
            if (v.active && ! v.released && v.note == note) { v.released = true; v.relTotal = v.relLeft = relLen; }
    };

    auto render = [&] (int from, int to)
    {
        for (auto& v : voices)
        {
            if (! v.active) continue;
            for (int i = from; i < to; ++i)
            {
                if (v.pos >= v.end) { v.active = false; break; }
                float g = 1.0f;
                const double since = v.pos - v.start, left = v.end - v.pos;
                if (since < fade) g *= (float) (since / fade);
                if (left < fade * v.rate) g *= (float) (left / (fade * v.rate));
                if (v.released)
                {
                    g *= (float) v.relLeft / (float) v.relTotal;
                    if (--v.relLeft <= 0) { v.active = false; break; }
                }
                const int i0 = (int) v.pos, i1 = juce::jmin (i0 + 1, total - 1);
                const float fr = (float) (v.pos - i0);
                for (int ch = 0; ch < nch; ++ch)
                {
                    const int sc = juce::jmin (ch, srcCh - 1);
                    const float s0 = data.getSample (sc, i0), s1 = data.getSample (sc, i1);
                    buffer.addSample (ch, i, g * (s0 + fr * (s1 - s0)));
                }
                v.pos += v.rate;
            }
        }
    };

    const int pending = pendingTrigger.exchange (-1);
    if (pending >= 0) startVoice (pending, -1);

    int cursor = 0;
    const int n = buffer.getNumSamples();
    for (const auto meta : midi)
    {
        const int p = juce::jlimit (0, n, meta.samplePosition);
        render (cursor, p);
        cursor = p;
        const auto m = meta.getMessage();
        if (m.isNoteOn()) startVoice (m.getNoteNumber() - firstNote, m.getNoteNumber());
        else if (m.isNoteOff()) releaseNote (m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff()) for (auto& v : voices) v.active = false;
    }
    render (cursor, n);
}

void SoulKutProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::XmlElement xml ("SOULKUT");
    xml.setAttribute ("file", getFileName());
    xml.setAttribute ("bpm", getBpm());
    xml.setAttribute ("division", getDivision());
    xml.setAttribute ("offsetMs", getOffsetMs());
    xml.setAttribute ("gate", getGate());
    xml.setAttribute ("targetKey", getTargetKey());
    xml.setAttribute ("transpose", getTranspose());
    copyXmlToBinary (xml, dest);
}

void SoulKutProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        if (xml->hasTagName ("SOULKUT"))
        {
            juce::File f (xml->getStringAttribute ("file"));
            if (f.existsAsFile()) loadSample (f);
            setBpm (xml->getDoubleAttribute ("bpm", 120.0));
            setDivision (xml->getIntAttribute ("division", 16));
            setOffsetMs (xml->getDoubleAttribute ("offsetMs", 0.0));
            setGate (xml->getBoolAttribute ("gate", false));
            setTargetKey (xml->getIntAttribute ("targetKey", -1));
            setTranspose (xml->getIntAttribute ("transpose", 0));
            ++analysisVersion;
        }
    }
}

juce::AudioProcessorEditor* SoulKutProcessor::createEditor() { return new SoulKutEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SoulKutProcessor(); }
