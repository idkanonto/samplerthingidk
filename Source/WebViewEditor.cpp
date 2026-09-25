#include "WebViewEditor.h"
#include <BinaryData.h>
#include <algorithm>
#include <cmath>

namespace
{
constexpr auto parameterValueEvent = "parameterValue";
constexpr auto parameterGestureEvent = "parameterGesture";
constexpr auto parameterChangedEvent = "parameterChanged";
constexpr auto backendCommandEvent = "backendCommand";
constexpr auto backendStateEvent = "backendState";
constexpr auto visualisationEvent = "visualisationState";

juce::var objectWithType(const juce::String& type)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("type", type);
    return juce::var(object);
}

std::vector<std::byte> bytesFrom(const char* data, int size)
{
    const auto* first = reinterpret_cast<const std::byte*>(data);
    return { first, first + size };
}

std::optional<juce::WebBrowserComponent::Resource> makeResource(
    const juce::String& path)
{
    if (path == "/" || path == "/index.html")
        return juce::WebBrowserComponent::Resource {
            bytesFrom(BinaryData::index_html, BinaryData::index_htmlSize), "text/html" };
    if (path == "/assets/app.js")
        return juce::WebBrowserComponent::Resource {
            bytesFrom(BinaryData::app_js, BinaryData::app_jsSize), "text/javascript" };
    if (path == "/assets/app.css")
        return juce::WebBrowserComponent::Resource {
            bytesFrom(BinaryData::app_css, BinaryData::app_cssSize), "text/css" };
    if (path == "/assets/geist-pixel-square.woff2")
        return juce::WebBrowserComponent::Resource {
            bytesFrom(BinaryData::geistpixelsquare_woff2, BinaryData::geistpixelsquare_woff2Size), "font/woff2" };
    if (path == "/assets/space-mono-regular.ttf")
        return juce::WebBrowserComponent::Resource {
            bytesFrom(BinaryData::spacemonoregular_ttf, BinaryData::spacemonoregular_ttfSize), "font/ttf" };
    if (path == "/assets/space-mono-bold.ttf")
        return juce::WebBrowserComponent::Resource {
            bytesFrom(BinaryData::spacemonobold_ttf, BinaryData::spacemonobold_ttfSize), "font/ttf" };
    return std::nullopt;
}
}

RandomChopSamplerWebViewEditor::RandomChopSamplerWebViewEditor(
    RandomChopSamplerAudioProcessor& p)
    : AudioProcessorEditor(&p),
      processor(p),
      selectedSampleId(p.getSelectedSampleId()),
      browser(createBrowserOptions())
{
    for (auto& dirty : dirtyParameters)
        dirty.store(false, std::memory_order_relaxed);

    for (auto* parameter : processor.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);
        if (ranged == nullptr)
            continue;
        const auto index = parameter->getParameterIndex();
        parameterBindings.push_back({ ranged->getParameterID(), ranged, index });
        parameter->addListener(this);
    }

    lastPool = processor.samples.getSnapshot();
    ensureValidSelection(lastPool);
    lastSpectralGeneration = processor.getSpectralCanvasGeneration();
    addAndMakeVisible(browser);
    setResizable(false, false);
    applyEditorScale();
    browser.goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
    startTimerHz(30);
}

RandomChopSamplerWebViewEditor::~RandomChopSamplerWebViewEditor()
{
    stopTimer();
    for (const auto& binding : parameterBindings)
        binding.parameter->removeListener(this);
}

juce::WebBrowserComponent::Options
RandomChopSamplerWebViewEditor::createBrowserOptions()
{
    return juce::WebBrowserComponent::Options {}
        .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
        .withWinWebView2Options(juce::WebBrowserComponent::Options::WinWebView2 {}
            .withStatusBarDisabled()
            .withUserDataFolder(juce::File::getSpecialLocation(
                juce::File::tempDirectory).getChildFile("recompiler-dll-webview")))
        .withNativeIntegrationEnabled()
        .withFileDropListener([this](const juce::StringArray& files) { addFiles(files); })
        .withInitialisationData("parameters", createParameterState())
        .withEventListener(parameterValueEvent,
            [this](const juce::var& value) { handleParameterValue(value); })
        .withEventListener(parameterGestureEvent,
            [this](const juce::var& value) { handleParameterGesture(value); })
        .withEventListener(backendCommandEvent,
            [this](const juce::var& value) { handleCommand(value); })
        .withResourceProvider(
            [this](const juce::String& url) { return getResource(url); });
}

std::optional<juce::WebBrowserComponent::Resource>
RandomChopSamplerWebViewEditor::getResource(const juce::String& requestedUrl) const
{
    auto path = requestedUrl.upToFirstOccurrenceOf("?", false, false)
                            .upToFirstOccurrenceOf("#", false, false);
    return makeResource(path);
}

juce::var RandomChopSamplerWebViewEditor::createParameterState() const
{
    juce::Array<juce::var> parameters;
    for (auto* parameter : processor.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);
        if (ranged == nullptr)
            continue;
        const auto range = ranged->getNormalisableRange();
        auto* descriptor = new juce::DynamicObject();
        descriptor->setProperty("id", ranged->getParameterID());
        descriptor->setProperty("name", ranged->getName(128));
        descriptor->setProperty("label", ranged->getLabel());
        descriptor->setProperty("value", range.convertFrom0to1(ranged->getValue()));
        descriptor->setProperty("defaultValue",
                                range.convertFrom0to1(ranged->getDefaultValue()));
        descriptor->setProperty("min", range.start);
        descriptor->setProperty("max", range.end);
        descriptor->setProperty("interval", range.interval);
        descriptor->setProperty("numSteps", ranged->getNumSteps());
        descriptor->setProperty("isDiscrete", ranged->isDiscrete());
        descriptor->setProperty("isBoolean", ranged->isBoolean());
        parameters.add(juce::var(descriptor));
    }
    return parameters;
}

juce::var RandomChopSamplerWebViewEditor::createBackendState()
{
    const auto pool = processor.samples.getSnapshot();
    ensureValidSelection(pool);
    auto result = objectWithType("snapshot");
    auto* object = result.getDynamicObject();
    object->setProperty("selectedSampleId", selectedSampleId);
    object->setProperty("sampleCount", static_cast<int>(pool->size()));
    object->setProperty("maximumSampleCount", SampleManager::maximumSamples);
    object->setProperty("voiceCount", processor.getActiveVoiceCount());
    object->setProperty("outputMuted", processor.isOutputMuted());
    object->setProperty("uiScale", processor.getUiScaleIndex());
    object->setProperty("importMessage", importMessage);
    juce::Array<juce::var> effectEnabled;
    for (int effect = 0; effect < 4; ++effect)
        effectEnabled.add(processor.isEffectEnabled(effect));
    object->setProperty("effectEnabled", effectEnabled);

    juce::Array<juce::var> samples;
    for (const auto& sample : *pool)
    {
        auto* item = new juce::DynamicObject();
        const auto& settings = sample->settings;
        item->setProperty("id", settings.id);
        item->setProperty("name", settings.displayName);
        item->setProperty("enabled", settings.enabled);
        item->setProperty("missing", settings.missing);
        item->setProperty("start", settings.startNormalised);
        item->setProperty("end", settings.endNormalised);
        item->setProperty("sourceKey", settings.sourceKey);
        item->setProperty("transpose", settings.transposeSemitones);
        item->setProperty("fineTune", settings.fineTuneCents);
        item->setProperty("gainDb", settings.gainDb);
        item->setProperty("sampleRate", sample->sampleRate);
        item->setProperty("bitDepth", sample->bitDepth);
        item->setProperty("durationSeconds", sample->audio != nullptr
            ? static_cast<double>(sample->audio->getNumSamples())
                / std::max(1.0, sample->sampleRate)
            : 0.0);

        if (settings.id == selectedSampleId && sample->waveformPeaks != nullptr)
        {
            juce::Array<juce::var> waveform;
            const auto& peaks = *sample->waveformPeaks;
            constexpr size_t maximumPoints = 256;
            const auto stride = std::max<size_t>(1, (peaks.size() + maximumPoints - 1)
                                                     / maximumPoints);
            for (size_t index = 0; index < peaks.size(); index += stride)
            {
                juce::Array<juce::var> pair;
                pair.add(peaks[index].minimum);
                pair.add(peaks[index].maximum);
                waveform.add(juce::var(pair));
            }
            item->setProperty("waveform", waveform);
        }
        samples.add(juce::var(item));
    }
    object->setProperty("samples", samples);
    object->setProperty("spectralWidth", randomchop::SpectralMaskStore::canvasWidth);
    object->setProperty("spectralHeight", randomchop::SpectralMaskStore::canvasHeight);
    juce::Array<juce::var> spectralCanvas;
    for (const auto value : processor.getSpectralCanvas())
        spectralCanvas.add(value);
    object->setProperty("spectralCanvas", spectralCanvas);
    return result;
}

juce::var RandomChopSamplerWebViewEditor::createVisualisationState() const
{
    auto result = objectWithType("frame");
    auto* object = result.getDynamicObject();
    object->setProperty("outputPeak", processor.getOutputPeak());
    object->setProperty("outputPeakLeft", processor.getOutputPeakLeft());
    object->setProperty("outputPeakRight", processor.getOutputPeakRight());
    object->setProperty("voiceCount", processor.getActiveVoiceCount());
    object->setProperty("scramblePhase", processor.getScrambleVisualPhase());
    object->setProperty("scrambleFlags", static_cast<int>(processor.getScrambleVisualFlags()));
    object->setProperty("meltStretch", processor.getMeltVisualStretch());
    object->setProperty("meltProgress", processor.getMeltVisualProgress());
    object->setProperty("meltFlags", static_cast<int>(processor.getMeltVisualFlags()));
    object->setProperty("smearActivity", processor.getSmearVisualActivity());
    object->setProperty("smearGain", processor.getSmearVisualGain());
    object->setProperty("spectralScan", processor.getSpectralScanPosition());
    juce::Array<juce::var> spectrum;
    for (const auto value : processor.getDisplaySpectrum())
        spectrum.add(value);
    object->setProperty("spectrum", spectrum);
    return result;
}

void RandomChopSamplerWebViewEditor::handleParameterValue(const juce::var& payload)
{
    const auto id = payload.getProperty("id", {}).toString();
    auto* binding = findParameter(id);
    if (binding == nullptr)
        return;
    const auto value = static_cast<float>(payload.getProperty("value", 0.0));
    if (!std::isfinite(value))
        return;
    const auto normalised = binding->parameter->getNormalisableRange().convertTo0to1(value);
    binding->parameter->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, normalised));
}

void RandomChopSamplerWebViewEditor::handleParameterGesture(const juce::var& payload)
{
    const auto id = payload.getProperty("id", {}).toString();
    auto* binding = findParameter(id);
    if (binding == nullptr)
        return;
    const auto phase = payload.getProperty("phase", {}).toString();
    if (phase == "begin")
        binding->parameter->beginChangeGesture();
    else if (phase == "end")
        binding->parameter->endChangeGesture();
}

void RandomChopSamplerWebViewEditor::handleCommand(const juce::var& payload)
{
    const auto type = payload.getProperty("type", {}).toString();
    const auto id = payload.getProperty("id", {}).toString();
    if (type == "requestState")
    {
        backendStateDirty = true;
    }
    else if (type == "selectSample")
    {
        selectedSampleId = id;
        processor.setSelectedSampleId(selectedSampleId);
        backendStateDirty = true;
    }
    else if (type == "setSampleEnabled")
    {
        processor.samples.setEnabled(id,
            static_cast<bool>(payload.getProperty("enabled", true)));
        backendStateDirty = true;
    }
    else if (type == "removeSample")
    {
        processor.requestSourcePreview(0);
        processor.samples.remove(id);
        backendStateDirty = true;
    }
    else if (type == "importSamples")
    {
        openFileChooser();
    }
    else if (type == "setSampleRegion")
    {
        const auto start = static_cast<double>(payload.getProperty("start", 0.0));
        const auto end = static_cast<double>(payload.getProperty("end", 1.0));
        processor.samples.updateSettings(id, [start, end](SampleSettings& settings)
        {
            settings.startNormalised = start;
            settings.endNormalised = end;
        });
        backendStateDirty = true;
    }
    else if (type == "setSampleProperty")
    {
        const auto property = payload.getProperty("property", {}).toString();
        const auto value = static_cast<double>(payload.getProperty("value", 0.0));
        processor.samples.updateSettings(id, [property, value](SampleSettings& settings)
        {
            if (property == "sourceKey") settings.sourceKey = static_cast<int>(value);
            else if (property == "transpose") settings.transposeSemitones = static_cast<int>(value);
            else if (property == "fineTune") settings.fineTuneCents = static_cast<float>(value);
            else if (property == "gainDb") settings.gainDb = static_cast<float>(value);
        });
        backendStateDirty = true;
    }
    else if (type == "resetSpectral")
    {
        processor.clearSpectralCanvas();
        backendStateDirty = true;
    }
    else if (type == "setSpectralCanvas")
    {
        if (const auto* values = payload.getProperty("values", {}).getArray();
            values != nullptr
            && values->size() == randomchop::SpectralMaskStore::cellCount)
        {
            randomchop::SpectralMaskStore::Canvas canvas {};
            for (int index = 0; index < values->size(); ++index)
                canvas[static_cast<size_t>(index)] = juce::jlimit(0.0f, 1.0f,
                    static_cast<float>((*values)[index]));
            processor.setSpectralCanvas(canvas);
        }
    }
    else if (type == "regenerateSeed")
    {
        processor.regenerateCreativeSeed();
    }
    else if (type == "setOutputMuted")
    {
        processor.setOutputMuted(static_cast<bool>(payload.getProperty("enabled", false)));
        backendStateDirty = true;
    }
    else if (type == "setEffectEnabled")
    {
        processor.setEffectEnabled(static_cast<int>(payload.getProperty("effect", -1)),
            static_cast<bool>(payload.getProperty("enabled", true)));
        backendStateDirty = true;
    }
    else if (type == "setUiScale")
    {
        processor.setUiScaleIndex(static_cast<int>(payload.getProperty("index", 1)));
        applyEditorScale();
        backendStateDirty = true;
    }
}

void RandomChopSamplerWebViewEditor::emitBackendState()
{
    browser.emitEventIfBrowserIsVisible(backendStateEvent, createBackendState());
    backendStateDirty = false;
}

void RandomChopSamplerWebViewEditor::openFileChooser()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Add samples", juce::File {}, "*.wav;*.aif;*.aiff;*.mp3;*.flac", true);
    juce::Component::SafePointer<RandomChopSamplerWebViewEditor> safe(this);
    fileChooser->launchAsync(juce::FileBrowserComponent::openMode
        | juce::FileBrowserComponent::canSelectFiles
        | juce::FileBrowserComponent::canSelectMultipleItems,
        [safe](const juce::FileChooser& chooser)
        {
            if (safe == nullptr)
                return;
            juce::StringArray paths;
            for (const auto& file : chooser.getResults())
                paths.add(file.getFullPathName());
            if (!paths.isEmpty())
                safe->addFiles(paths);
        });
}

void RandomChopSamplerWebViewEditor::addFiles(const juce::StringArray& files)
{
    const auto before = processor.samples.getSnapshot();
    const auto beforeCount = static_cast<int>(before->size());
    const auto errors = processor.samples.addFiles(files);
    const auto after = processor.samples.getSnapshot();
    const auto added = static_cast<int>(after->size()) - beforeCount;
    if (added > 0 && static_cast<size_t>(beforeCount) < after->size())
    {
        selectedSampleId = (*after)[static_cast<size_t>(beforeCount)]->settings.id;
        processor.setSelectedSampleId(selectedSampleId);
    }
    importMessage = juce::String(added) + (added == 1 ? " FILE ADDED" : " FILES ADDED");
    if (!errors.empty())
        importMessage += " · " + juce::String(static_cast<int>(errors.size())) + " REJECTED";
    backendStateDirty = true;
}

bool RandomChopSamplerWebViewEditor::pageAboutToLoad(const juce::String& url)
{
    return url == "about:blank"
        || url.startsWith(juce::WebBrowserComponent::getResourceProviderRoot());
}

void RandomChopSamplerWebViewEditor::applyEditorScale()
{
    static constexpr std::array<juce::Point<int>, 4> sizes {
        juce::Point<int> { 720, 485 }, juce::Point<int> { 960, 647 },
        juce::Point<int> { 1200, 809 }, juce::Point<int> { 1440, 971 }
    };
    appliedUiScale = processor.getUiScaleIndex();
    const auto size = sizes[static_cast<size_t>(appliedUiScale)];
    setSize(size.x, size.y);
}

void RandomChopSamplerWebViewEditor::ensureValidSelection(
    const std::shared_ptr<const SampleManager::Pool>& pool)
{
    const auto selectedExists = std::any_of(pool->begin(), pool->end(), [this](const auto& sample)
    {
        return sample->settings.id == selectedSampleId;
    });
    if (!selectedExists)
        selectedSampleId = pool->empty() ? juce::String() : pool->front()->settings.id;
    processor.setSelectedSampleId(selectedSampleId);
}

RandomChopSamplerWebViewEditor::ParameterBinding*
RandomChopSamplerWebViewEditor::findParameter(const juce::String& id) noexcept
{
    const auto match = std::find_if(parameterBindings.begin(), parameterBindings.end(),
        [&id](const auto& binding) { return binding.id == id; });
    return match == parameterBindings.end() ? nullptr : &*match;
}

void RandomChopSamplerWebViewEditor::parameterValueChanged(int parameterIndex, float)
{
    if (parameterIndex >= 0
        && static_cast<size_t>(parameterIndex) < dirtyParameters.size())
        dirtyParameters[static_cast<size_t>(parameterIndex)].store(true,
            std::memory_order_release);
}

void RandomChopSamplerWebViewEditor::timerCallback()
{
    processor.samples.collectGarbage();
    if (appliedUiScale != processor.getUiScaleIndex())
        applyEditorScale();
    for (const auto& binding : parameterBindings)
    {
        if (binding.processorIndex < 0
            || static_cast<size_t>(binding.processorIndex) >= dirtyParameters.size()
            || !dirtyParameters[static_cast<size_t>(binding.processorIndex)].exchange(
                false, std::memory_order_acq_rel))
            continue;
        auto update = objectWithType("value");
        update.getDynamicObject()->setProperty("id", binding.id);
        update.getDynamicObject()->setProperty("value",
            binding.parameter->getNormalisableRange().convertFrom0to1(
                binding.parameter->getValue()));
        browser.emitEventIfBrowserIsVisible(parameterChangedEvent, update);
    }

    const auto pool = processor.samples.getSnapshot();
    const auto spectralGeneration = processor.getSpectralCanvasGeneration();
    if (pool != lastPool || spectralGeneration != lastSpectralGeneration)
    {
        lastPool = pool;
        lastSpectralGeneration = spectralGeneration;
        backendStateDirty = true;
    }
    if (backendStateDirty)
        emitBackendState();
    browser.emitEventIfBrowserIsVisible(visualisationEvent, createVisualisationState());
}

void RandomChopSamplerWebViewEditor::resized()
{
    browser.setBounds(getLocalBounds());
}

bool RandomChopSamplerWebViewEditor::isInterestedInFileDrag(
    const juce::StringArray& files)
{
    return std::any_of(files.begin(), files.end(), [](const auto& path)
    {
        return SampleManager::isSupported(juce::File(path));
    });
}

void RandomChopSamplerWebViewEditor::filesDropped(
    const juce::StringArray& files, int, int)
{
    addFiles(files);
}
