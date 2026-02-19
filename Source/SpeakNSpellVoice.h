#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include <array>
#include <cmath>
#include <cstring>
#include <optional>
#include <vector>

class SpeakNSpellVoice
{
public:
    struct Parameters
    {
        enum class Backend
        {
            classicSam,
            betterSam
        };

        int speed = 72;
        int pitch = 64;
        int mouth = 128;
        int throat = 128;
        bool singMode = false;
        bool phoneticInput = false;
        Backend backend = Backend::classicSam;
    };

    struct RealtimeControls
    {
        float playbackSpeed = 1.0f;
        float repitchSemitones = 0.0f;
        float formantWarp = 0.0f;
        float glitchGate = 0.0f;
        float bitCrush = 0.0f;
        float microLoop = 0.0f;
        float spectralTilt = 0.0f;
        float ringMod = 0.0f;
        float freqShift = 0.0f;
        float repitchJitter = 0.0f;
        float mutation = 0.0f;
    };

    static int getNumFactoryPresets()
    {
        return 10;
    }

    static juce::String getFactoryPresetName (int index)
    {
        static const char* names[] =
        {
            "Classic SAM",
            "Soft Tutor",
            "Toy Robot",
            "Singy Crystal",
            "Radio Glitch",
            "Monster Pipe",
            "Chipmunk Pop",
            "Haunted PA",
            "Broken Console",
            "Cyber Oracle"
        };

        if (index < 0 || index >= getNumFactoryPresets())
            return "Preset";
        return names[index];
    }

    static void applyFactoryPreset (int index, Parameters& p, RealtimeControls& r)
    {
        p = {};
        r = {};

        switch (juce::jlimit (0, getNumFactoryPresets() - 1, index))
        {
            case 0:
                p.speed = 72; p.pitch = 64; p.mouth = 128; p.throat = 128;
                break;
            case 1:
                p.speed = 82; p.pitch = 58; p.mouth = 110; p.throat = 140;
                r.spectralTilt = 0.40f;
                break;
            case 2:
                p.speed = 68; p.pitch = 80; p.mouth = 170; p.throat = 100;
                r.formantWarp = 0.48f; r.ringMod = 0.18f;
                break;
            case 3:
                p.speed = 74; p.pitch = 92; p.mouth = 160; p.throat = 150;
                p.singMode = true;
                r.playbackSpeed = 1.08f; r.repitchSemitones = 2.0f; r.spectralTilt = 0.62f;
                break;
            case 4:
                p.speed = 78; p.pitch = 70; p.mouth = 140; p.throat = 128;
                r.glitchGate = 0.52f; r.bitCrush = 0.42f; r.microLoop = 0.34f;
                break;
            case 5:
                p.speed = 60; p.pitch = 36; p.mouth = 90; p.throat = 60;
                r.repitchSemitones = -7.0f; r.formantWarp = 0.72f; r.freqShift = 0.20f;
                break;
            case 6:
                p.speed = 96; p.pitch = 122; p.mouth = 190; p.throat = 180;
                r.repitchSemitones = 7.0f; r.repitchJitter = 0.10f;
                break;
            case 7:
                p.speed = 66; p.pitch = 54; p.mouth = 100; p.throat = 90;
                r.microLoop = 0.24f; r.ringMod = 0.32f; r.spectralTilt = 0.18f;
                break;
            case 8:
                p.speed = 84; p.pitch = 62; p.mouth = 130; p.throat = 120;
                r.bitCrush = 0.68f; r.glitchGate = 0.28f; r.repitchJitter = 0.32f; r.mutation = 0.36f;
                break;
            case 9:
                p.speed = 70; p.pitch = 76; p.mouth = 150; p.throat = 132;
                r.formantWarp = 0.35f; r.freqShift = 0.46f; r.spectralTilt = 0.64f; r.mutation = 0.22f;
                break;
            default:
                break;
        }
    }

    void setSampleRate (double newSampleRate)
    {
        sampleRate = juce::jmax (8000.0, newSampleRate);
    }

    void queueText (juce::String text, Parameters params)
    {
        text = text.trim();
        if (text.isEmpty())
        {
            setStatus ("Idle");
            return;
        }

        setStatus ("Rendering SAM...");

        text = mutateTextForRealtimeEffects (text, getRealtimeControls().mutation);
        auto samSamples = renderSamSamples (text, params);
        if (samSamples.empty())
        {
            if (getStatusText().startsWith ("Rendering"))
                setStatus ("SAM render failed");
            return;
        }

        auto resampled = resample (samSamples, 22050.0, sampleRate);
        if (resampled.empty())
        {
            setStatus ("Resample failed");
            return;
        }

        const juce::SpinLock::ScopedLockType sl (audioLock);
        audioQueue.insert (audioQueue.end(), resampled.begin(), resampled.end());

        const auto gapSamples = static_cast<int> (0.04 * sampleRate);
        audioQueue.insert (audioQueue.end(), static_cast<size_t> (juce::jmax (0, gapSamples)), 0.0f);
        loopSource = resampled;
        loopSource.insert (loopSource.end(), static_cast<size_t> (juce::jmax (0, gapSamples)), 0.0f);
        setStatus ("Queued " + juce::String (static_cast<int> (resampled.size())) + " samples");
    }

    juce::String getStatusText() const
    {
        const juce::SpinLock::ScopedLockType sl (statusLock);
        return statusText;
    }

    void render (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
    {
        auto* left = buffer.getWritePointer (0, startSample);
        auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1, startSample) : nullptr;

        const juce::SpinLock::ScopedLockType sl (audioLock);
        const auto controls = getRealtimeControls();
        const auto speed = juce::jlimit (0.25f, 4.0f, controls.playbackSpeed);
        const auto pitchRatio = std::pow (2.0, static_cast<double> (controls.repitchSemitones) / 12.0);

        for (int i = 0; i < numSamples; ++i)
        {
            float out = 0.0f;
            const auto jitter = nextJitterRatio (controls.repitchJitter);
            const auto step = juce::jlimit (0.05, 8.0, static_cast<double> (speed) * pitchRatio * jitter);
            const auto sample = getSampleAtPlayhead();
            if (sample.has_value())
            {
                out = *sample;
                playhead += step;
            }

            out = processRealtimeEffects (out, controls);

            left[i] = out;
            if (right != nullptr)
                right[i] = out;
        }

        if (playhead >= audioQueue.size())
        {
            if (loopAtEnd.load() && ! loopSource.empty())
            {
                audioQueue = loopSource;
                playhead = 0.0;
                setStatus ("Looping");
            }
            else
            {
                const bool hadAudio = ! audioQueue.empty();
                audioQueue.clear();
                playhead = 0.0;
                if (hadAudio)
                    setStatus ("Idle");
            }
        }
    }

    void setLoopAtEnd (bool shouldLoop)
    {
        loopAtEnd.store (shouldLoop);
    }

    bool getLoopAtEnd() const
    {
        return loopAtEnd.load();
    }

    void setRealtimeControls (const RealtimeControls& controls)
    {
        playbackSpeed.store (juce::jlimit (0.25f, 4.0f, controls.playbackSpeed));
        repitchSemitones.store (juce::jlimit (-24.0f, 24.0f, controls.repitchSemitones));
        formantWarp.store (juce::jlimit (0.0f, 1.0f, controls.formantWarp));
        glitchGate.store (juce::jlimit (0.0f, 1.0f, controls.glitchGate));
        bitCrush.store (juce::jlimit (0.0f, 1.0f, controls.bitCrush));
        microLoop.store (juce::jlimit (0.0f, 1.0f, controls.microLoop));
        spectralTilt.store (juce::jlimit (0.0f, 1.0f, controls.spectralTilt));
        ringMod.store (juce::jlimit (0.0f, 1.0f, controls.ringMod));
        freqShift.store (juce::jlimit (0.0f, 1.0f, controls.freqShift));
        repitchJitter.store (juce::jlimit (0.0f, 1.0f, controls.repitchJitter));
        mutation.store (juce::jlimit (0.0f, 1.0f, controls.mutation));
    }

    RealtimeControls getRealtimeControls() const
    {
        RealtimeControls c;
        c.playbackSpeed = playbackSpeed.load();
        c.repitchSemitones = repitchSemitones.load();
        c.formantWarp = formantWarp.load();
        c.glitchGate = glitchGate.load();
        c.bitCrush = bitCrush.load();
        c.microLoop = microLoop.load();
        c.spectralTilt = spectralTilt.load();
        c.ringMod = ringMod.load();
        c.freqShift = freqShift.load();
        c.repitchJitter = repitchJitter.load();
        c.mutation = mutation.load();
        return c;
    }

    void setCustomNodePath (juce::String path)
    {
        path = path.trim();
        const juce::SpinLock::ScopedLockType sl (nodePathLock);
        customNodePath = path;
    }

    juce::String getCustomNodePath() const
    {
        const juce::SpinLock::ScopedLockType sl (nodePathLock);
        return customNodePath;
    }

private:
    juce::String mutateTextForRealtimeEffects (const juce::String& text, float amount) const
    {
        amount = juce::jlimit (0.0f, 1.0f, amount);
        if (amount < 0.01f)
            return text;

        juce::Random localRng;
        juce::String out;
        out.preallocateBytes (text.getNumBytesAsUTF8() + 16);

        for (int i = 0; i < text.length(); ++i)
        {
            const auto ch = text[i];
            if (ch == '\n' || ch == '\r')
            {
                out << ch;
                continue;
            }

            const auto r = localRng.nextFloat();
            if (r < 0.08f * amount && out.isNotEmpty())
            {
                out << out[out.length() - 1];
                continue;
            }

            if (r < 0.14f * amount)
                continue;

            out << ch;

            if (localRng.nextFloat() < 0.12f * amount)
                out << ch;
        }

        return out.trim().isEmpty() ? text : out;
    }

    double nextJitterRatio (float jitterAmount)
    {
        jitterAmount = juce::jlimit (0.0f, 1.0f, jitterAmount);
        if (jitterAmount <= 0.001f)
            return 1.0;

        if (--jitterCounter <= 0)
        {
            const auto span = 0.35 * static_cast<double> (jitterAmount);
            jitterRatio = 1.0 + rng.nextDouble() * (2.0 * span) - span;
            jitterCounter = juce::jmax (1, static_cast<int> (sampleRate * (0.008 + (0.08 * (1.0 - jitterAmount)))));
        }

        return jitterRatio;
    }

    float processRealtimeEffects (float in, const RealtimeControls& controls)
    {
        auto out = in;
        out = applyMicroLoop (out, controls.microLoop);
        out = applyFormantWarp (out, controls.formantWarp);
        out = applySpectralTilt (out, controls.spectralTilt);
        out = applyGlitchGate (out, controls.glitchGate);
        out = applyBitCrush (out, controls.bitCrush);
        out = applyRingMod (out, controls.ringMod);
        out = applyFrequencyShift (out, controls.freqShift);
        return juce::jlimit (-1.0f, 1.0f, out);
    }

    float applyFormantWarp (float x, float amount)
    {
        amount = juce::jlimit (0.0f, 1.0f, amount);
        if (amount <= 0.001f)
            return x;

        const auto warp = 0.75 + 1.5 * static_cast<double> (amount);
        const auto f1 = juce::jlimit (0.002, 0.25, (900.0 * warp) / sampleRate);
        const auto f2 = juce::jlimit (0.002, 0.25, (2200.0 * warp) / sampleRate);

        formant1 += static_cast<float> (f1) * (x - formant1);
        const auto hp1 = x - formant1;
        formantBand1 += static_cast<float> (f1) * (hp1 - formantBand1);

        formant2 += static_cast<float> (f2) * (x - formant2);
        const auto hp2 = x - formant2;
        formantBand2 += static_cast<float> (f2) * (hp2 - formantBand2);

        return x + amount * 0.75f * (formantBand1 * 0.7f + formantBand2 * 0.45f);
    }

    float applySpectralTilt (float x, float amount)
    {
        amount = juce::jlimit (0.0f, 1.0f, amount);
        if (amount <= 0.001f)
            return x;

        const auto tilt = (amount * 2.0f) - 1.0f;
        tiltLp += 0.03f * (x - tiltLp);
        const auto hp = x - tiltLp;
        return x + tilt * (tiltLp * 0.8f - hp * 0.55f);
    }

    float applyGlitchGate (float x, float amount)
    {
        amount = juce::jlimit (0.0f, 1.0f, amount);
        if (amount <= 0.001f)
            return x;

        if (--gateCounter <= 0)
        {
            gateCounter = juce::jmax (1, static_cast<int> (sampleRate * (0.005 + (0.08 * (1.0 - amount)))));
            gateOpen = rng.nextFloat() > (0.25f + 0.6f * amount);
        }
        return gateOpen ? x : 0.0f;
    }

    float applyBitCrush (float x, float amount)
    {
        amount = juce::jlimit (0.0f, 1.0f, amount);
        if (amount <= 0.001f)
            return x;

        if (--crushHoldCounter <= 0)
        {
            crushHoldCounter = 1 + static_cast<int> (amount * amount * 42.0f);
            crushHeldSample = x;
        }

        const auto bits = juce::jlimit (3, 16, 16 - static_cast<int> (amount * 13.0f));
        const auto levels = static_cast<float> (1 << bits);
        return std::floor ((crushHeldSample * 0.5f + 0.5f) * levels) / levels * 2.0f - 1.0f;
    }

    float applyRingMod (float x, float amount)
    {
        amount = juce::jlimit (0.0f, 1.0f, amount);
        if (amount <= 0.001f)
            return x;

        const auto freq = 18.0 + 740.0 * static_cast<double> (amount);
        ringPhase += juce::MathConstants<double>::twoPi * (freq / sampleRate);
        if (ringPhase > juce::MathConstants<double>::twoPi)
            ringPhase -= juce::MathConstants<double>::twoPi;
        const auto mod = static_cast<float> (std::sin (ringPhase));
        return x * ((1.0f - amount) + amount * mod);
    }

    float applyFrequencyShift (float x, float amount)
    {
        amount = juce::jlimit (0.0f, 1.0f, amount);
        if (amount <= 0.001f)
            return x;

        const auto freq = 35.0 + 1200.0 * static_cast<double> (amount);
        shiftPhase += juce::MathConstants<double>::twoPi * (freq / sampleRate);
        if (shiftPhase > juce::MathConstants<double>::twoPi)
            shiftPhase -= juce::MathConstants<double>::twoPi;

        const auto carrier = static_cast<float> (std::cos (shiftPhase));
        const auto shifted = x * carrier * 1.8f;
        return juce::jlimit (-1.0f, 1.0f, x * (1.0f - amount * 0.65f) + shifted * amount);
    }

    float applyMicroLoop (float x, float amount)
    {
        amount = juce::jlimit (0.0f, 1.0f, amount);
        history[static_cast<size_t> (historyWrite)] = x;
        historyWrite = (historyWrite + 1) % static_cast<int> (history.size());

        if (amount <= 0.001f)
        {
            loopActive = false;
            return x;
        }

        if (loopActive)
        {
            const auto idx = (loopStart + loopPos) % static_cast<int> (history.size());
            const auto y = history[static_cast<size_t> (idx)];
            loopPos = (loopPos + 1) % juce::jmax (1, loopLength);
            if (--loopRemain <= 0)
                loopActive = false;
            return y;
        }

        if (--loopTriggerCounter <= 0)
        {
            loopTriggerCounter = juce::jmax (1, static_cast<int> (sampleRate * (0.03 + (0.22 * (1.0 - amount)))));
            if (rng.nextFloat() < (0.18f + 0.72f * amount))
            {
                loopLength = juce::jlimit (24, static_cast<int> (history.size()) / 2, 24 + static_cast<int> (amount * 1500.0f));
                loopRemain = juce::jmax (loopLength, static_cast<int> (loopLength * (1.0f + amount * 4.0f)));
                loopStart = (historyWrite + static_cast<int> (history.size()) - loopLength) % static_cast<int> (history.size());
                loopPos = 0;
                loopActive = true;
            }
        }

        return x;
    }

    std::optional<float> getSampleAtPlayhead() const
    {
        if (playhead >= static_cast<double> (audioQueue.size()))
            return std::nullopt;

        const auto i0 = static_cast<size_t> (playhead);
        const auto i1 = juce::jmin (i0 + 1, audioQueue.size() - 1);
        const auto frac = static_cast<float> (playhead - static_cast<double> (i0));

        const auto a = audioQueue[i0];
        const auto b = audioQueue[i1];
        return a + (b - a) * frac;
    }

    void setStatus (const juce::String& text) const
    {
        const juce::SpinLock::ScopedLockType sl (statusLock);
        statusText = text;
    }

    static juce::File findProjectFile (const juce::String& relativePath)
    {
        auto check = [&relativePath] (const juce::File& base) -> juce::File
        {
            if (! base.isDirectory())
                return {};

            auto f = base.getChildFile (relativePath);
            if (f.existsAsFile())
                return f;

            return {};
        };

       #ifdef SAM_PROJECT_ROOT
        if (auto f = check (juce::File (SAM_PROJECT_ROOT)); f.existsAsFile())
            return f;
       #endif

        if (auto f = check (juce::File::getCurrentWorkingDirectory()); f.existsAsFile())
            return f;

        auto exeDir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();

        // App/plugin bundles place runtime JS assets in Contents/Resources.
        if (auto f = check (exeDir.getSiblingFile ("Resources")); f.existsAsFile())
            return f;

        for (int depth = 0; depth < 10 && exeDir.isDirectory(); ++depth)
        {
            if (auto f = check (exeDir); f.existsAsFile())
                return f;
            exeDir = exeDir.getParentDirectory();
        }

        return {};
    }

    juce::String findNodeBinary() const
    {
        auto isValidExe = [] (const juce::String& p) -> bool
        {
            if (p.isEmpty())
                return false;
            juce::File f (p.unquoted().trim());
            return f.existsAsFile();
        };

        auto env = [] (const char* key) -> juce::String
        {
            return juce::SystemStats::getEnvironmentVariable (key, {}).trim();
        };

        if (auto configured = getCustomNodePath(); isValidExe (configured))
            return configured.unquoted();

        if (auto forced = env ("SAM_NODE_PATH"); isValidExe (forced))
            return forced.unquoted();

       #if JUCE_WINDOWS
        auto exeDir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
        const juce::StringArray relativeCandidates {
            "node.exe",
            "..\\node.exe",
            "..\\Resources\\node\\node.exe",
            "..\\..\\Resources\\node\\node.exe",
            "Resources\\node\\node.exe"
        };
        for (const auto& rel : relativeCandidates)
        {
            auto f = exeDir.getChildFile (rel);
            if (f.existsAsFile())
                return f.getFullPathName();
        }

        auto nvmHome = env ("NVM_HOME");
        if (nvmHome.isNotEmpty())
        {
            auto f = juce::File (nvmHome).getChildFile ("node.exe");
            if (f.existsAsFile())
                return f.getFullPathName();
        }
       #endif

        constexpr const char* candidates[] =
        {
            "C:\\Program Files\\nodejs\\node.exe",
            "C:\\Program Files (x86)\\nodejs\\node.exe",
            "/usr/local/bin/node",
            "/opt/homebrew/bin/node",
            "/usr/bin/node"
        };

        for (auto* c : candidates)
        {
            juce::File f (c);
            if (f.existsAsFile())
                return f.getFullPathName();
        }

       #if JUCE_WINDOWS
        juce::ChildProcess whereProc;
        if (whereProc.start ("where node"))
        {
            whereProc.waitForProcessToFinish (1200);
            auto out = whereProc.readAllProcessOutput();
            auto lines = juce::StringArray::fromLines (out);
            for (const auto& line : lines)
            {
                if (isValidExe (line))
                    return line.trim();
            }
        }
       #endif

        return "node";
    }

    static std::vector<float> decodePcm8 (const juce::MemoryBlock& block)
    {
        const auto bytes = block.getSize();
        if (bytes == 0)
            return {};

        const auto* src = static_cast<const uint8_t*> (block.getData());
        const auto count = static_cast<size_t> (bytes);
        std::vector<float> out (count);
        for (size_t i = 0; i < count; ++i)
            out[i] = (static_cast<float> (src[i]) - 128.0f) / 256.0f;
        return out;
    }

    static std::vector<float> resample (const std::vector<float>& in, double sourceRate, double targetRate)
    {
        if (in.empty() || sourceRate <= 0.0 || targetRate <= 0.0)
            return {};

        if (std::abs (sourceRate - targetRate) < 1.0)
            return in;

        const auto ratio = sourceRate / targetRate;
        const auto outCount = static_cast<size_t> (std::max (1.0, std::floor (static_cast<double> (in.size()) * (targetRate / sourceRate))));
        std::vector<float> out;
        out.resize (outCount);

        double pos = 0.0;
        for (size_t i = 0; i < outCount; ++i)
        {
            const auto idx = static_cast<size_t> (std::floor (pos));
            const auto frac = static_cast<float> (pos - static_cast<double> (idx));

            const auto a = in[juce::jmin (idx, in.size() - 1)];
            const auto b = in[juce::jmin (idx + 1, in.size() - 1)];
            out[i] = a + (b - a) * frac;
            pos += ratio;
        }

        return out;
    }

    std::vector<float> renderSamSamples (const juce::String& text, const Parameters& params) const
    {
        auto scriptPath = findProjectFile ("Source/sam_bridge.js");
        if (! scriptPath.existsAsFile())
        {
            setStatus ("Missing Source/sam_bridge.js");
            return {};
        }

        const auto backendLib = (params.backend == Parameters::Backend::betterSam)
                                    ? "third_party/better-samjs.common.js"
                                    : "third_party/samjs.common.js";
        auto samLibPath = findProjectFile (backendLib);
        if (! samLibPath.existsAsFile())
        {
            setStatus ("Missing " + juce::String (backendLib));
            return {};
        }

        juce::ChildProcess proc;
        juce::StringArray args;
        const auto nodePath = findNodeBinary();
        auto tempFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getNonexistentChildFile ("sam-render-", ".pcm", false);

        args.add (nodePath);
        args.add (scriptPath.getFullPathName());
        args.add (tempFile.getFullPathName());
        args.add (juce::String (juce::jlimit (1, 255, params.speed)));
        args.add (juce::String (juce::jlimit (0, 255, params.pitch)));
        args.add (juce::String (juce::jlimit (0, 255, params.mouth)));
        args.add (juce::String (juce::jlimit (0, 255, params.throat)));
        args.add (params.singMode ? "1" : "0");
        args.add (params.phoneticInput ? "1" : "0");
        args.add (params.backend == Parameters::Backend::betterSam ? "better" : "classic");
        args.add (text);

        if (! proc.start (args))
        {
            setStatus ("Failed to launch Node: " + nodePath + " (set SAM_NODE_PATH or install Node.js)");
            return {};
        }

        const auto deadline = juce::Time::getMillisecondCounter() + 12000u;
        while (proc.isRunning() && juce::Time::getMillisecondCounter() < deadline)
        {
            (void) proc.readAllProcessOutput();
            juce::Thread::sleep (10);
        }

        if (proc.isRunning())
        {
            proc.kill();
            setStatus ("SAM render timeout");
            return {};
        }

        (void) proc.readAllProcessOutput();

        juce::MemoryBlock pcmData;
        if (! tempFile.existsAsFile() || ! tempFile.loadFileAsData (pcmData))
        {
            auto errFile = tempFile.getSiblingFile (tempFile.getFileName() + ".err.txt");
            if (errFile.existsAsFile())
            {
                setStatus ("SAM error: " + errFile.loadFileAsString().upToFirstOccurrenceOf ("\n", false, false));
                errFile.deleteFile();
            }
            else
            {
                setStatus ("SAM output file missing");
            }
            return {};
        }
        tempFile.deleteFile();

        return decodePcm8 (pcmData);
    }

    double sampleRate = 44100.0;

    juce::SpinLock audioLock;
    std::vector<float> audioQueue;
    std::vector<float> loopSource;
    double playhead = 0.0;
    std::atomic<bool> loopAtEnd { false };

    std::atomic<float> playbackSpeed { 1.0f };
    std::atomic<float> repitchSemitones { 0.0f };
    std::atomic<float> formantWarp { 0.0f };
    std::atomic<float> glitchGate { 0.0f };
    std::atomic<float> bitCrush { 0.0f };
    std::atomic<float> microLoop { 0.0f };
    std::atomic<float> spectralTilt { 0.0f };
    std::atomic<float> ringMod { 0.0f };
    std::atomic<float> freqShift { 0.0f };
    std::atomic<float> repitchJitter { 0.0f };
    std::atomic<float> mutation { 0.0f };

    juce::Random rng;
    int jitterCounter = 0;
    double jitterRatio = 1.0;

    int gateCounter = 1;
    bool gateOpen = true;

    int crushHoldCounter = 1;
    float crushHeldSample = 0.0f;

    double ringPhase = 0.0;
    double shiftPhase = 0.0;

    float formant1 = 0.0f;
    float formant2 = 0.0f;
    float formantBand1 = 0.0f;
    float formantBand2 = 0.0f;
    float tiltLp = 0.0f;

    std::array<float, 4096> history {};
    int historyWrite = 0;
    bool loopActive = false;
    int loopStart = 0;
    int loopPos = 0;
    int loopLength = 64;
    int loopRemain = 0;
    int loopTriggerCounter = 1;

    mutable juce::SpinLock nodePathLock;
    juce::String customNodePath;

    mutable juce::SpinLock statusLock;
    mutable juce::String statusText { "Idle" };
};
