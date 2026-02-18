#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <cstring>
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

        for (int i = 0; i < numSamples; ++i)
        {
            float out = 0.0f;
            if (playhead < audioQueue.size())
                out = audioQueue[playhead++];

            left[i] = out;
            if (right != nullptr)
                right[i] = out;
        }

        if (playhead >= audioQueue.size())
        {
            const bool hadAudio = ! audioQueue.empty();
            audioQueue.clear();
            playhead = 0;
            if (hadAudio)
                setStatus ("Idle");
        }
    }

private:
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

        if (auto f = check (juce::File::getCurrentWorkingDirectory()); f.existsAsFile())
            return f;

        auto exeDir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
        for (int depth = 0; depth < 10 && exeDir.isDirectory(); ++depth)
        {
            if (auto f = check (exeDir); f.existsAsFile())
                return f;
            exeDir = exeDir.getParentDirectory();
        }

        return {};
    }

    static juce::String findNodeBinary()
    {
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
            setStatus ("Failed to launch Node: " + nodePath);
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
    size_t playhead = 0;

    mutable juce::SpinLock statusLock;
    mutable juce::String statusText { "Idle" };
};
