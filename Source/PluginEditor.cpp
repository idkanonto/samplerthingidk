#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

namespace
{
constexpr auto xpInk = 0xff11275c;
constexpr auto xpPanel = 0xffeaf3fb;

void paintXpPanel(juce::Graphics& g, juce::Rectangle<int> bounds,
                  const juce::String& heading = {})
{
    if (bounds.isEmpty())
        return;

    const auto area = bounds.toFloat();
    g.setColour(juce::Colour(0xff7895b4).withAlpha(0.45f));
    g.fillRoundedRectangle(area.translated(1.0f, 2.0f), 8.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xfff8fbfd), area.getX(), area.getY(),
                                           juce::Colour(xpPanel), area.getX(), area.getBottom(), false));
    g.fillRoundedRectangle(area, 8.0f);
    g.setColour(juce::Colours::white.withAlpha(0.9f));
    g.drawRoundedRectangle(area.reduced(1.0f), 7.0f, 1.2f);
    g.setColour(juce::Colour(0xff6f8fb1));
    g.drawRoundedRectangle(area.reduced(0.5f), 8.0f, 1.3f);

    if (heading.isNotEmpty())
    {
        auto headingArea = area.reduced(3.0f).removeFromTop(25.0f);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xffeaf5ff), headingArea.getX(),
                                               headingArea.getY(), juce::Colour(0xffbcd6ee),
                                               headingArea.getX(), headingArea.getBottom(), false));
        g.fillRoundedRectangle(headingArea, 6.0f);
        g.setColour(juce::Colour(0xff86a6c8));
        g.drawHorizontalLine(juce::roundToInt(headingArea.getBottom()),
                             headingArea.getX() + 4.0f, headingArea.getRight() - 4.0f);
        g.setColour(juce::Colour(xpInk));
        g.setFont(juce::Font(15.0f, juce::Font::bold));
        g.drawText(heading, headingArea.reduced(11.0f, 0.0f).toNearestInt(),
                   juce::Justification::centredLeft, false);

        const auto hatch = headingArea.withTrimmedLeft(headingArea.getWidth() * 0.62f)
                                      .reduced(7.0f, 6.0f);
        g.setColour(juce::Colour(0xff4b87bd).withAlpha(0.35f));
        for (float x = hatch.getX(); x < hatch.getRight(); x += 5.0f)
            g.drawLine(x, hatch.getBottom(), x + 8.0f, hatch.getY(), 1.0f);
    }
}
}

XpLookAndFeel::XpLookAndFeel()
{
    setColour(juce::Label::textColourId, juce::Colour(xpInk));
    setColour(juce::TextButton::buttonColourId, juce::Colour(0xffdceaf8));
    setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff75baff));
    setColour(juce::TextButton::textColourOffId, juce::Colour(xpInk));
    setColour(juce::TextButton::textColourOnId, juce::Colour(0xff071d51));
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xfff8fbff));
    setColour(juce::ComboBox::textColourId, juce::Colour(xpInk));
    setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff6a87a7));
    setColour(juce::ComboBox::arrowColourId, juce::Colour(xpInk));
    setColour(juce::Slider::textBoxTextColourId, juce::Colour(xpInk));
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xfff8fbff));
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff7693b3));
    setColour(juce::Slider::trackColourId, juce::Colour(0xff1686e4));
    setColour(juce::Slider::thumbColourId, juce::Colour(0xffdcecf9));
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xfff7fbff));
    setColour(juce::PopupMenu::textColourId, juce::Colour(xpInk));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff2f91ee));
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
}

void XpLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                         const juce::Colour&, bool over, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    const auto top = down ? juce::Colour(0xffb4cee8)
                          : (over ? juce::Colour(0xffffffff) : juce::Colour(0xfff6f9fd));
    const auto bottom = down ? juce::Colour(0xffedf5fc)
                             : (over ? juce::Colour(0xffc9e5ff) : juce::Colour(0xffcdddec));
    g.setGradientFill(juce::ColourGradient(top, bounds.getCentreX(), bounds.getY(), bottom,
                                           bounds.getCentreX(), bounds.getBottom(), false));
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(juce::Colours::white.withAlpha(down ? 0.45f : 0.9f));
    g.drawRoundedRectangle(bounds.reduced(1.0f), 3.0f, 1.0f);
    g.setColour(juce::Colour(down ? 0xff315d8e : 0xff718dab));
    g.drawRoundedRectangle(bounds, 4.0f, 1.2f);
}

void XpLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                   bool, bool)
{
    g.setColour(button.findColour(button.getToggleState()
        ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId));
    g.setFont(juce::Font(12.0f, juce::Font::bold));
    g.drawFittedText(button.getButtonText().toUpperCase(), button.getLocalBounds().reduced(5, 1),
                     juce::Justification::centred, 1);
}

void XpLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                     float position, float startAngle, float endAngle,
                                     juce::Slider& slider)
{
    auto size = static_cast<float>(juce::jmin(width, height)) - 8.0f;
    auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                         static_cast<float>(width), static_cast<float>(height))
                      .withSizeKeepingCentre(size, size);
    const auto centre = bounds.getCentre();
    const auto radius = bounds.getWidth() * 0.5f;
    const auto angle = startAngle + position * (endAngle - startAngle);
    const auto accent = slider.findColour(juce::Slider::rotarySliderFillColourId);

    g.setColour(juce::Colour(0xff51657d).withAlpha(0.35f));
    g.fillEllipse(bounds.translated(1.5f, 2.5f));
    juce::Path track;
    track.addCentredArc(centre.x, centre.y, radius - 2.5f, radius - 2.5f, 0.0f,
                        startAngle, endAngle, true);
    g.setColour(juce::Colour(0xff244a78));
    g.strokePath(track, juce::PathStrokeType(6.5f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));
    juce::Path valueArc;
    valueArc.addCentredArc(centre.x, centre.y, radius - 2.5f, radius - 2.5f, 0.0f,
                           startAngle, angle, true);
    g.setColour(accent);
    g.strokePath(valueArc, juce::PathStrokeType(6.5f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));

    auto face = bounds.reduced(8.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colours::white, face.getX(), face.getY(),
                                           juce::Colour(0xff9dacbd), face.getRight(),
                                           face.getBottom(), false));
    g.fillEllipse(face);
    g.setColour(juce::Colour(0xffedf5fc));
    g.drawEllipse(face.reduced(1.0f), 1.6f);
    g.setColour(juce::Colour(0xff33475f));
    g.drawEllipse(face, 1.2f);
    const auto pointerStart = centre.getPointOnCircumference(radius * 0.20f, angle);
    const auto pointerEnd = centre.getPointOnCircumference(radius * 0.62f, angle);
    g.setColour(juce::Colour(0xff172231));
    g.drawLine({ pointerStart, pointerEnd }, 2.4f);
    g.fillEllipse(centre.x - 2.3f, centre.y - 2.3f, 4.6f, 4.6f);
}

void XpLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                     float sliderPos, float, float,
                                     juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearHorizontal && style != juce::Slider::LinearBar
        && style != juce::Slider::TwoValueHorizontal
        && style != juce::Slider::ThreeValueHorizontal)
    {
        LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, sliderPos,
                                         sliderPos, style, slider);
        return;
    }
    const auto cy = static_cast<float>(y + height / 2);
    const auto left = static_cast<float>(x + 5);
    const auto right = static_cast<float>(x + width - 5);
    g.setColour(juce::Colour(0xff8297ad));
    g.fillRoundedRectangle(left, cy - 3.0f, right - left, 6.0f, 3.0f);
    g.setColour(slider.findColour(juce::Slider::trackColourId));
    g.fillRoundedRectangle(left, cy - 2.0f, juce::jmax(0.0f, sliderPos - left), 4.0f, 2.0f);
    auto thumb = juce::Rectangle<float>(sliderPos - 5.0f, cy - 10.0f, 10.0f, 20.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colours::white, thumb.getX(), thumb.getY(),
                                           juce::Colour(0xffb6c8db), thumb.getX(),
                                           thumb.getBottom(), false));
    g.fillRoundedRectangle(thumb, 2.0f);
    g.setColour(juce::Colour(0xff526b85));
    g.drawRoundedRectangle(thumb, 2.0f, 1.0f);
}

void XpLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool down,
                                 int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width),
                                         static_cast<float>(height)).reduced(0.5f);
    g.setGradientFill(juce::ColourGradient(juce::Colours::white, 0.0f, 0.0f,
                                           juce::Colour(0xffdbe8f4), 0.0f,
                                           static_cast<float>(height), false));
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(box.findColour(juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle(bounds, 4.0f, 1.1f);
    auto arrowArea = bounds.removeFromRight(static_cast<float>(height));
    g.setColour(juce::Colour(down ? 0xffb4d8f8 : 0xffc9e4fb));
    g.fillRoundedRectangle(arrowArea.reduced(2.0f), 3.0f);
    juce::Path arrow;
    const auto centre = arrowArea.getCentre();
    arrow.startNewSubPath(centre.x - 5.0f, centre.y - 2.0f);
    arrow.lineTo(centre.x + 5.0f, centre.y - 2.0f);
    arrow.lineTo(centre.x, centre.y + 4.0f);
    arrow.closeSubPath();
    g.setColour(juce::Colour(xpInk));
    g.fillPath(arrow);
}

void XpLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(8, 1, box.getWidth() - box.getHeight() - 7, box.getHeight() - 2);
    label.setFont(juce::Font(14.0f));
}

void XpLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                     bool over, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
    const auto on = button.getToggleState();
    const auto top = on ? juce::Colour(over ? 0xff82c5ff : 0xff55aaff)
                        : juce::Colour(over ? 0xffeff7ff : 0xffdce8f4);
    const auto bottom = on ? juce::Colour(down ? 0xff0a61c5 : 0xff1680e7)
                           : juce::Colour(down ? 0xffb4c7da : 0xffb8c9da);
    g.setGradientFill(juce::ColourGradient(top, bounds.getCentreX(), bounds.getY(), bottom,
                                           bounds.getCentreX(), bounds.getBottom(), false));
    g.fillRoundedRectangle(bounds, bounds.getHeight() * 0.48f);
    g.setColour(juce::Colour(on ? 0xff0750ad : 0xff6f879f));
    g.drawRoundedRectangle(bounds, bounds.getHeight() * 0.48f, 1.2f);
    g.setColour(on ? juce::Colours::white : juce::Colour(xpInk));
    g.setFont(juce::Font(12.0f, juce::Font::bold));
    g.drawFittedText(button.getButtonText().toUpperCase(), button.getLocalBounds().reduced(8, 1),
                     juce::Justification::centred, 1);
}

void SourceWaveformComponent::setSource(SampleManager::SamplePtr newSource)
{
    source = std::move(newSource);
    region = source != nullptr
        ? randomchop::clampNormalisedRegion(source->settings.startNormalised,
                                            source->settings.endNormalised)
        : randomchop::NormalisedRegion {};
    repaint();
}

juce::Rectangle<int> SourceWaveformComponent::getWaveformBounds() const
{
    return getLocalBounds().reduced(5).withTrimmedTop(25).withTrimmedBottom(5);
}

double SourceWaveformComponent::positionToNormalised(float x) const noexcept
{
    const auto bounds = getWaveformBounds();
    if (bounds.getWidth() <= 0)
        return 0.0;
    return juce::jlimit(0.0, 1.0,
        static_cast<double>(x - static_cast<float>(bounds.getX()))
            / static_cast<double>(bounds.getWidth()));
}

juce::String SourceWaveformComponent::markerDescription(const juce::String& name,
                                                          double position) const
{
    if (source == nullptr || source->audio == nullptr)
        return name;

    const auto lastFrame = juce::jmax(0, source->audio->getNumSamples() - 1);
    const auto frame = juce::jlimit(0, lastFrame,
        static_cast<int>(std::llround(position * static_cast<double>(lastFrame))));
    const auto seconds = static_cast<double>(frame) / juce::jmax(1.0, source->sampleRate);
    return name + "  " + juce::String(position * 100.0, 1) + "%  |  "
        + juce::String(seconds, 3) + " s  |  sample " + juce::String(frame);
}

void SourceWaveformComponent::paint(juce::Graphics& g)
{
    const auto outer = getLocalBounds().toFloat();
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xfff8fbff), outer.getX(), outer.getY(),
                                           juce::Colour(0xffbed4e8), outer.getX(),
                                           outer.getBottom(), false));
    g.fillRoundedRectangle(outer, 7.0f);
    g.setColour(juce::Colour(0xff59799c));
    g.drawRoundedRectangle(outer.reduced(0.5f), 7.0f, 1.4f);

    g.setFont(juce::Font(13.0f, juce::Font::bold));
    g.setColour(juce::Colour(xpInk));
    juce::String heading("SELECT A SOURCE");
    if (source != nullptr)
        heading = source->settings.displayName;
    auto headingBounds = getLocalBounds().reduced(9).removeFromTop(18);
    g.drawText(heading, headingBounds,
               juce::Justification::centredLeft, true);
    if (source != nullptr && source->audio != nullptr)
    {
        const auto seconds = static_cast<double>(source->audio->getNumSamples())
            / juce::jmax(1.0, source->sampleRate);
        const auto details = juce::String(source->sampleRate / 1000.0, 1) + " kHz   "
            + juce::String(seconds, 2) + " s";
        g.setFont(11.0f);
        g.setColour(juce::Colour(0xff456d9c));
        g.drawText(details, headingBounds, juce::Justification::centredRight, true);
    }

    const auto waveBounds = getWaveformBounds();
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff061b39),
                                           static_cast<float>(waveBounds.getX()),
                                           static_cast<float>(waveBounds.getY()),
                                           juce::Colour(0xff0b3159),
                                           static_cast<float>(waveBounds.getX()),
                                           static_cast<float>(waveBounds.getBottom()), false));
    g.fillRoundedRectangle(waveBounds.toFloat(), 4.0f);
    g.setColour(juce::Colour(0xff2c6696).withAlpha(0.45f));
    for (int line = 1; line < 12; ++line)
    {
        const auto x = waveBounds.getX() + line * waveBounds.getWidth() / 12;
        g.drawVerticalLine(x, static_cast<float>(waveBounds.getY()),
                           static_cast<float>(waveBounds.getBottom()));
    }
    for (int line = 1; line < 4; ++line)
    {
        const auto y = waveBounds.getY() + line * waveBounds.getHeight() / 4;
        g.drawHorizontalLine(y, static_cast<float>(waveBounds.getX()),
                             static_cast<float>(waveBounds.getRight()));
    }
    g.setColour(juce::Colour(0xff78aee0).withAlpha(0.55f));
    g.drawHorizontalLine(waveBounds.getCentreY(), static_cast<float>(waveBounds.getX()),
                         static_cast<float>(waveBounds.getRight()));

    if (source == nullptr)
    {
        g.setColour(juce::Colour(0xff9fc5eb));
        g.drawText("Select a source to edit its region", waveBounds,
                   juce::Justification::centred);
        return;
    }

    if (source->settings.missing || source->audio == nullptr
        || source->waveformPeaks == nullptr || source->waveformPeaks->empty())
    {
        g.setColour(juce::Colour(0xffffa0a0));
        g.drawText("Waveform unavailable for missing source", waveBounds,
                   juce::Justification::centred);
    }
    else
    {
        const auto& peaks = *source->waveformPeaks;
        const auto peakCount = peaks.size();
        const auto halfHeight = static_cast<float>(waveBounds.getHeight()) * 0.46f;
        const auto centreY = static_cast<float>(waveBounds.getCentreY());
        g.setColour(juce::Colour(0xff86c2ff));
        for (int x = 0; x < waveBounds.getWidth(); ++x)
        {
            const auto first = static_cast<size_t>(x) * peakCount
                / static_cast<size_t>(waveBounds.getWidth());
            const auto last = juce::jmax(first + 1,
                static_cast<size_t>(x + 1) * peakCount
                    / static_cast<size_t>(waveBounds.getWidth()));
            float minimum = 0.0f;
            float maximum = 0.0f;
            for (auto peak = first; peak < juce::jmin(last, peakCount); ++peak)
            {
                minimum = juce::jmin(minimum, peaks[peak].minimum);
                maximum = juce::jmax(maximum, peaks[peak].maximum);
            }
            g.drawVerticalLine(waveBounds.getX() + x,
                               centreY - maximum * halfHeight,
                               centreY - minimum * halfHeight);
        }
    }

    const auto startX = static_cast<float>(waveBounds.getX())
        + static_cast<float>(region.start) * static_cast<float>(waveBounds.getWidth());
    const auto endX = static_cast<float>(waveBounds.getX())
        + static_cast<float>(region.end) * static_cast<float>(waveBounds.getWidth());
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.fillRect(juce::Rectangle<float>(static_cast<float>(waveBounds.getX()),
                                     static_cast<float>(waveBounds.getY()),
                                     juce::jmax(0.0f, startX - waveBounds.getX()),
                                     static_cast<float>(waveBounds.getHeight())));
    g.fillRect(juce::Rectangle<float>(endX, static_cast<float>(waveBounds.getY()),
                                     juce::jmax(0.0f, waveBounds.getRight() - endX),
                                     static_cast<float>(waveBounds.getHeight())));

    g.setColour(juce::Colour(0xff66e3a4));
    g.drawLine(startX, static_cast<float>(waveBounds.getY()), startX,
               static_cast<float>(waveBounds.getBottom()), 2.0f);
    g.setColour(juce::Colour(0xffffa65c));
    g.drawLine(endX, static_cast<float>(waveBounds.getY()), endX,
               static_cast<float>(waveBounds.getBottom()), 2.0f);

    const auto drawTag = [&g, &waveBounds](float markerX, const juce::String& text,
                                           juce::Colour colour)
    {
        constexpr float tagWidth = 48.0f;
        const auto x = juce::jlimit(static_cast<float>(waveBounds.getX()),
                                    static_cast<float>(waveBounds.getRight()) - tagWidth,
                                    markerX - tagWidth * 0.5f);
        auto tag = juce::Rectangle<float>(x, static_cast<float>(waveBounds.getY()) + 5.0f,
                                          tagWidth, 19.0f);
        g.setColour(colour);
        g.fillRoundedRectangle(tag, 3.0f);
        g.setColour(juce::Colour(0xff10213c));
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        g.drawText(text, tag.toNearestInt(), juce::Justification::centred, false);
    };
    drawTag(startX, "START", juce::Colour(0xffa9e95e));
    drawTag(endX, "END", juce::Colour(0xffffa23d));
    g.setColour(juce::Colour(0xff82a7c9));
    g.drawRoundedRectangle(waveBounds.toFloat(), 4.0f, 1.1f);
}

void SourceWaveformComponent::mouseDown(const juce::MouseEvent& event)
{
    if (source == nullptr || source->audio == nullptr)
        return;

    const auto bounds = getWaveformBounds();
    const auto startX = static_cast<float>(bounds.getX())
        + static_cast<float>(region.start) * static_cast<float>(bounds.getWidth());
    const auto endX = static_cast<float>(bounds.getX())
        + static_cast<float>(region.end) * static_cast<float>(bounds.getWidth());
    if (std::abs(startX - endX) < 0.5f)
    {
        const auto position = positionToNormalised(event.position.x);
        dragMarker = position < region.start ? DragMarker::start
            : (position > region.end ? DragMarker::end : DragMarker::coincident);
    }
    else
    {
        dragMarker = std::abs(event.position.x - startX) <= std::abs(event.position.x - endX)
            ? DragMarker::start : DragMarker::end;
    }

    if (dragMarker != DragMarker::coincident)
        mouseDrag(event);
}

void SourceWaveformComponent::mouseDrag(const juce::MouseEvent& event)
{
    if (dragMarker == DragMarker::none || source == nullptr)
        return;

    const auto position = positionToNormalised(event.position.x);
    if (dragMarker == DragMarker::coincident)
    {
        if (position < region.start)
            dragMarker = DragMarker::start;
        else if (position > region.end)
            dragMarker = DragMarker::end;
        else
            return;
    }

    if (dragMarker == DragMarker::start)
        region.start = juce::jmin(position, region.end);
    else
        region.end = juce::jmax(position, region.start);

    if (onRegionChanged)
        onRegionChanged(region.start, region.end);
    repaint();
}

void SourceWaveformComponent::mouseUp(const juce::MouseEvent&)
{
    dragMarker = DragMarker::none;
}

void SpectralCanvasComponent::setCanvas(const Canvas& newCanvas)
{
    canvas = newCanvas;
    repaint();
}

void SpectralCanvasComponent::clearCanvas()
{
    canvas.fill(0.0f);
    if (onCanvasChanged)
        onCanvasChanged(canvas);
    repaint();
}

void SpectralCanvasComponent::setScanPosition(float position)
{
    const auto next = std::clamp(std::isfinite(position) ? position : 0.0f, 0.0f, 1.0f);
    if (std::abs(next - scanPosition) > 0.0001f)
    {
        scanPosition = next;
        repaint();
    }
}

juce::Point<int> SpectralCanvasComponent::eventToCell(
    const juce::MouseEvent& event) const noexcept
{
    const auto bounds = getLocalBounds().reduced(2);
    if (bounds.isEmpty())
        return {};
    const auto x = juce::jlimit(0, randomchop::SpectralMaskStore::canvasWidth - 1,
        static_cast<int>((event.position.x - static_cast<float>(bounds.getX()))
            * randomchop::SpectralMaskStore::canvasWidth
            / static_cast<float>(bounds.getWidth())));
    const auto y = juce::jlimit(0, randomchop::SpectralMaskStore::canvasHeight - 1,
        static_cast<int>((event.position.y - static_cast<float>(bounds.getY()))
            * randomchop::SpectralMaskStore::canvasHeight
            / static_cast<float>(bounds.getHeight())));
    return { x, y };
}

void SpectralCanvasComponent::applyBrush(juce::Point<int> cell) noexcept
{
    for (int row = std::max(0, cell.y - 1);
         row <= std::min(randomchop::SpectralMaskStore::canvasHeight - 1, cell.y + 1); ++row)
        for (int column = std::max(0, cell.x - 1);
             column <= std::min(randomchop::SpectralMaskStore::canvasWidth - 1, cell.x + 1);
             ++column)
            canvas[static_cast<std::size_t>(
                row * randomchop::SpectralMaskStore::canvasWidth + column)]
                = 1.0f;
}

void SpectralCanvasComponent::applyLine(juce::Point<int> from, juce::Point<int> to)
{
    const auto steps = std::max(std::abs(to.x - from.x), std::abs(to.y - from.y));
    for (int step = 0; step <= steps; ++step)
    {
        const auto amount = steps > 0
            ? static_cast<float>(step) / static_cast<float>(steps) : 0.0f;
        applyBrush({ juce::roundToInt(static_cast<float>(from.x)
                                      + static_cast<float>(to.x - from.x) * amount),
                     juce::roundToInt(static_cast<float>(from.y)
                                      + static_cast<float>(to.y - from.y) * amount) });
    }
    if (onCanvasChanged)
        onCanvasChanged(canvas);
    repaint();
}

void SpectralCanvasComponent::mouseDown(const juce::MouseEvent& event)
{
    lastCell = eventToCell(event);
    applyLine(lastCell, lastCell);
}

void SpectralCanvasComponent::mouseDrag(const juce::MouseEvent& event)
{
    const auto next = eventToCell(event);
    applyLine(lastCell, next);
    lastCell = next;
}

void SpectralCanvasComponent::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().reduced(2);
    g.fillAll(juce::Colour(0xffbdd1e5));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff061b3c),
                                           static_cast<float>(bounds.getX()),
                                           static_cast<float>(bounds.getY()),
                                           juce::Colour(0xff102f58),
                                           static_cast<float>(bounds.getX()),
                                           static_cast<float>(bounds.getBottom()), false));
    g.fillRoundedRectangle(bounds.toFloat(), 4.0f);
    g.setColour(juce::Colour(0xff2e6192).withAlpha(0.5f));
    for (int line = 1; line < 8; ++line)
    {
        const auto x = bounds.getX() + line * bounds.getWidth() / 8;
        g.drawVerticalLine(x, static_cast<float>(bounds.getY()),
                           static_cast<float>(bounds.getBottom()));
    }
    for (int line = 1; line < 6; ++line)
    {
        const auto y = bounds.getY() + line * bounds.getHeight() / 6;
        g.drawHorizontalLine(y, static_cast<float>(bounds.getX()),
                             static_cast<float>(bounds.getRight()));
    }
    const auto cellWidth = static_cast<float>(bounds.getWidth())
        / randomchop::SpectralMaskStore::canvasWidth;
    const auto cellHeight = static_cast<float>(bounds.getHeight())
        / randomchop::SpectralMaskStore::canvasHeight;
    for (int row = 0; row < randomchop::SpectralMaskStore::canvasHeight; ++row)
        for (int column = 0; column < randomchop::SpectralMaskStore::canvasWidth; ++column)
        {
            const auto value = canvas[static_cast<std::size_t>(
                row * randomchop::SpectralMaskStore::canvasWidth + column)];
            if (value > 0.0001f)
            {
                g.setColour(juce::Colour(0xffa765ff).withAlpha(0.25f + 0.65f * value));
                g.fillRect(static_cast<float>(bounds.getX()) + column * cellWidth,
                           static_cast<float>(bounds.getY()) + row * cellHeight,
                           cellWidth + 0.5f, cellHeight + 0.5f);
            }
        }
    const auto scannerX = static_cast<float>(bounds.getX())
        + scanPosition * static_cast<float>(bounds.getWidth());
    g.setColour(juce::Colour(0xffffdd45).withAlpha(0.18f));
    g.fillRect(scannerX - 5.0f, static_cast<float>(bounds.getY()), 10.0f,
               static_cast<float>(bounds.getHeight()));
    g.setColour(juce::Colour(0xffffed73));
    g.drawVerticalLine(juce::roundToInt(scannerX), static_cast<float>(bounds.getY()),
                       static_cast<float>(bounds.getBottom()));
    g.setColour(juce::Colour(0xff6f95ba));
    g.drawRoundedRectangle(bounds.toFloat(), 4.0f, 1.0f);
}

void CreativeVisualizer::setState(float primaryPercent) noexcept
{
    const auto nextPrimary = std::clamp(std::isfinite(primaryPercent)
        ? primaryPercent * 0.01f : 0.0f, 0.0f, 1.0f);
    if (std::abs(nextPrimary - primary) > 0.0001f)
    {
        primary = nextPrimary;
        repaint();
    }
}

void CreativeVisualizer::advance() noexcept
{
    phase = std::fmod(phase + 0.012f + 0.030f * primary, 1.0f);
    if (primary > 0.0001f)
        repaint();
}

void CreativeVisualizer::setTelemetry(float first, float second,
                                      uint32_t flags) noexcept
{
    const auto nextFirst = std::clamp(std::isfinite(first) ? first : 0.0f,
                                      0.0f, 1.0f);
    const auto nextSecond = std::clamp(std::isfinite(second) ? second : 0.0f,
                                       0.0f, 1.0f);
    if (std::abs(nextFirst - telemetryFirst) > 0.0001f
        || std::abs(nextSecond - telemetrySecond) > 0.0001f
        || flags != telemetryFlags)
    {
        telemetryFirst = nextFirst;
        telemetrySecond = nextSecond;
        telemetryFlags = flags;
        repaint();
    }
}

void CreativeVisualizer::paint(juce::Graphics& g)
{
    const auto outer = getLocalBounds().toFloat();
    const auto inner = outer.reduced(7.0f, 8.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff061a37), outer.getX(), outer.getY(),
                                           juce::Colour(0xff0b3159), outer.getX(),
                                           outer.getBottom(), false));
    g.fillRoundedRectangle(outer, 7.0f);
    g.setColour(juce::Colour(0xff668eb6));
    g.drawRoundedRectangle(outer.reduced(0.5f), 7.0f, 1.2f);

    if (kind == Kind::scramble)
    {
        constexpr int slices = 8;
        const auto gap = 2.0f;
        const auto cellWidth = (inner.getWidth() - gap * (slices - 1)) / slices;
        for (int index = 0; index < slices; ++index)
        {
            const auto isRunning = (telemetryFlags & (uint32_t { 1 } << 8)) != 0;
            const auto activeSlice = isRunning
                ? (telemetryFlags & (uint32_t { 1 }
                    << static_cast<uint32_t>(index))) != 0
                : false;
            const auto displacement = activeSlice
                ? std::sin((phase * 2.0f + index * 0.31f)
                           * juce::MathConstants<float>::twoPi)
                    * primary * inner.getHeight() * 0.16f
                : 0.0f;
            auto cell = juce::Rectangle<float>(
                inner.getX() + index * (cellWidth + gap), inner.getY() + displacement,
                cellWidth, inner.getHeight() - std::abs(displacement));
            g.setColour(activeSlice ? juce::Colour(0xff7fc2ff).withAlpha(0.42f + 0.48f * primary)
                                    : juce::Colour(0xff183c63));
            g.fillRoundedRectangle(cell, 2.0f);
        }
        const auto scanner = inner.getX() + telemetryFirst * inner.getWidth();
        const auto armed = (telemetryFlags & (uint32_t { 1 } << 9)) != 0;
        g.setColour(juce::Colour(0xffd8efff).withAlpha(armed ? 0.65f : 1.0f));
        g.drawLine(scanner, inner.getY() - 2.0f, scanner, inner.getBottom() + 2.0f, 1.5f);
        return;
    }

    if (kind == Kind::melt)
    {
        constexpr int slices = 4;
        const auto active = (telemetryFlags & (uint32_t { 1 } << 8)) != 0;
        const auto scanner = inner.getX() + telemetrySecond * inner.getWidth();
        const auto stretch = 0.10f + 0.90f * telemetryFirst;
        const auto gap = 3.0f;
        const auto cellWidth = (inner.getWidth() - gap * (slices - 1)) / slices;
        for (int slice = 0; slice < slices; ++slice)
        {
            const auto reversed = (telemetryFlags & (uint32_t { 1 }
                << static_cast<uint32_t>(slice))) != 0;
            auto cell = juce::Rectangle<float>(
                inner.getX() + slice * (cellWidth + gap), inner.getY(),
                cellWidth, inner.getHeight());
            g.setColour(juce::Colour(reversed ? 0xffff8fab : 0xff67e8f9)
                .withAlpha(active ? 0.22f + 0.38f * primary : 0.12f));
            g.fillRoundedRectangle(cell, 3.0f);

            juce::Path ribbon;
            constexpr int points = 20;
            for (int point = 0; point <= points; ++point)
            {
                const auto unit = static_cast<float>(point) / points;
                const auto direction = reversed ? 1.0f - unit : unit;
                const auto x = cell.getX() + unit * cell.getWidth();
                const auto wave = std::sin((direction * (1.0f + 2.2f * stretch)
                    + phase * 0.35f) * juce::MathConstants<float>::twoPi);
                const auto y = cell.getCentreY()
                    + wave * cell.getHeight() * (0.08f + 0.22f * stretch);
                if (point == 0) ribbon.startNewSubPath(x, y);
                else ribbon.lineTo(x, y);
            }
            g.setColour(juce::Colour(reversed ? 0xffff8fab : 0xff67e8f9)
                .withAlpha(active ? 0.90f : 0.36f));
            g.strokePath(ribbon, juce::PathStrokeType(1.7f));
        }
        g.setColour(juce::Colour(0xffffcf5a).withAlpha(active ? 1.0f : 0.45f));
        g.drawLine(scanner, inner.getY() - 2.0f, scanner,
                   inner.getBottom() + 2.0f, 1.5f);
        return;
    }

    constexpr int maximumVisualGrains = randomchop::SmearProcessor::maximumGrains;
    const auto visibleGrains = std::clamp(static_cast<int>(std::ceil(
        telemetryFirst * static_cast<float>(maximumVisualGrains))),
        0, maximumVisualGrains);
    for (int index = 0; index < visibleGrains; ++index)
    {
        const auto orbit = std::fmod(phase * (0.55f + index * 0.07f)
                                     + index * 0.173f, 1.0f);
        const auto x = inner.getX() + orbit * inner.getWidth();
        const auto y = inner.getCentreY() + std::sin((orbit + index * 0.21f)
            * juce::MathConstants<float>::twoPi) * inner.getHeight() * 0.30f * primary;
        const auto lengthVariation = 0.72f + 0.28f
            * std::fmod(static_cast<float>(index) * 0.618f, 1.0f);
        const auto length = (10.0f - 6.5f * primary) * lengthVariation
            * (0.72f + 0.28f * telemetrySecond);
        g.setColour(juce::Colour(0xff67e8f9).withAlpha(0.18f + 0.62f * primary));
        g.drawLine(x - length, y + length * 0.35f, x + length, y - length * 0.35f,
                   1.0f + 1.2f * primary);
        g.setColour(juce::Colour(0xfff0f9ff).withAlpha(0.28f + 0.66f * primary));
        const auto radius = 1.8f - 0.7f * primary;
        g.fillEllipse(x - radius, y - radius, radius * 2.0f, radius * 2.0f);
    }
}

void XpInfoButton::paintButton(juce::Graphics& g, bool isMouseOverButton,
                               bool isButtonDown)
{
    auto bounds = getLocalBounds().toFloat().reduced(2.0f);
    const auto top = isButtonDown ? juce::Colour(0xff0b62c7)
                                  : (isMouseOverButton ? juce::Colour(0xff69b9ff)
                                                       : juce::Colour(0xff3b98ee));
    const auto bottom = isButtonDown ? juce::Colour(0xff43a8ff)
                                     : juce::Colour(0xff0754b5);
    g.setGradientFill(juce::ColourGradient(top, bounds.getCentreX(), bounds.getY(),
                                           bottom, bounds.getCentreX(), bounds.getBottom(),
                                           false));
    g.fillEllipse(bounds);
    g.setColour(juce::Colours::white.withAlpha(0.95f));
    g.drawEllipse(bounds.reduced(0.75f), 1.5f);
    g.setFont(juce::Font(17.0f, juce::Font::bold));
    g.drawText("i", bounds.toNearestInt().translated(0, -1),
               juce::Justification::centred, false);
}

XpInfoPanel::XpInfoPanel()
{
    setOpaque(true);
    addAndMakeVisible(closeButton);
    closeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffe8eef8));
    closeButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffd5e8ff));
    closeButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff15295a));
    closeButton.onClick = [this]
    {
        if (onClose)
            onClose();
    };
}

void XpInfoPanel::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.fillAll(juce::Colour(0xffece9d8));
    g.setColour(juce::Colour(0xff003c9d));
    g.drawRect(bounds, 3.0f);

    auto titleBar = bounds.reduced(3.0f).removeFromTop(31.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff0a70e8), titleBar.getX(),
                                           titleBar.getY(), juce::Colour(0xff0751bb),
                                           titleBar.getRight(), titleBar.getY(), false));
    g.fillRect(titleBar);
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(15.0f, juce::Font::bold));
    g.drawText("Information", titleBar.reduced(10.0f, 0.0f).toNearestInt(),
               juce::Justification::centredLeft, false);

    auto icon = juce::Rectangle<float>(25.0f, 58.0f, 42.0f, 42.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff70c4ff), icon.getCentreX(),
                                           icon.getY(), juce::Colour(0xff0756bc),
                                           icon.getCentreX(), icon.getBottom(), false));
    g.fillEllipse(icon);
    g.setColour(juce::Colours::white);
    g.drawEllipse(icon.reduced(1.0f), 2.0f);
    g.setFont(juce::Font(27.0f, juce::Font::bold));
    g.drawText("i", icon.toNearestInt().translated(0, -2),
               juce::Justification::centred, false);

    g.setColour(juce::Colour(0xff111111));
    g.setFont(15.0f);
    g.drawText("test", 84, 61, getWidth() - 104, 36,
               juce::Justification::centredLeft, false);
}

void XpInfoPanel::resized()
{
    closeButton.setBounds(getWidth() - 104, getHeight() - 44, 84, 25);
}

namespace
{
class SourceRowControls final : public juce::Component
{
public:
    SourceRowControls()
    {
        setInterceptsMouseClicks(true, true);
        addAndMakeVisible(enabled);
        addAndMakeVisible(remove);
        remove.setButtonText("Remove");
    }
    void mouseDown(const juce::MouseEvent&) override
    {
        if (onSelect)
            onSelect(row);
    }
    void resized() override
    {
        auto area = getLocalBounds();
        remove.setBounds(area.removeFromRight(68).reduced(2));
        enabled.setBounds(area.removeFromRight(70).reduced(2));
    }
    juce::ToggleButton enabled { "On" };
    juce::TextButton remove;
    std::function<void(int)> onSelect;
    int row = -1;
};
}

RandomChopSamplerAudioProcessorEditor::RandomChopSamplerAudioProcessorEditor(RandomChopSamplerAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&xpLookAndFeel);
    setOpaque(true);
    setResizable(true, true);
    setResizeLimits(760, 520, 1180, 820);
    setSize(880, 600);
    title.setText("recompiler.dll", juce::dontSendNotification);
    title.setFont(juce::Font(22.0f, juce::Font::bold));
    title.setColour(juce::Label::textColourId, juce::Colours::white);
    subtitle.setText("|   random sample instrument", juce::dontSendNotification);
    subtitle.setFont(juce::Font(13.0f));
    subtitle.setColour(juce::Label::textColourId, juce::Colour(0xffd9edff));
    status.setFont(juce::Font(12.0f, juce::Font::bold));
    status.setColour(juce::Label::textColourId, juce::Colour(0xffd9edff));
    status.setJustificationType(juce::Justification::centredRight);

    juce::Component* components[] = { &title, &subtitle, &status, &infoButton, &list, &waveform, &sourceKey,
        &sourceTranspose, &sourceFineTune, &sourceGain,
        &sourceKeyLabel,
        &sourceTransposeLabel, &sourceFineTuneLabel, &sourceGainLabel,
        &targetKey, &targetKeyLabel, &midiPitch,
        &voiceMode, &voiceModeLabel,
        &spectralDrawLabel, &spectralResetButton, &spectralCanvas,
        &scrambleVisual, &meltVisual, &smearVisual };
    for (auto* component : components) addAndMakeVisible(component);
    addChildComponent(infoPanel);
    infoButton.onClick = [this]
    {
        infoPanel.setVisible(true);
        infoPanel.toFront(true);
    };
    infoPanel.onClose = [this] { infoPanel.setVisible(false); };
    list.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xfff5f8fc));
    list.setColour(juce::ListBox::outlineColourId, juce::Colour(0xff7896b5));
    list.setOutlineThickness(1);
    list.setRowHeight(25);
    list.setTooltip("Drop WAV, AIFF, MP3, or FLAC files here");

    for (int index = 0; index < static_cast<int>(randomchop::tonicNames.size()); ++index)
    {
        sourceKey.addItem(randomchop::tonicNames[static_cast<size_t>(index)], index + 1);
        targetKey.addItem(randomchop::tonicNames[static_cast<size_t>(index)], index + 1);
    }
    sourceTranspose.setRange(-24.0, 24.0, 1.0);
    sourceTranspose.setTextValueSuffix(" st");
    sourceFineTune.setRange(-100.0, 100.0, 1.0);
    sourceFineTune.setTextValueSuffix(" cents");
    sourceGain.setRange(-60.0, 12.0, 0.1);
    sourceGain.setTextValueSuffix(" dB");
    for (auto* slider : { &sourceTranspose, &sourceFineTune, &sourceGain })
    {
        slider->setSliderStyle(juce::Slider::LinearHorizontal);
        slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 84, 22);
    }
    sourceGain.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    sourceGain.setTextBoxStyle(juce::Slider::TextBoxRight, false, 58, 22);
    sourceGain.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff1686e4));
    sourceKeyLabel.setText("SOURCE KEY", juce::dontSendNotification);
    sourceTransposeLabel.setText("TRANSPOSE", juce::dontSendNotification);
    sourceFineTuneLabel.setText("FINE TUNE", juce::dontSendNotification);
    sourceGainLabel.setText("GAIN", juce::dontSendNotification);
    targetKeyLabel.setText("PLAY IN KEY", juce::dontSendNotification);
    voiceModeLabel.setText("VOICES", juce::dontSendNotification);
    spectralDrawLabel.setText("SPECTRAL DRAW", juce::dontSendNotification);
    midiPitch.setTooltip("Off keeps every trigger in Play In Key; on follows MIDI notes for chords");
    voiceMode.setTooltip("POLY overlaps held notes; MONO cuts the previous voice");
    voiceModeLabel.setJustificationType(juce::Justification::centredLeft);
    for (auto* label : { &sourceKeyLabel, &sourceTransposeLabel, &sourceFineTuneLabel,
                         &sourceGainLabel, &targetKeyLabel, &voiceModeLabel,
                         &spectralDrawLabel })
        label->setColour(juce::Label::textColourId, juce::Colour(xpInk));
    spectralResetButton.onClick = [this] { spectralCanvas.clearCanvas(); };
    spectralCanvas.setCanvas(processor.getSpectralCanvas());
    lastSpectralCanvasGeneration = processor.getSpectralCanvasGeneration();
    spectralCanvas.onCanvasChanged = [this](const SpectralCanvasComponent::Canvas& canvas)
    {
        processor.setSpectralCanvas(canvas);
        lastSpectralCanvasGeneration = processor.getSpectralCanvasGeneration();
    };
    sourceKey.onChange = [this]
    {
        if (selectedSourceId.isNotEmpty()) processor.samples.updateSettings(selectedSourceId,
            [this](SampleSettings& s) { s.sourceKey = sourceKey.getSelectedItemIndex(); });
    };
    sourceTranspose.onValueChange = [this]
    {
        if (selectedSourceId.isNotEmpty()) processor.samples.updateSettings(selectedSourceId,
            [this](SampleSettings& s)
            {
                s.transposeSemitones = static_cast<int>(sourceTranspose.getValue());
            });
    };
    sourceFineTune.onValueChange = [this]
    {
        if (selectedSourceId.isNotEmpty()) processor.samples.updateSettings(selectedSourceId,
            [this](SampleSettings& s)
            {
                s.fineTuneCents = static_cast<float>(sourceFineTune.getValue());
            });
    };
    sourceGain.onValueChange = [this]
    {
        if (selectedSourceId.isNotEmpty()) processor.samples.updateSettings(selectedSourceId,
            [this](SampleSettings& s) { s.gainDb = static_cast<float>(sourceGain.getValue()); });
    };
    waveform.onRegionChanged = [this](double start, double end)
    {
        if (selectedSourceId.isEmpty())
            return;
        processor.samples.updateSettings(selectedSourceId, [start, end](SampleSettings& settings)
        {
            settings.startNormalised = start;
            settings.endNormalised = end;
        });
        displayPool = processor.samples.getSnapshot();
        list.repaint();
    };

    configureKnob(output, outputLabel, "OUTPUT");
    configureKnob(scrambleAmount, scrambleAmountLabel, "SCRAMBLE");
    configureKnob(meltAmount, meltAmountLabel, "MELT");
    configureLinearControl(spectralDepth, spectralDepthLabel, "SPECTRAL DEPTH");
    configureKnob(smearAmount, smearAmountLabel, "SMEAR");
    scrambleAmount.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff1587e8));
    meltAmount.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffe45aa5));
    smearAmount.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff8059dc));
    output.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff1686e4));
    spectralDepth.setColour(juce::Slider::trackColourId, juce::Colour(0xff7d59dc));
    outputAttachment = std::make_unique<SliderAttachment>(p.parameters, "output", output);
    targetKeyAttachment = std::make_unique<ComboBoxAttachment>(p.parameters, "targetKey", targetKey);
    voiceModeAttachment = std::make_unique<ButtonAttachment>(p.parameters, "voiceMode", voiceMode);
    midiPitchAttachment = std::make_unique<ButtonAttachment>(p.parameters, "midiPitch", midiPitch);
    voiceMode.setButtonText(voiceMode.getToggleState() ? "MONO" : "POLY");
    scrambleAmountAttachment = std::make_unique<SliderAttachment>(
        p.parameters, "scrambleAmount", scrambleAmount);
    meltAmountAttachment = std::make_unique<SliderAttachment>(
        p.parameters, "meltAmount", meltAmount);
    spectralDepthAttachment = std::make_unique<SliderAttachment>(
        p.parameters, "spectralDepth", spectralDepth);
    smearAmountAttachment = std::make_unique<SliderAttachment>(
        p.parameters, "smearAmount", smearAmount);

    refresh();
    startTimerHz(20);
}

RandomChopSamplerAudioProcessorEditor::~RandomChopSamplerAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void RandomChopSamplerAudioProcessorEditor::configureKnob(juce::Slider& slider, juce::Label& label,
                                                           const juce::String& text)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 54, 20);
    slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff8b5cf6));
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colour(xpInk));
    addAndMakeVisible(slider);
    addAndMakeVisible(label);
}

void RandomChopSamplerAudioProcessorEditor::configureLinearControl(
    juce::Slider& slider, juce::Label& label, const juce::String& text)
{
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 20);
    slider.setColour(juce::Slider::trackColourId, juce::Colour(0xff8b5cf6));
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colour(xpInk));
    addAndMakeVisible(slider);
    addAndMakeVisible(label);
}

void RandomChopSamplerAudioProcessorEditor::paint(juce::Graphics& g)
{
    const auto full = getLocalBounds().toFloat();
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xfff5f8fb), full.getX(), full.getY(),
                                           juce::Colour(0xffc7d9e8), full.getX(),
                                           full.getBottom(), false));
    g.fillRect(full);
    g.setColour(juce::Colours::white.withAlpha(0.24f));
    for (int y = titleBarBounds.getBottom() + 1; y < getHeight(); y += 3)
        g.drawHorizontalLine(y, 2.0f, static_cast<float>(getWidth() - 2));

    const auto bar = titleBarBounds.toFloat();
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff0c8aff), bar.getX(), bar.getY(),
                                           juce::Colour(0xff0752d2), bar.getX(),
                                           bar.getBottom(), false));
    g.fillRect(bar);
    g.setColour(juce::Colour(0xff0646ad));
    g.drawHorizontalLine(titleBarBounds.getBottom() - 1, bar.getX(), bar.getRight());

    auto icon = juce::Rectangle<float>(10.0f, bar.getY() + 6.0f, 28.0f, bar.getHeight() - 12.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colours::white, icon.getX(), icon.getY(),
                                           juce::Colour(0xff9fd3ff), icon.getX(),
                                           icon.getBottom(), false));
    g.fillRoundedRectangle(icon, 3.0f);
    g.setColour(juce::Colour(0xff173e78));
    g.drawRoundedRectangle(icon, 3.0f, 1.0f);
    juce::Path miniWave;
    for (int index = 0; index <= 16; ++index)
    {
        const auto unit = static_cast<float>(index) / 16.0f;
        const auto x = icon.getX() + 3.0f + unit * (icon.getWidth() - 6.0f);
        const auto y = icon.getCentreY() + std::sin(unit * 6.0f * juce::MathConstants<float>::pi)
            * (2.0f + 5.0f * std::sin(unit * juce::MathConstants<float>::pi));
        if (index == 0) miniWave.startNewSubPath(x, y); else miniWave.lineTo(x, y);
    }
    g.strokePath(miniWave, juce::PathStrokeType(1.3f));

    auto statusPlate = status.getBounds().expanded(5, 2).toFloat();
    g.setColour(juce::Colour(0xff064aa9).withAlpha(0.58f));
    g.fillRoundedRectangle(statusPlate, 6.0f);
    g.setColour(juce::Colour(0xff61b5ff).withAlpha(0.75f));
    g.drawRoundedRectangle(statusPlate, 6.0f, 1.0f);

    paintXpPanel(g, samplePanelBounds, "SAMPLES");
    paintXpPanel(g, sourcePanelBounds);
    paintXpPanel(g, globalPanelBounds, "GLOBAL");
    paintXpPanel(g, scramblePanelBounds, "SCRAMBLE");
    paintXpPanel(g, meltPanelBounds, "MELT");
    paintXpPanel(g, smearPanelBounds, "SMEAR");
    paintXpPanel(g, spectralPanelBounds, "SPECTRAL DRAW");
    paintXpPanel(g, outputPanelBounds, "OUTPUT");

    if (!sampleDropBounds.isEmpty())
    {
        const auto drop = sampleDropBounds.toFloat();
        g.setColour(juce::Colour(0xfff8fbff).withAlpha(0.75f));
        g.fillRoundedRectangle(drop, 5.0f);
        g.setColour(juce::Colour(0xff7a9fc4));
        const float dashes[] { 5.0f, 4.0f };
        g.drawDashedLine({ drop.getX(), drop.getY(), drop.getRight(), drop.getY() },
                         dashes, 2, 1.0f);
        g.drawDashedLine({ drop.getX(), drop.getBottom(), drop.getRight(), drop.getBottom() },
                         dashes, 2, 1.0f);
        g.setColour(juce::Colour(0xff6887a8));
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        g.drawFittedText("DROP WAV, AIFF, MP3 OR FLAC", sampleDropBounds.reduced(5),
                         juce::Justification::centred, 1);
    }

    g.setColour(juce::Colour(0xff3972ad));
    g.drawRoundedRectangle(full.reduced(1.0f), 7.0f, 2.0f);
}

void RandomChopSamplerAudioProcessorEditor::resized()
{
    constexpr int gap = 6;
    const auto verticalScale = juce::jlimit(0.86f, 1.25f,
        static_cast<float>(getHeight()) / 600.0f);
    const auto scaled = [verticalScale](int value)
    {
        return juce::jmax(1, juce::roundToInt(static_cast<float>(value) * verticalScale));
    };

    titleBarBounds = getLocalBounds().removeFromTop(scaled(42));
    auto header = titleBarBounds.reduced(7, 4);
    infoButton.setBounds(header.removeFromRight(scaled(29)).reduced(2));
    header.removeFromRight(5);
    status.setBounds(header.removeFromRight(175).reduced(3, 2));
    auto brand = header;
    brand.removeFromLeft(38);
    title.setBounds(brand.removeFromLeft(190));
    subtitle.setBounds(brand);

    auto area = getLocalBounds().withTrimmedTop(titleBarBounds.getHeight()).reduced(8, 7);
    const auto topHeight = juce::jlimit(scaled(202), scaled(270),
                                        juce::roundToInt(area.getHeight() * 0.43f));
    auto topRow = area.removeFromTop(topHeight);
    area.removeFromTop(gap);
    auto globalRow = area.removeFromTop(68);
    area.removeFromTop(gap);
    auto bottomRow = area;

    samplePanelBounds = topRow.removeFromLeft(juce::roundToInt(topRow.getWidth() * 0.29f));
    topRow.removeFromLeft(gap);
    sourcePanelBounds = topRow;

    auto sampleContent = samplePanelBounds.reduced(7).withTrimmedTop(25);
    sampleDropBounds = sampleContent.removeFromBottom(scaled(43)).reduced(2, 4);
    list.setBounds(sampleContent.reduced(1));

    auto sourceContent = sourcePanelBounds.reduced(7);
    auto sourceControlRow = sourceContent.removeFromBottom(scaled(65));
    waveform.setBounds(sourceContent.reduced(1));
    const auto cellWidth = sourceControlRow.getWidth() / 4;
    auto layoutSourceCell = [cellWidth](juce::Rectangle<int>& row, juce::Label& label,
                                        juce::Component& control)
    {
        auto cell = row.removeFromLeft(cellWidth).reduced(4, 2);
        label.setBounds(cell.removeFromTop(18));
        control.setBounds(cell.reduced(0, 1));
    };
    layoutSourceCell(sourceControlRow, sourceKeyLabel, sourceKey);
    layoutSourceCell(sourceControlRow, sourceTransposeLabel, sourceTranspose);
    layoutSourceCell(sourceControlRow, sourceFineTuneLabel, sourceFineTune);
    auto gainCell = sourceControlRow.reduced(4, 2);
    sourceGainLabel.setBounds(gainCell.removeFromTop(18));
    sourceGain.setBounds(gainCell.reduced(0, 1));

    globalPanelBounds = globalRow;
    auto globalContent = globalPanelBounds.reduced(8).withTrimmedTop(25);
    const auto globalCellWidth = globalContent.getWidth() / 3;
    auto targetCell = globalContent.removeFromLeft(globalCellWidth).reduced(4, 1);
    targetKeyLabel.setBounds(targetCell.removeFromLeft(78));
    targetKey.setBounds(targetCell);
    midiPitch.setBounds(globalContent.removeFromLeft(globalCellWidth).reduced(16, 1));
    auto voiceCell = globalContent.reduced(4, 1);
    voiceModeLabel.setBounds(voiceCell.removeFromLeft(92));
    voiceMode.setBounds(voiceCell.reduced(6, 0));

    const auto available = bottomRow.getWidth() - gap * 4;
    const auto effectWidth = juce::roundToInt(static_cast<float>(available) * 0.175f);
    const auto spectralWidth = juce::roundToInt(static_cast<float>(available) * 0.36f);
    scramblePanelBounds = bottomRow.removeFromLeft(effectWidth);
    bottomRow.removeFromLeft(gap);
    meltPanelBounds = bottomRow.removeFromLeft(effectWidth);
    bottomRow.removeFromLeft(gap);
    smearPanelBounds = bottomRow.removeFromLeft(effectWidth);
    bottomRow.removeFromLeft(gap);
    spectralPanelBounds = bottomRow.removeFromLeft(spectralWidth);
    bottomRow.removeFromLeft(gap);
    outputPanelBounds = bottomRow;

    const auto layoutEffect = [scaled](juce::Rectangle<int> panel, juce::Label& label,
                                        juce::Slider& slider, CreativeVisualizer& visual)
    {
        label.setBounds(0, 0, 0, 0);
        auto content = panel.reduced(7).withTrimmedTop(25);
        auto knobArea = content.removeFromTop(juce::jmin(scaled(91), content.getHeight() / 2));
        slider.setBounds(knobArea.withSizeKeepingCentre(juce::jmin(104, knobArea.getWidth()),
                                                        knobArea.getHeight()));
        visual.setBounds(content.reduced(1, 2));
    };
    layoutEffect(scramblePanelBounds, scrambleAmountLabel, scrambleAmount, scrambleVisual);
    layoutEffect(meltPanelBounds, meltAmountLabel, meltAmount, meltVisual);
    layoutEffect(smearPanelBounds, smearAmountLabel, smearAmount, smearVisual);

    spectralDrawLabel.setBounds(0, 0, 0, 0);
    auto spectralContent = spectralPanelBounds.reduced(7).withTrimmedTop(25);
    auto spectralTools = spectralContent.removeFromBottom(scaled(55));
    spectralCanvas.setBounds(spectralContent.reduced(1, 2));
    auto depthArea = spectralTools.reduced(4, 3);
    spectralResetButton.setBounds(depthArea.removeFromRight(66).reduced(2, 8));
    spectralDepthLabel.setBounds(depthArea.removeFromTop(17));
    spectralDepth.setBounds(depthArea.reduced(2, 0));

    outputLabel.setBounds(0, 0, 0, 0);
    auto outputContent = outputPanelBounds.reduced(7).withTrimmedTop(25);
    output.setBounds(outputContent.withSizeKeepingCentre(
        juce::jmin(105, outputContent.getWidth()), juce::jmin(125, outputContent.getHeight())));

    const auto dialogWidth = juce::jmin(350, getWidth() - 48);
    const auto dialogHeight = juce::jmin(180, getHeight() - 48);
    infoPanel.setBounds(getLocalBounds().withSizeKeepingCentre(dialogWidth, dialogHeight));
}

bool RandomChopSamplerAudioProcessorEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& path : files) if (SampleManager::isSupported(juce::File(path))) return true;
    return false;
}

void RandomChopSamplerAudioProcessorEditor::filesDropped(const juce::StringArray& files, int, int)
{
    addFiles(files);
}

void RandomChopSamplerAudioProcessorEditor::addFiles(const juce::StringArray& files)
{
    const auto errors = processor.samples.addFiles(files);
    transientMessage = errors.empty() ? juce::String()
                                      : juce::String(errors.size()) + " file(s) rejected";
    refresh();
}

void RandomChopSamplerAudioProcessorEditor::refresh()
{
    displayPool = processor.samples.getSnapshot();
    int selected = -1;
    for (size_t i = 0; displayPool && i < displayPool->size(); ++i)
        if ((*displayPool)[i]->settings.id == selectedSourceId)
            selected = static_cast<int>(i);
    if (selected < 0 && displayPool && !displayPool->empty())
    {
        selected = 0;
        selectedSourceId = displayPool->front()->settings.id;
    }
    if (!displayPool || displayPool->empty())
        selectedSourceId.clear();
    list.updateContent();
    list.selectRow(selected);
    if (displayPool && selected >= 0 && selected < static_cast<int>(displayPool->size()))
    {
        const auto& selectedSource = (*displayPool)[static_cast<size_t>(selected)];
        sourceKey.setSelectedItemIndex(selectedSource->settings.sourceKey,
                                       juce::dontSendNotification);
        sourceTranspose.setValue(selectedSource->settings.transposeSemitones,
                                 juce::dontSendNotification);
        sourceFineTune.setValue(selectedSource->settings.fineTuneCents,
                                juce::dontSendNotification);
        sourceGain.setValue(selectedSource->settings.gainDb, juce::dontSendNotification);
        waveform.setSource(selectedSource);
    }
    else
    {
        waveform.setSource({});
    }
    list.repaint();
    repaint();
}

int RandomChopSamplerAudioProcessorEditor::getNumRows()
{
    return displayPool ? static_cast<int>(displayPool->size()) : 0;
}

void RandomChopSamplerAudioProcessorEditor::paintListBoxItem(int row, juce::Graphics& g, int width,
                                                              int height, bool selected)
{
    if (!displayPool || row < 0 || row >= static_cast<int>(displayPool->size())) return;
    const auto& source = (*displayPool)[static_cast<size_t>(row)];
    const bool recent = source->runtimeId
        == processor.lastTriggeredRuntimeId.load(std::memory_order_relaxed);
    if (selected)
    {
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xffd9eeff), 0.0f, 0.0f,
                                               juce::Colour(0xff8ec8fa), 0.0f,
                                               static_cast<float>(height), false));
        g.fillRect(0, 0, width, height);
    }
    else
    {
        g.fillAll(recent ? juce::Colour(0xffe7efff) : juce::Colour(0xfff7f9fc));
    }
    g.setColour(juce::Colour(0xffd0dce8));
    g.drawHorizontalLine(height - 1, 0.0f, static_cast<float>(width));
    const auto iconX = 8.0f;
    const auto centreY = static_cast<float>(height) * 0.5f;
    g.setColour(juce::Colour(0xff174d83));
    for (int line = 0; line < 5; ++line)
    {
        const auto h = line % 2 == 0 ? 12.0f : 7.0f;
        g.drawVerticalLine(juce::roundToInt(iconX + line * 3.0f), centreY - h * 0.5f,
                           centreY + h * 0.5f);
    }
    g.setColour(source->settings.missing ? juce::Colour(0xffb52a2a) : juce::Colour(xpInk));
    const auto suffix = source->settings.missing ? juce::String("  [MISSING]")
                                                 : juce::String();
    g.setFont(13.0f);
    g.drawText(source->settings.displayName + suffix, 27, 0, width - 166, height,
               juce::Justification::centredLeft, true);
}

juce::Component* RandomChopSamplerAudioProcessorEditor::refreshComponentForRow(int row, bool,
                                                                                juce::Component* existing)
{
    auto* controls = dynamic_cast<SourceRowControls*>(existing);
    if (controls == nullptr) { delete existing; controls = new SourceRowControls(); }
    controls->row = row;
    controls->onSelect = [this](int selectedRow)
    {
        if (selectedRow >= 0 && selectedRow < getNumRows())
            list.selectRow(selectedRow);
    };
    if (displayPool && row >= 0 && row < static_cast<int>(displayPool->size()))
    {
        controls->enabled.setToggleState((*displayPool)[static_cast<size_t>(row)]->settings.enabled,
                                         juce::dontSendNotification);
        controls->enabled.setButtonText(controls->enabled.getToggleState() ? "ON" : "OFF");
    }
    controls->enabled.onClick = [this, controls]
    {
        if (controls->onSelect)
            controls->onSelect(controls->row);
        if (displayPool && controls->row >= 0
            && controls->row < static_cast<int>(displayPool->size()))
            processor.samples.setEnabled((*displayPool)[static_cast<size_t>(controls->row)]->settings.id,
                                         controls->enabled.getToggleState());
        refresh();
    };
    controls->remove.onClick = [this, controls]
    {
        if (displayPool && controls->row >= 0
            && controls->row < static_cast<int>(displayPool->size()))
            processor.samples.remove((*displayPool)[static_cast<size_t>(controls->row)]->settings.id);
        refresh();
    };
    return controls;
}

void RandomChopSamplerAudioProcessorEditor::selectedRowsChanged(int row)
{
    if (displayPool && row >= 0 && row < static_cast<int>(displayPool->size()))
    {
        const auto& settings = (*displayPool)[static_cast<size_t>(row)]->settings;
        selectedSourceId = settings.id;
        sourceKey.setSelectedItemIndex(settings.sourceKey, juce::dontSendNotification);
        sourceTranspose.setValue(settings.transposeSemitones, juce::dontSendNotification);
        sourceFineTune.setValue(settings.fineTuneCents, juce::dontSendNotification);
        sourceGain.setValue(settings.gainDb, juce::dontSendNotification);
        waveform.setSource((*displayPool)[static_cast<size_t>(row)]);
    }
}

void RandomChopSamplerAudioProcessorEditor::timerCallback()
{
    processor.samples.collectGarbage();
    if (processor.samples.getSnapshot() != displayPool)
        refresh();
    const int count = processor.samples.size();
    auto message = juce::String(count).paddedLeft('0', 2) + " / 20 SOURCES";
    if (processor.triggeredWhileEmpty.load(std::memory_order_relaxed))
        message = "No enabled playable sources";
    else if (transientMessage.isNotEmpty())
        message += " — " + transientMessage;
    status.setText(message, juce::dontSendNotification);
    voiceMode.setButtonText(voiceMode.getToggleState() ? "MONO" : "POLY");
    midiPitch.setButtonText(midiPitch.getToggleState() ? "CHORDS ON" : "CHORDS OFF");
    const auto canvasGeneration = processor.getSpectralCanvasGeneration();
    if (canvasGeneration != lastSpectralCanvasGeneration)
    {
        spectralCanvas.setCanvas(processor.getSpectralCanvas());
        lastSpectralCanvasGeneration = canvasGeneration;
    }
    spectralCanvas.setScanPosition(processor.getSpectralScanPosition());
    const auto readParameter = [this](const char* id)
    {
        if (const auto* value = processor.parameters.getRawParameterValue(id))
            return value->load();
        return 0.0f;
    };
    scrambleVisual.setState(readParameter("scrambleAmount"));
    meltVisual.setState(readParameter("meltAmount"));
    smearVisual.setState(readParameter("smearAmount"));
    scrambleVisual.setTelemetry(processor.getScrambleVisualPhase(), 0.0f,
                                processor.getScrambleVisualFlags());
    meltVisual.setTelemetry(processor.getMeltVisualStretch(),
                            processor.getMeltVisualProgress(),
                            processor.getMeltVisualFlags());
    smearVisual.setTelemetry(processor.getSmearVisualActivity(),
                             processor.getSmearVisualGain());
    scrambleVisual.advance();
    meltVisual.advance();
    smearVisual.advance();
    list.repaint();
}

