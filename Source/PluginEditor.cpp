#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

namespace
{
constexpr auto xpInk = 0xff1b1b1b;
constexpr auto xpPanel = 0xffd7d7d7;

void paintXpPanel(juce::Graphics& g, juce::Rectangle<int> bounds,
                  const juce::String& heading = {})
{
    if (bounds.isEmpty())
        return;

    const auto area = bounds.toFloat();
    g.setColour(juce::Colour(0xff777777).withAlpha(0.45f));
    g.fillRoundedRectangle(area.translated(1.0f, 2.0f), 4.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xfff7f7f7), area.getX(), area.getY(),
                                           juce::Colour(xpPanel), area.getX(), area.getBottom(), false));
    g.fillRoundedRectangle(area, 4.0f);
    g.setColour(juce::Colours::white.withAlpha(0.9f));
    g.drawRoundedRectangle(area.reduced(1.0f), 3.0f, 1.2f);
    g.setColour(juce::Colour(0xff555555));
    g.drawRoundedRectangle(area.reduced(0.5f), 4.0f, 1.3f);

    if (heading.isNotEmpty())
    {
        auto headingArea = area.reduced(3.0f).removeFromTop(25.0f);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xfff2f2f2), headingArea.getX(),
                                               headingArea.getY(), juce::Colour(0xffc8c8c8),
                                               headingArea.getX(), headingArea.getBottom(), false));
        g.fillRoundedRectangle(headingArea, 3.0f);
        g.setColour(juce::Colour(0xff777777));
        g.drawHorizontalLine(juce::roundToInt(headingArea.getBottom()),
                             headingArea.getX() + 4.0f, headingArea.getRight() - 4.0f);
        g.setColour(juce::Colour(xpInk));
        g.setFont(juce::Font(15.0f, juce::Font::bold));
        g.drawText(heading, headingArea.reduced(11.0f, 0.0f).toNearestInt(),
                   juce::Justification::centredLeft, false);

        const auto hatch = headingArea.withTrimmedLeft(headingArea.getWidth() * 0.62f)
                                      .reduced(7.0f, 6.0f);
        g.setColour(juce::Colour(0xff555555).withAlpha(0.42f));
        for (float x = hatch.getX(); x < hatch.getRight(); x += 5.0f)
            g.drawLine(x, hatch.getBottom(), x + 8.0f, hatch.getY(), 1.0f);
    }
}
}

XpLookAndFeel::XpLookAndFeel()
{
    setColour(juce::Label::textColourId, juce::Colour(xpInk));
    setColour(juce::TextButton::buttonColourId, juce::Colour(0xffdedede));
    setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff333333));
    setColour(juce::TextButton::textColourOffId, juce::Colour(xpInk));
    setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xffeeeeee));
    setColour(juce::ComboBox::textColourId, juce::Colour(xpInk));
    setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff333333));
    setColour(juce::ComboBox::arrowColourId, juce::Colour(xpInk));
    setColour(juce::Slider::textBoxTextColourId, juce::Colour(xpInk));
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xffededed));
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff444444));
    setColour(juce::Slider::trackColourId, juce::Colour(0xff333333));
    setColour(juce::Slider::thumbColourId, juce::Colour(0xffdddddd));
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xffeeeeee));
    setColour(juce::PopupMenu::textColourId, juce::Colour(xpInk));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff333333));
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
}

void XpLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                         const juce::Colour&, bool over, bool down)
{
    if (button.getName() == "Fold card") return;
    if (button.getName() == "Effect power")
    {
        auto circle = button.getLocalBounds().toFloat().reduced(2.0f);
        g.setColour(button.getToggleState() ? juce::Colour(0xff222222)
                                            : juce::Colour(0xffaaaaaa));
        g.fillEllipse(circle);
        g.setColour(juce::Colour(0xff333333));
        g.drawEllipse(circle, 1.1f);
        if (over || down)
        {
            g.setColour(juce::Colour(0xff555555));
            g.drawEllipse(circle.expanded(1.0f), 1.0f);
        }
        return;
    }
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    const auto top = button.getToggleState() ? juce::Colour(0xff353535) : down ? juce::Colour(0xffa9a9a9)
                          : (over ? juce::Colour(0xffffffff) : juce::Colour(0xffeeeeee));
    const auto bottom = button.getToggleState() ? juce::Colour(0xff171717) : down ? juce::Colour(0xffdddddd)
                             : (over ? juce::Colour(0xffd4d4d4) : juce::Colour(0xffc6c6c6));
    g.setGradientFill(juce::ColourGradient(top, bounds.getCentreX(), bounds.getY(), bottom,
                                           bounds.getCentreX(), bounds.getBottom(), false));
    g.fillRoundedRectangle(bounds, 2.0f);
    g.setColour(juce::Colours::white.withAlpha(down ? 0.45f : 0.9f));
    g.drawRoundedRectangle(bounds.reduced(1.0f), 1.0f, 1.0f);
    g.setColour(juce::Colour(down ? 0xff111111 : 0xff555555));
    g.drawRoundedRectangle(bounds, 2.0f, 1.2f);
    if (button.hasKeyboardFocus(true))
    {
        g.setColour(juce::Colour(0xff222222));
        g.drawRoundedRectangle(bounds.reduced(2.5f), 1.0f, 1.4f);
    }
}

void XpLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                   bool, bool)
{
    if (button.getName() == "Effect power") return;
    if (button.getName() == "Fold card")
    {
        const auto centre = button.getLocalBounds().toFloat().getCentre();
        juce::Path arrow;
        arrow.startNewSubPath(centre.x - 3.0f, centre.y - 5.0f);
        arrow.lineTo(centre.x + 4.0f, centre.y);
        arrow.lineTo(centre.x - 3.0f, centre.y + 5.0f);
        arrow.closeSubPath();
        g.setColour(juce::Colour(0xff222222));
        g.fillPath(arrow);
        return;
    }
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

    g.setColour(juce::Colour(0xff333333).withAlpha(0.72f));
    for (int tick = 0; tick <= 10; ++tick)
    {
        const auto tickAngle = startAngle + static_cast<float>(tick) * (endAngle - startAngle) / 10.0f;
        const auto tickStart = centre.getPointOnCircumference(radius + 0.5f, tickAngle);
        const auto tickEnd = centre.getPointOnCircumference(radius + (tick % 5 == 0 ? 4.5f : 3.0f),
                                                             tickAngle);
        g.drawLine({ tickStart, tickEnd }, tick % 5 == 0 ? 1.4f : 0.9f);
    }

    g.setColour(juce::Colour(0xff555555).withAlpha(0.35f));
    g.fillEllipse(bounds.translated(1.5f, 2.5f));
    juce::Path track;
    track.addCentredArc(centre.x, centre.y, radius - 2.5f, radius - 2.5f, 0.0f,
                        startAngle, endAngle, true);
    g.setColour(juce::Colour(0xff9a9a9a));
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
                                           juce::Colour(0xff9a9a9a), face.getRight(),
                                           face.getBottom(), false));
    g.fillEllipse(face);
    g.setColour(juce::Colour(0xfff3f3f3));
    g.drawEllipse(face.reduced(1.0f), 1.6f);
    g.setColour(juce::Colour(0xff252525));
    g.drawEllipse(face, 1.2f);
    const auto pointerStart = centre.getPointOnCircumference(radius * 0.20f, angle);
    const auto pointerEnd = centre.getPointOnCircumference(radius * 0.62f, angle);
    g.setColour(juce::Colour(0xff172231));
    g.drawLine({ pointerStart, pointerEnd }, 2.4f);
    g.fillEllipse(centre.x - 2.3f, centre.y - 2.3f, 4.6f, 4.6f);
    if (slider.isMouseOverOrDragging())
    {
        g.setColour(accent.withAlpha(0.28f));
        g.drawEllipse(bounds.expanded(1.5f), 2.2f);
    }
    if (slider.hasKeyboardFocus(true))
    {
        g.setColour(juce::Colour(0xff222222));
        g.drawEllipse(bounds.expanded(3.0f), 1.5f);
    }
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
    g.setColour(juce::Colour(0xff9b9b9b));
    g.fillRoundedRectangle(left, cy - 3.0f, right - left, 6.0f, 3.0f);
    g.setColour(slider.findColour(juce::Slider::trackColourId));
    g.fillRoundedRectangle(left, cy - 2.0f, juce::jmax(0.0f, sliderPos - left), 4.0f, 2.0f);
    auto thumb = juce::Rectangle<float>(sliderPos - 5.0f, cy - 10.0f, 10.0f, 20.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colours::white, thumb.getX(), thumb.getY(),
                                           juce::Colour(0xffb6b6b6), thumb.getX(),
                                           thumb.getBottom(), false));
    g.fillRoundedRectangle(thumb, 2.0f);
    g.setColour(juce::Colour(0xff555555));
    g.drawRoundedRectangle(thumb, 2.0f, 1.0f);
    if (slider.hasKeyboardFocus(true))
    {
        g.setColour(juce::Colour(0xff222222));
        g.drawRoundedRectangle(thumb.expanded(2.0f), 3.0f, 1.4f);
    }
}

void XpLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool down,
                                 int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width),
                                         static_cast<float>(height)).reduced(0.5f);
    g.setGradientFill(juce::ColourGradient(juce::Colours::white, 0.0f, 0.0f,
                                           juce::Colour(0xffcccccc), 0.0f,
                                           static_cast<float>(height), false));
    g.fillRoundedRectangle(bounds, 2.0f);
    g.setColour(box.findColour(juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle(bounds, 2.0f, 1.1f);
    auto arrowArea = bounds.removeFromRight(static_cast<float>(height));
    g.setColour(juce::Colour(down ? 0xff999999 : 0xffbbbbbb));
    g.fillRoundedRectangle(arrowArea.reduced(2.0f), 1.0f);
    juce::Path arrow;
    const auto centre = arrowArea.getCentre();
    arrow.startNewSubPath(centre.x - 5.0f, centre.y - 2.0f);
    arrow.lineTo(centre.x + 5.0f, centre.y - 2.0f);
    arrow.lineTo(centre.x, centre.y + 4.0f);
    arrow.closeSubPath();
    g.setColour(juce::Colour(xpInk));
    g.fillPath(arrow);
    if (box.hasKeyboardFocus(true))
    {
        g.setColour(juce::Colour(0xff222222));
        g.drawRoundedRectangle(juce::Rectangle<float>(1.8f, 1.8f,
            static_cast<float>(width) - 3.6f, static_cast<float>(height) - 3.6f), 3.0f, 1.4f);
    }
}

void XpLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(8, 1, box.getWidth() - box.getHeight() - 7, box.getHeight() - 2);
    label.setFont(juce::Font(14.0f));
}

void XpLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                     bool over, bool down)
{
    if (button.getName() == "Source enabled")
    {
        const auto box = button.getLocalBounds().toFloat().reduced(1.5f);
        g.setColour(over ? juce::Colours::white : juce::Colour(0xffececec));
        g.fillRect(box);
        g.setColour(juce::Colour(0xff333333));
        g.drawRect(box, 1.2f);
        if (button.getToggleState())
        {
            juce::Path check;
            check.startNewSubPath(box.getX() + box.getWidth() * 0.18f,
                                  box.getY() + box.getHeight() * 0.5f);
            check.lineTo(box.getX() + box.getWidth() * 0.42f,
                         box.getY() + box.getHeight() * 0.78f);
            check.lineTo(box.getX() + box.getWidth() * 0.82f,
                         box.getY() + box.getHeight() * 0.18f);
            g.strokePath(check, juce::PathStrokeType(2.1f));
        }
        return;
    }
    auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
    const auto on = button.getToggleState();
    const auto top = on ? juce::Colour(0xff3c3c3c)
                        : juce::Colour(over ? 0xfffafafa : 0xffe5e5e5);
    const auto bottom = on ? juce::Colour(0xff181818)
                           : juce::Colour(down ? 0xffaaaaaa : 0xffc5c5c5);
    g.setGradientFill(juce::ColourGradient(top, bounds.getCentreX(), bounds.getY(), bottom,
                                           bounds.getCentreX(), bounds.getBottom(), false));
    g.fillRoundedRectangle(bounds, 2.0f);
    g.setColour(juce::Colour(on ? 0xff111111 : 0xff555555));
    g.drawRoundedRectangle(bounds, 2.0f, 1.2f);
    g.setColour(on ? juce::Colours::white : juce::Colour(xpInk));
    g.setFont(juce::Font(12.0f, juce::Font::bold));
    g.drawFittedText(button.getButtonText().toUpperCase(), button.getLocalBounds().reduced(8, 1),
                     juce::Justification::centred, 1);
    if (button.hasKeyboardFocus(true))
    {
        g.setColour(juce::Colour(on ? 0xffffffff : 0xff222222).withAlpha(0.9f));
        g.drawRoundedRectangle(bounds.reduced(2.5f), 1.0f, 1.2f);
    }
}

void XpLookAndFeel::drawScrollbar(juce::Graphics& g, juce::ScrollBar&, int x, int y,
                                  int width, int height, bool vertical, int thumbStart,
                                  int thumbSize, bool over, bool down)
{
    auto track = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                        static_cast<float>(width), static_cast<float>(height));
    g.setColour(juce::Colour(0xffdddddd));
    g.fillRect(track);
    g.setColour(juce::Colour(0xff777777));
    g.drawRect(track, 1.0f);
    auto thumb = vertical
        ? juce::Rectangle<float>(static_cast<float>(x + 2), static_cast<float>(thumbStart),
                                 static_cast<float>(width - 4), static_cast<float>(thumbSize))
        : juce::Rectangle<float>(static_cast<float>(thumbStart), static_cast<float>(y + 2),
                                 static_cast<float>(thumbSize), static_cast<float>(height - 4));
    g.setGradientFill(juce::ColourGradient(
        juce::Colour(over ? 0xffffffff : 0xffeeeeee), thumb.getX(), thumb.getY(),
        juce::Colour(down ? 0xff999999 : 0xffbbbbbb), thumb.getRight(), thumb.getBottom(), false));
    g.fillRoundedRectangle(thumb, 3.0f);
    g.setColour(juce::Colour(0xff555555));
    g.drawRoundedRectangle(thumb, 3.0f, 1.0f);
    g.setColour(juce::Colour(0xff777777));
    const auto centre = thumb.getCentre();
    for (int offset = -3; offset <= 3; offset += 3)
    {
        if (vertical)
            g.drawHorizontalLine(juce::roundToInt(centre.y + static_cast<float>(offset)),
                                 centre.x - 3.0f, centre.x + 3.0f);
        else
            g.drawVerticalLine(juce::roundToInt(centre.x + static_cast<float>(offset)),
                               centre.y - 3.0f, centre.y + 3.0f);
    }
}

void XpLookAndFeel::drawTooltip(juce::Graphics& g, const juce::String& text,
                                int width, int height)
{
    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width),
                                         static_cast<float>(height)).reduced(0.5f);
    g.setColour(juce::Colour(0xff333333).withAlpha(0.28f));
    g.fillRoundedRectangle(bounds.translated(1.0f, 1.5f), 4.0f);
    g.setColour(juce::Colour(0xfff2f2f2));
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(juce::Colour(0xff555555));
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
    g.setColour(juce::Colour(0xff222222));
    g.setFont(12.5f);
    g.drawFittedText(text, 7, 4, width - 14, height - 8,
                     juce::Justification::centredLeft, 3);
}

void SourceWaveformComponent::setSource(SampleManager::SamplePtr newSource)
{
    if (source == nullptr || newSource == nullptr
        || source->settings.id != newSource->settings.id)
    {
        viewStart = 0.0;
        viewSpan = 1.0;
    }
    source = std::move(newSource);
    region = source != nullptr
        ? randomchop::clampNormalisedRegion(source->settings.startNormalised,
                                            source->settings.endNormalised)
        : randomchop::NormalisedRegion {};
    setTooltip(source != nullptr
        ? "Drag START and END to set the playable source region."
        : "Select a sample to view and edit its playable region.");
    repaint();
}

void SourceWaveformComponent::zoomIn()
{
    const auto centre = viewStart + viewSpan * 0.5;
    viewSpan = juce::jmax(0.025, viewSpan * 0.5);
    viewStart = juce::jlimit(0.0, 1.0 - viewSpan, centre - viewSpan * 0.5);
    repaint();
}

void SourceWaveformComponent::zoomOut()
{
    const auto centre = viewStart + viewSpan * 0.5;
    viewSpan = juce::jmin(1.0, viewSpan * 2.0);
    viewStart = juce::jlimit(0.0, 1.0 - viewSpan, centre - viewSpan * 0.5);
    repaint();
}

void SourceWaveformComponent::focusRegion()
{
    viewSpan = juce::jlimit(0.025, 1.0, (region.end - region.start) * 1.1);
    viewStart = juce::jlimit(0.0, 1.0 - viewSpan,
                             (region.start + region.end - viewSpan) * 0.5);
    repaint();
}

void SourceWaveformComponent::fitAll()
{
    viewStart = 0.0;
    viewSpan = 1.0;
    repaint();
}

juce::Rectangle<int> SourceWaveformComponent::getWaveformBounds() const
{
    return getLocalBounds().reduced(5).withTrimmedTop(25).withTrimmedRight(44);
}

double SourceWaveformComponent::positionToNormalised(float x) const noexcept
{
    const auto bounds = getWaveformBounds();
    if (bounds.getWidth() <= 0)
        return 0.0;
    return juce::jlimit(viewStart, viewStart + viewSpan,
        static_cast<double>(x - static_cast<float>(bounds.getX()))
            / static_cast<double>(bounds.getWidth()) * viewSpan + viewStart);
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
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff353535), outer.getX(), outer.getY(),
                                           juce::Colour(0xff171717), outer.getX(),
                                           outer.getBottom(), false));
    g.fillRoundedRectangle(outer, 3.0f);
    g.setColour(juce::Colour(0xff555555));
    g.drawRoundedRectangle(outer.reduced(0.5f), 3.0f, 1.4f);

    g.setFont(juce::Font(13.0f, juce::Font::bold));
    g.setColour(juce::Colours::white);
    juce::String heading("SELECT A SOURCE");
    if (source != nullptr)
        heading = source->settings.displayName;
    auto headingBounds = getLocalBounds().reduced(9).removeFromTop(18);
    g.drawText(heading, headingBounds.withTrimmedRight(190),
               juce::Justification::centredLeft, true);
    if (source != nullptr && source->audio != nullptr)
    {
        const auto seconds = static_cast<double>(source->audio->getNumSamples())
            / juce::jmax(1.0, source->sampleRate);
        const auto details = juce::String(source->sampleRate / 1000.0, 1) + " kHz   "
            + (source->bitDepth > 0 ? juce::String(source->bitDepth) + " bit   " : juce::String())
            + juce::String(seconds, 1) + " s";
        g.setFont(11.0f);
        g.setColour(juce::Colour(0xffdedede));
        g.drawText(details, headingBounds.removeFromRight(185),
                   juce::Justification::centredRight, true);
    }

    const auto waveBounds = getWaveformBounds();
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff222222),
                                           static_cast<float>(waveBounds.getX()),
                                           static_cast<float>(waveBounds.getY()),
                                           juce::Colour(0xff292929),
                                           static_cast<float>(waveBounds.getX()),
                                           static_cast<float>(waveBounds.getBottom()), false));
    g.fillRoundedRectangle(waveBounds.toFloat(), 1.0f);
    g.setColour(juce::Colour(0xffb0b0b0).withAlpha(0.4f));
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
    g.setColour(juce::Colour(0xffbcbcbc).withAlpha(0.55f));
    g.drawHorizontalLine(waveBounds.getCentreY(), static_cast<float>(waveBounds.getX()),
                         static_cast<float>(waveBounds.getRight()));

    if (source == nullptr)
    {
        g.setColour(juce::Colour(0xffdddddd));
        g.drawText("Select a source to edit its region", waveBounds,
                   juce::Justification::centred);
        g.setColour(juce::Colour(0xff888888));
        g.drawRoundedRectangle(waveBounds.toFloat(), 1.0f, 1.0f);
        return;
    }

    if (source->settings.missing || source->audio == nullptr
        || source->waveformPeaks == nullptr || source->waveformPeaks->empty())
    {
        g.setColour(juce::Colour(0xffeeeeee));
        g.drawText("Waveform unavailable for missing source", waveBounds,
                   juce::Justification::centred);
    }
    else
    {
        const auto& peaks = *source->waveformPeaks;
        const auto peakCount = peaks.size();
        const auto halfHeight = static_cast<float>(waveBounds.getHeight()) * 0.46f;
        const auto centreY = static_cast<float>(waveBounds.getCentreY());
        g.setColour(juce::Colour(0xfff5f5f5));
        for (int x = 0; x < waveBounds.getWidth(); ++x)
        {
            const auto first = static_cast<size_t>((viewStart
                + static_cast<double>(x) * viewSpan / waveBounds.getWidth()) * peakCount);
            const auto last = juce::jmax(first + 1,
                static_cast<size_t>((viewStart
                    + static_cast<double>(x + 1) * viewSpan / waveBounds.getWidth()) * peakCount));
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
        + static_cast<float>((region.start - viewStart) / viewSpan) * static_cast<float>(waveBounds.getWidth());
    const auto endX = static_cast<float>(waveBounds.getX())
        + static_cast<float>((region.end - viewStart) / viewSpan) * static_cast<float>(waveBounds.getWidth());
    g.saveState();
    g.reduceClipRegion(waveBounds);
    g.setColour(juce::Colour(0xffffffff).withAlpha(0.06f));
    g.fillRect(juce::Rectangle<float>(startX, static_cast<float>(waveBounds.getY()),
                                     juce::jmax(0.0f, endX - startX),
                                     static_cast<float>(waveBounds.getHeight())));
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.fillRect(juce::Rectangle<float>(static_cast<float>(waveBounds.getX()),
                                     static_cast<float>(waveBounds.getY()),
                                     juce::jmax(0.0f, startX - waveBounds.getX()),
                                     static_cast<float>(waveBounds.getHeight())));
    g.fillRect(juce::Rectangle<float>(endX, static_cast<float>(waveBounds.getY()),
                                     juce::jmax(0.0f, waveBounds.getRight() - endX),
                                     static_cast<float>(waveBounds.getHeight())));

    const auto startHovered = hoverMarker == DragMarker::start || dragMarker == DragMarker::start;
    const auto endHovered = hoverMarker == DragMarker::end || dragMarker == DragMarker::end;
    g.setColour(juce::Colour(0xffffffff).withAlpha(startHovered ? 0.28f : 0.12f));
    g.fillRect(startX - (startHovered ? 5.0f : 3.0f), static_cast<float>(waveBounds.getY()),
               startHovered ? 10.0f : 6.0f, static_cast<float>(waveBounds.getHeight()));
    g.setColour(juce::Colour(0xffeeeeee));
    g.drawLine(startX, static_cast<float>(waveBounds.getY()), startX,
               static_cast<float>(waveBounds.getBottom()), startHovered ? 3.0f : 2.0f);
    g.setColour(juce::Colour(0xffffffff).withAlpha(endHovered ? 0.28f : 0.12f));
    g.fillRect(endX - (endHovered ? 5.0f : 3.0f), static_cast<float>(waveBounds.getY()),
               endHovered ? 10.0f : 6.0f, static_cast<float>(waveBounds.getHeight()));
    g.setColour(juce::Colour(0xffeeeeee));
    g.drawLine(endX, static_cast<float>(waveBounds.getY()), endX,
               static_cast<float>(waveBounds.getBottom()), endHovered ? 3.0f : 2.0f);

    const auto drawHandle = [&g, &waveBounds](float x, juce::Colour colour, bool pointsRight)
    {
        juce::Path handle;
        const auto y = static_cast<float>(waveBounds.getBottom()) - 3.0f;
        handle.startNewSubPath(x, y - 8.0f);
        handle.lineTo(x + (pointsRight ? 7.0f : -7.0f), y);
        handle.lineTo(x, y);
        handle.closeSubPath();
        g.setColour(colour);
        g.fillPath(handle);
    };
    drawHandle(startX, juce::Colour(0xffeeeeee), true);
    drawHandle(endX, juce::Colour(0xffeeeeee), false);

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
        g.setColour(juce::Colour(0xff111111));
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        g.drawText(text, tag.toNearestInt(), juce::Justification::centred, false);
    };
    drawTag(startX, "START", juce::Colour(0xffeeeeee));
    drawTag(endX, "END", juce::Colour(0xffeeeeee));
    g.restoreState();
    g.setColour(juce::Colour(0xff888888));
    g.drawRoundedRectangle(waveBounds.toFloat(), 1.0f, 1.1f);
}

void SourceWaveformComponent::mouseDown(const juce::MouseEvent& event)
{
    if (source == nullptr || source->audio == nullptr)
        return;

    const auto bounds = getWaveformBounds();
    const auto startX = static_cast<float>(bounds.getX())
        + static_cast<float>((region.start - viewStart) / viewSpan) * static_cast<float>(bounds.getWidth());
    const auto endX = static_cast<float>(bounds.getX())
        + static_cast<float>((region.end - viewStart) / viewSpan) * static_cast<float>(bounds.getWidth());
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
    repaint();
}

void SourceWaveformComponent::mouseMove(const juce::MouseEvent& event)
{
    if (source == nullptr || source->audio == nullptr)
        return;

    const auto bounds = getWaveformBounds();
    const auto startX = static_cast<float>(bounds.getX())
        + static_cast<float>((region.start - viewStart) / viewSpan) * static_cast<float>(bounds.getWidth());
    const auto endX = static_cast<float>(bounds.getX())
        + static_cast<float>((region.end - viewStart) / viewSpan) * static_cast<float>(bounds.getWidth());
    const auto distanceToStart = std::abs(event.position.x - startX);
    const auto distanceToEnd = std::abs(event.position.x - endX);
    const auto next = juce::jmin(distanceToStart, distanceToEnd) <= 9.0f
        ? (distanceToStart <= distanceToEnd ? DragMarker::start : DragMarker::end)
        : DragMarker::none;
    if (next != hoverMarker)
    {
        hoverMarker = next;
        setMouseCursor(hoverMarker == DragMarker::none
            ? juce::MouseCursor::NormalCursor : juce::MouseCursor::LeftRightResizeCursor);
        setTooltip(hoverMarker == DragMarker::start
            ? markerDescription("START", region.start)
            : (hoverMarker == DragMarker::end ? markerDescription("END", region.end)
                                               : "Drag the handles to set the playable source region."));
        repaint();
    }
}

void SourceWaveformComponent::mouseExit(const juce::MouseEvent&)
{
    hoverMarker = DragMarker::none;
    setMouseCursor(juce::MouseCursor::NormalCursor);
    setTooltip(source != nullptr
        ? "Drag START and END to set the playable source region."
        : "Select a sample to view and edit its playable region.");
    repaint();
}

void SpectralCanvasComponent::setCanvas(const Canvas& newCanvas)
{
    canvas = newCanvas;
    repaint();
}

void SpectralCanvasComponent::setSpectrum(
    const std::array<float, randomchop::SpectralDrawProcessor::displayBins>& values)
{
    for (size_t index = 0; index < spectrum.size(); ++index)
        spectrum[index] = juce::jmax(values[index], spectrum[index] * 0.80f);
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
    hoverPosition = event.position;
    applyLine(lastCell, lastCell);
}

void SpectralCanvasComponent::mouseDrag(const juce::MouseEvent& event)
{
    const auto next = eventToCell(event);
    applyLine(lastCell, next);
    lastCell = next;
    hoverPosition = event.position;
}

void SpectralCanvasComponent::mouseMove(const juce::MouseEvent& event)
{
    hoverPosition = event.position;
    setMouseCursor(juce::MouseCursor::CrosshairCursor);
    repaint();
}

void SpectralCanvasComponent::mouseExit(const juce::MouseEvent&)
{
    hoverPosition = { -1.0f, -1.0f };
    setMouseCursor(juce::MouseCursor::NormalCursor);
    repaint();
}

void SpectralCanvasComponent::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().reduced(2);
    g.fillAll(juce::Colour(0xffaaaaaa));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff202020),
                                           static_cast<float>(bounds.getX()),
                                           static_cast<float>(bounds.getY()),
                                           juce::Colour(0xff303030),
                                           static_cast<float>(bounds.getX()),
                                           static_cast<float>(bounds.getBottom()), false));
    g.fillRoundedRectangle(bounds.toFloat(), 2.0f);
    g.setColour(juce::Colour(0xffaaaaaa).withAlpha(0.5f));
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
    const auto barWidth = static_cast<float>(bounds.getWidth())
        / static_cast<float>(spectrum.size());
    for (size_t index = 0; index < spectrum.size(); ++index)
    {
        const auto barHeight = std::sqrt(juce::jlimit(0.0f, 1.0f, spectrum[index]))
            * static_cast<float>(bounds.getHeight()) * 0.88f;
        g.setColour(juce::Colour(0xffcccccc).withAlpha(0.62f));
        g.fillRect(static_cast<float>(bounds.getX()) + static_cast<float>(index) * barWidth,
                   static_cast<float>(bounds.getBottom()) - barHeight,
                   juce::jmax(1.0f, barWidth - 1.0f), barHeight);
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
                g.setColour(juce::Colour(0xffeeeeee).withAlpha(0.25f + 0.65f * value));
                g.fillRect(static_cast<float>(bounds.getX()) + column * cellWidth,
                           static_cast<float>(bounds.getY()) + row * cellHeight,
                           cellWidth + 0.5f, cellHeight + 0.5f);
            }
        }
    const auto scannerX = static_cast<float>(bounds.getX())
        + scanPosition * static_cast<float>(bounds.getWidth());
    g.setColour(juce::Colour(0xffffffff).withAlpha(0.18f));
    g.fillRect(scannerX - 5.0f, static_cast<float>(bounds.getY()), 10.0f,
               static_cast<float>(bounds.getHeight()));
    g.setColour(juce::Colour(0xffffffff));
    g.drawVerticalLine(juce::roundToInt(scannerX), static_cast<float>(bounds.getY()),
                       static_cast<float>(bounds.getBottom()));
    g.setColour(juce::Colour(0xffaaaaaa));
    g.drawRoundedRectangle(bounds.toFloat(), 2.0f, 1.0f);
    g.setFont(juce::Font(8.0f, juce::Font::bold));
    g.setColour(juce::Colour(0xffeeeeee).withAlpha(0.78f));
    g.drawText("HIGH", bounds.getX() + 5, bounds.getY() + 3, 30, 10,
               juce::Justification::centredLeft, false);
    g.drawText("LOW", bounds.getX() + 5, bounds.getBottom() - 13, 30, 10,
               juce::Justification::centredLeft, false);
    g.drawText("TIME  >", bounds.getRight() - 47, bounds.getBottom() - 13, 42, 10,
               juce::Justification::centredRight, false);
    if (hoverPosition.x >= static_cast<float>(bounds.getX())
        && hoverPosition.x <= static_cast<float>(bounds.getRight())
        && hoverPosition.y >= static_cast<float>(bounds.getY())
        && hoverPosition.y <= static_cast<float>(bounds.getBottom()))
    {
        g.setColour(juce::Colours::white.withAlpha(0.55f));
        g.drawLine(hoverPosition.x - 5.0f, hoverPosition.y,
                   hoverPosition.x + 5.0f, hoverPosition.y, 1.0f);
        g.drawLine(hoverPosition.x, hoverPosition.y - 5.0f,
                   hoverPosition.x, hoverPosition.y + 5.0f, 1.0f);
    }
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
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff202020), outer.getX(), outer.getY(),
                                           juce::Colour(0xff303030), outer.getX(),
                                           outer.getBottom(), false));
    g.fillRoundedRectangle(outer, 2.0f);
    g.setColour(juce::Colour(0xff888888));
    g.drawRoundedRectangle(outer.reduced(0.5f), 2.0f, 1.2f);
    g.setColour(juce::Colour(0xffaaaaaa).withAlpha(0.20f));
    for (int line = 1; line < 4; ++line)
    {
        const auto y = inner.getY() + static_cast<float>(line) * inner.getHeight() / 4.0f;
        g.drawHorizontalLine(juce::roundToInt(y), inner.getX(), inner.getRight());
    }
    for (int line = 1; line < 8; ++line)
    {
        const auto x = inner.getX() + static_cast<float>(line) * inner.getWidth() / 8.0f;
        g.drawVerticalLine(juce::roundToInt(x), inner.getY(), inner.getBottom());
    }

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
            g.setColour(activeSlice ? juce::Colour(0xffeeeeee).withAlpha(0.42f + 0.48f * primary)
                                    : juce::Colour(0xff555555));
            g.fillRoundedRectangle(cell, 2.0f);
        }
        const auto scanner = inner.getX() + telemetryFirst * inner.getWidth();
        const auto armed = (telemetryFlags & (uint32_t { 1 } << 9)) != 0;
        g.setColour(juce::Colour(0xffffffff).withAlpha(armed ? 0.65f : 1.0f));
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
            g.setColour(juce::Colour(reversed ? 0xffdddddd : 0xffaaaaaa)
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
            g.setColour(juce::Colour(reversed ? 0xffffffff : 0xffcccccc)
                .withAlpha(active ? 0.90f : 0.36f));
            g.strokePath(ribbon, juce::PathStrokeType(1.7f));
        }
        g.setColour(juce::Colour(0xffffffff).withAlpha(active ? 1.0f : 0.45f));
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
        g.setColour(juce::Colour(0xffaaaaaa).withAlpha(0.18f + 0.62f * primary));
        g.drawLine(x - length, y + length * 0.35f, x + length, y - length * 0.35f,
                   1.0f + 1.2f * primary);
        g.setColour(juce::Colour(0xfff5f5f5).withAlpha(0.28f + 0.66f * primary));
        const auto radius = 1.8f - 0.7f * primary;
        g.fillEllipse(x - radius, y - radius, radius * 2.0f, radius * 2.0f);
    }
}

void OutputMeterComponent::setPeak(float newPeak) noexcept
{
    peak = juce::jmax(juce::jlimit(0.0f, 1.0f, newPeak), peak * 0.82f);
    repaint();
}

void OutputMeterComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().reduced(1);
    const auto meter = bounds.removeFromLeft(juce::jmax(12, bounds.getWidth() / 3));
    g.setColour(juce::Colour(0xff1e1e1e));
    g.fillRect(meter);
    const auto db = peak > 0.000001f ? juce::Decibels::gainToDecibels(peak) : -60.0f;
    const auto fraction = juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f);
    const auto segments = 12;
    for (int index = 0; index < segments; ++index)
    {
        const auto y = meter.getBottom() - (index + 1) * meter.getHeight() / segments;
        g.setColour(index < juce::roundToInt(fraction * segments)
            ? juce::Colour(0xffeeeeee) : juce::Colour(0xff555555));
        g.fillRect(meter.getX() + 3, y + 1, meter.getWidth() - 6,
                   juce::jmax(2, meter.getHeight() / segments - 3));
    }
    g.setColour(juce::Colour(0xff222222));
    g.setFont(juce::Font(9.0f));
    const char* labels[] { "0", "-12", "-24", "-36", "-60" };
    for (int index = 0; index < 5; ++index)
    {
        const auto y = bounds.getY() + index * (bounds.getHeight() - 12) / 4;
        g.drawText(labels[index], meter.getRight() + 3, y, bounds.getRight() - meter.getRight(),
                   12, juce::Justification::centredLeft);
    }
}

void XpInfoButton::paintButton(juce::Graphics& g, bool isMouseOverButton,
                               bool isButtonDown)
{
    auto bounds = getLocalBounds().toFloat().reduced(2.0f);
    const auto top = isButtonDown ? juce::Colour(0xff555555)
                                  : (isMouseOverButton ? juce::Colour(0xffaaaaaa)
                                                       : juce::Colour(0xff888888));
    const auto bottom = isButtonDown ? juce::Colour(0xff777777)
                                     : juce::Colour(0xff333333);
    g.setGradientFill(juce::ColourGradient(top, bounds.getCentreX(), bounds.getY(),
                                           bottom, bounds.getCentreX(), bounds.getBottom(),
                                           false));
    g.fillEllipse(bounds);
    g.setColour(juce::Colours::white.withAlpha(0.95f));
    g.drawEllipse(bounds.reduced(0.75f), 1.5f);
    g.setFont(juce::Font(17.0f, juce::Font::bold));
    g.drawText("i", bounds.toNearestInt().translated(0, -1),
               juce::Justification::centred, false);
    if (hasKeyboardFocus(true))
    {
        g.setColour(juce::Colours::white);
        g.drawEllipse(bounds.expanded(1.5f), 1.2f);
    }
}

void XpWindowCloseButton::paintButton(juce::Graphics& g, bool over, bool down)
{
    auto bounds = getLocalBounds().toFloat().reduced(0.7f);
    const auto top = down ? juce::Colour(0xff555555)
                          : (over ? juce::Colour(0xffbbbbbb) : juce::Colour(0xff999999));
    const auto bottom = down ? juce::Colour(0xff777777) : juce::Colour(0xff333333);
    g.setGradientFill(juce::ColourGradient(top, bounds.getCentreX(), bounds.getY(), bottom,
                                           bounds.getCentreX(), bounds.getBottom(), false));
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(juce::Colours::white.withAlpha(0.9f));
    g.drawRoundedRectangle(bounds.reduced(1.0f), 3.0f, 1.0f);
    g.setColour(juce::Colour(0xff222222));
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
    g.setColour(juce::Colours::white);
    g.drawLine(bounds.getX() + 6.0f, bounds.getY() + 6.0f,
               bounds.getRight() - 6.0f, bounds.getBottom() - 6.0f, 2.2f);
    g.drawLine(bounds.getRight() - 6.0f, bounds.getY() + 6.0f,
               bounds.getX() + 6.0f, bounds.getBottom() - 6.0f, 2.2f);
}

void XpModalOverlay::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff111111).withAlpha(0.40f));
}

void XpModalOverlay::mouseDown(const juce::MouseEvent&)
{
    if (onDismiss)
        onDismiss();
}

XpInfoPanel::XpInfoPanel()
{
    setOpaque(true);
    setWantsKeyboardFocus(true);
    addAndMakeVisible(closeButton);
    addAndMakeVisible(titleCloseButton);
    closeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffeeeeee));
    closeButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffcccccc));
    closeButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff222222));
    const auto close = [this]
    {
        if (onClose)
            onClose();
    };
    closeButton.onClick = close;
    titleCloseButton.onClick = close;
    titleCloseButton.setTooltip("Close information window");
}

void XpInfoPanel::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.fillAll(juce::Colour(0xffdddddd));
    g.setColour(juce::Colour(0xff333333));
    g.drawRect(bounds, 3.0f);

    auto titleBar = bounds.reduced(3.0f).removeFromTop(31.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff555555), titleBar.getX(),
                                           titleBar.getY(), juce::Colour(0xff222222),
                                           titleBar.getRight(), titleBar.getY(), false));
    g.fillRect(titleBar);
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(15.0f, juce::Font::bold));
    g.drawText("Information", titleBar.reduced(10.0f, 0.0f).toNearestInt(),
               juce::Justification::centredLeft, false);

    auto icon = juce::Rectangle<float>(25.0f, 58.0f, 42.0f, 42.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xffaaaaaa), icon.getCentreX(),
                                           icon.getY(), juce::Colour(0xff333333),
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
    titleCloseButton.setBounds(getWidth() - 31, 6, 23, 22);
    closeButton.setBounds(getWidth() - 104, getHeight() - 44, 84, 25);
}

bool XpInfoPanel::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (onClose)
            onClose();
        return true;
    }
    return Component::keyPressed(key);
}

namespace
{
class SourceRowControls final : public juce::Component
{
public:
    SourceRowControls()
    {
        setInterceptsMouseClicks(false, true);
        addAndMakeVisible(enabled);
        addAndMakeVisible(preview);
        preview.setButtonText("> ");
        enabled.setTooltip("Include or exclude this source from random selection.");
        preview.setTooltip("Audition this source; click again to stop.");
        enabled.setName("Source enabled");
        preview.setName("Preview source");
    }
    void resized() override
    {
        auto area = getLocalBounds();
        enabled.setBounds(area.removeFromLeft(30).reduced(4, 6));
        preview.setBounds(area.removeFromLeft(27).reduced(2, 5));
    }
    juce::ToggleButton enabled;
    juce::TextButton preview;
    std::function<void(int)> onSelect;
    int row = -1;
};
}

RandomChopSamplerAudioProcessorEditor::RandomChopSamplerAudioProcessorEditor(RandomChopSamplerAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&xpLookAndFeel);
    setOpaque(true);
    setFocusContainerType(juce::Component::FocusContainerType::keyboardFocusContainer);
    setResizable(true, true);
    setResizeLimits(900, 600, 1536, 1024);
    setSize(1024, 683);
    title.setText("recompiler.dll", juce::dontSendNotification);
    title.setFont(juce::Font(25.0f, juce::Font::bold));
    title.setColour(juce::Label::textColourId, juce::Colour(xpInk));
    subtitle.setText("|   RANDOM SAMPLE INSTRUMENT", juce::dontSendNotification);
    subtitle.setFont(juce::Font(12.0f));
    subtitle.setColour(juce::Label::textColourId, juce::Colour(xpInk));
    status.setFont(juce::Font(13.0f));
    status.setColour(juce::Label::textColourId, juce::Colour(xpInk));
    status.setJustificationType(juce::Justification::centred);
    alert.setFont(juce::Font(11.0f, juce::Font::bold));
    alert.setColour(juce::Label::textColourId, juce::Colour(xpInk));
    alert.setJustificationType(juce::Justification::centred);
    alert.setVisible(false);

    juce::Component* components[] = { &title, &subtitle, &alert, &status,
        &pageMessage, &pageActionButton, &infoButton, &list, &waveform, &outputMeter,
        &addButton, &sampleMenuButton, &previousSourceButton, &nextSourceButton, &closeEditorButton,
        &mainTab, &fxTab, &seqTab, &settingsTab, &zoomInButton, &zoomOutButton,
        &focusRegionButton, &fitButton, &randomSourceButton, &regenerateButton,
        &muteButton, &moreButton, &chordsOffButton, &chordsOnButton, &polyButton, &monoButton,
        &scrambleModeButton, &meltModeButton, &smearModeButton,
        &scrambleFoldButton, &meltFoldButton, &smearFoldButton, &spectralFoldButton, &outputFoldButton,
        &scramblePowerButton, &meltPowerButton, &smearPowerButton, &spectralPowerButton,
        &outputPowerButton,
        &sourceKey,
        &sourceTranspose, &sourceFineTune, &sourceGain,
        &sourceKeyLabel,
        &sourceTransposeLabel, &sourceFineTuneLabel, &sourceGainLabel,
        &targetKey, &targetKeyLabel, &midiPitch,
        &voiceMode, &voiceModeLabel,
        &spectralDrawLabel, &spectralResetButton, &spectralCanvas,
        &scrambleVisual, &meltVisual, &smearVisual };
    for (auto* component : components) addAndMakeVisible(component);
    alert.setVisible(false);
    addChildComponent(modalOverlay);
    addChildComponent(infoPanel);
    infoButton.onClick = [this]
    {
        modalOverlay.setVisible(true);
        modalOverlay.toFront(false);
        infoPanel.setVisible(true);
        infoPanel.toFront(true);
        infoPanel.grabKeyboardFocus();
    };
    const auto dismissInfo = [this]
    {
        infoPanel.setVisible(false);
        modalOverlay.setVisible(false);
    };
    infoPanel.onClose = dismissInfo;
    modalOverlay.onDismiss = dismissInfo;
    infoButton.setTooltip("About recompiler.dll");
    infoButton.setVisible(false);
    pageMessage.setJustificationType(juce::Justification::centred);
    pageMessage.setFont(juce::Font(17.0f));
    pageMessage.setColour(juce::Label::textColourId, juce::Colour(xpInk));
    pageMessage.setVisible(false);
    pageActionButton.setVisible(false);
    pageActionButton.onClick = [this]
    {
        if (selectedTab == 3) infoButton.triggerClick();
        else selectTab(0);
    };
    list.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xffeeeeee));
    list.setColour(juce::ListBox::outlineColourId, juce::Colour(0xff555555));
    list.setOutlineThickness(1);
    list.setRowHeight(36);
    list.setTooltip("Drop WAV, AIFF, MP3, or FLAC files here");
    list.setName("Sample sources");
    list.setHelpText("Select a source to edit it. Use the checkbox to include it, the triangle to audition, or the menu to remove it.");

    addButton.setTooltip("Choose WAV, AIFF, MP3, or FLAC files to add to the pool.");
    sampleMenuButton.setTooltip("Sample actions, including Remove selected.");
    previousSourceButton.setTooltip("Select previous source.");
    nextSourceButton.setTooltip("Select next source.");
    closeEditorButton.setTooltip("Hide this editor; reopen it from the host.");
    zoomInButton.setTooltip("Zoom into the waveform.");
    zoomOutButton.setTooltip("Zoom out of the waveform.");
    focusRegionButton.setTooltip("Focus on the playable region.");
    fitButton.setTooltip("Fit the whole source waveform.");
    randomSourceButton.setTooltip("Select a random loaded source for editing.");
    regenerateButton.setTooltip("Generate a new internal creative seed.");
    muteButton.setTooltip("Temporarily mute or unmute the plugin output.");
    moreButton.setTooltip("Output actions.");
    addButton.onClick = [this] { openFileChooser(); };
    sampleMenuButton.onClick = [this] { showSampleMenu(); };
    previousSourceButton.onClick = [this] { selectRelativeSource(-1); };
    nextSourceButton.onClick = [this] { selectRelativeSource(1); };
    closeEditorButton.onClick = [this]
    {
        setVisible(false);
    };
    zoomInButton.onClick = [this] { waveform.zoomIn(); };
    zoomOutButton.onClick = [this] { waveform.zoomOut(); };
    focusRegionButton.onClick = [this] { waveform.focusRegion(); };
    fitButton.onClick = [this] { waveform.fitAll(); };
    randomSourceButton.onClick = [this]
    {
        if (displayPool && !displayPool->empty())
            list.selectRow(juce::Random::getSystemRandom().nextInt(
                static_cast<int>(displayPool->size())));
    };
    regenerateButton.onClick = [this] { processor.regenerateCreativeSeed(); };
    muteButton.onClick = [this] { processor.setOutputMuted(!processor.isOutputMuted()); };
    moreButton.onClick = [this]
    {
        juce::PopupMenu menu;
        menu.addItem(1, "Reset output to 0 dB");
        menu.addItem(2, "About recompiler.dll");
        juce::Component::SafePointer<RandomChopSamplerAudioProcessorEditor> safe(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&moreButton),
            [safe](int choice)
            {
                if (safe == nullptr) return;
                if (choice == 1) safe->output.setValue(0.0);
                if (choice == 2) safe->infoButton.triggerClick();
            });
    };
    chordsOffButton.onClick = [this] { if (midiPitch.getToggleState()) midiPitch.triggerClick(); };
    chordsOnButton.onClick = [this] { if (!midiPitch.getToggleState()) midiPitch.triggerClick(); };
    polyButton.onClick = [this] { if (voiceMode.getToggleState()) voiceMode.triggerClick(); };
    monoButton.onClick = [this] { if (!voiceMode.getToggleState()) voiceMode.triggerClick(); };
    mainTab.onClick = [this] { selectTab(0); };
    fxTab.onClick = [this] { selectTab(1); };
    seqTab.onClick = [this] { selectTab(2); };
    settingsTab.onClick = [this] { selectTab(3); };
    scrambleModeButton.onClick = [this] { showEffectModeMenu(0); };
    meltModeButton.onClick = [this] { showEffectModeMenu(1); };
    smearModeButton.onClick = [this] { showEffectModeMenu(2); };
    juce::TextButton* foldButtons[] { &scrambleFoldButton, &meltFoldButton,
        &smearFoldButton, &spectralFoldButton, &outputFoldButton };
    for (int index = 0; index < 5; ++index)
        foldButtons[index]->onClick = [this, index]
        {
            expandedCards[index] = !expandedCards[index];
            resized();
            repaint();
        };
    for (auto* button : foldButtons)
        button->setName("Fold card");
    juce::TextButton* powerButtons[] { &scramblePowerButton, &meltPowerButton,
        &smearPowerButton, &spectralPowerButton, &outputPowerButton };
    for (int index = 0; index < 5; ++index)
        powerButtons[index]->onClick = [this, index]
        {
            if (index == 4) processor.setOutputMuted(!processor.isOutputMuted());
            else processor.setEffectEnabled(index, !processor.isEffectEnabled(index));
            repaint();
        };
    for (auto* button : powerButtons)
        button->setName("Effect power");

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
    for (auto* slider : { &sourceTranspose, &sourceFineTune })
    {
        slider->setSliderStyle(juce::Slider::IncDecButtons);
        slider->setIncDecButtonsMode(juce::Slider::incDecButtonsDraggable_Vertical);
        slider->setTextBoxStyle(juce::Slider::TextBoxLeft, false, 64, 22);
    }
    sourceGain.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    sourceGain.setTextBoxStyle(juce::Slider::TextBoxRight, false, 58, 22);
    sourceGain.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff222222));
    sourceKey.setTooltip("Set the detected or known musical key of the selected source.");
    sourceTranspose.setTooltip("Shift the selected source by whole semitones.");
    sourceFineTune.setTooltip("Correct the selected source in cents.");
    sourceGain.setTooltip("Balance the selected source before global effects.");
    targetKey.setTooltip("Choose the shared musical key used to align all sources.");
    sourceKey.setName("Source key");
    sourceTranspose.setName("Source transpose");
    sourceFineTune.setName("Source fine tune");
    sourceGain.setName("Source gain");
    targetKey.setName("Play in key");
    sourceKeyLabel.setText("SOURCE KEY", juce::dontSendNotification);
    sourceTransposeLabel.setText("TRANSPOSE", juce::dontSendNotification);
    sourceFineTuneLabel.setText("FINE TUNE", juce::dontSendNotification);
    sourceGainLabel.setText("GAIN", juce::dontSendNotification);
    targetKeyLabel.setText("PLAY IN KEY", juce::dontSendNotification);
    voiceModeLabel.setText("POLY / MONO", juce::dontSendNotification);
    spectralDrawLabel.setText("SPECTRAL DRAW", juce::dontSendNotification);
    midiPitch.setTooltip("Off keeps every trigger in Play In Key; on follows MIDI notes for chords");
    voiceMode.setTooltip("POLY overlaps held notes; MONO cuts the previous voice");
    spectralResetButton.setTooltip("Clear the entire spectral drawing.");
    spectralCanvas.setTooltip("Drag to draw attenuation into the time/frequency canvas.");
    scrambleVisual.setTooltip("Live view of Scramble's slice rearrangement and scan position.");
    meltVisual.setTooltip("Live view of Melt's stretched slices and reversed segments.");
    smearVisual.setTooltip("Live view of Smear's active pitched-grain cloud.");
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
    configureKnob(spectralDepth, spectralDepthLabel, "SPECTRAL DEPTH");
    configureKnob(smearAmount, smearAmountLabel, "SMEAR");
    scrambleAmount.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff222222));
    meltAmount.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff222222));
    smearAmount.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff222222));
    output.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff222222));
    spectralDepth.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff333333));
    scrambleAmount.setTooltip("Increase rhythmic rearrangement, repeats, reversals, and octave gestures.");
    meltAmount.setTooltip("Increase automatic slice density and pitch-preserving time stretch.");
    smearAmount.setTooltip("Increase the density and brightness of progressively smaller pitched grains.");
    spectralDepth.setTooltip("Control how strongly the spectral drawing attenuates the signal.");
    output.setTooltip("Set the final plug-in output level.");
    scrambleAmount.setName("Scramble amount");
    meltAmount.setName("Melt amount");
    smearAmount.setName("Smear amount");
    spectralDepth.setName("Spectral depth");
    output.setName("Output level");
    midiPitch.setName("Chords mode");
    voiceMode.setName("Voice mode");

    int focusOrder = 1;
    for (auto* component : { static_cast<juce::Component*>(&list),
                             static_cast<juce::Component*>(&sourceKey),
                             static_cast<juce::Component*>(&sourceTranspose),
                             static_cast<juce::Component*>(&sourceFineTune),
                             static_cast<juce::Component*>(&sourceGain),
                             static_cast<juce::Component*>(&targetKey),
                             static_cast<juce::Component*>(&midiPitch),
                             static_cast<juce::Component*>(&voiceMode),
                             static_cast<juce::Component*>(&scrambleAmount),
                             static_cast<juce::Component*>(&meltAmount),
                             static_cast<juce::Component*>(&smearAmount),
                             static_cast<juce::Component*>(&spectralDepth),
                             static_cast<juce::Component*>(&spectralResetButton),
                             static_cast<juce::Component*>(&output),
                             static_cast<juce::Component*>(&infoButton) })
    {
        component->setWantsKeyboardFocus(true);
        component->setExplicitFocusOrder(focusOrder++);
    }
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

    tooltipWindow = std::make_unique<juce::TooltipWindow>(this, 550);
    selectTab(0);
    refresh();
    startTimerHz(20);
}

RandomChopSamplerAudioProcessorEditor::~RandomChopSamplerAudioProcessorEditor()
{
    tooltipWindow.reset();
    setLookAndFeel(nullptr);
}

void RandomChopSamplerAudioProcessorEditor::configureKnob(juce::Slider& slider, juce::Label& label,
                                                           const juce::String& text)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 54, 20);
    slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff222222));
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
    slider.setColour(juce::Slider::trackColourId, juce::Colour(0xff222222));
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colour(xpInk));
    addAndMakeVisible(slider);
    addAndMakeVisible(label);
}

void RandomChopSamplerAudioProcessorEditor::paint(juce::Graphics& g)
{
    const auto full = getLocalBounds().toFloat();
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xffefefef), full.getX(), full.getY(),
                                           juce::Colour(0xffcacaca), full.getX(),
                                           full.getBottom(), false));
    g.fillRect(full);
    g.setColour(juce::Colour(0xff666666));
    g.drawRect(getLocalBounds().reduced(1), 2);
    const auto bar = titleBarBounds.toFloat();
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xfff7f7f7), bar.getX(), bar.getY(),
                                           juce::Colour(0xffd1d1d1), bar.getX(),
                                           bar.getBottom(), false));
    g.fillRect(bar);
    g.setColour(juce::Colour(0xff555555));
    g.drawHorizontalLine(titleBarBounds.getBottom() - 1, bar.getX(), bar.getRight());

    auto icon = juce::Rectangle<float>(12.0f, bar.getY() + 7.0f, 56.0f, 39.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colours::white, icon.getX(), icon.getY(),
                                           juce::Colour(0xffbbbbbb), icon.getX(),
                                           icon.getBottom(), false));
    g.fillRoundedRectangle(icon, 2.0f);
    g.setColour(juce::Colour(0xff222222));
    g.drawRoundedRectangle(icon, 2.0f, 1.0f);
    juce::Path miniWave;
    for (int index = 0; index <= 16; ++index)
    {
        const auto unit = static_cast<float>(index) / 16.0f;
        const auto x = icon.getX() + 3.0f + unit * (icon.getWidth() - 6.0f);
        const auto y = icon.getCentreY() + std::sin(unit * 6.0f * juce::MathConstants<float>::pi)
            * (2.0f + 5.0f * std::sin(unit * juce::MathConstants<float>::pi));
        if (index == 0) miniWave.startNewSubPath(x, y); else miniWave.lineTo(x, y);
    }
    g.strokePath(miniWave, juce::PathStrokeType(2.0f));

    auto statusPlate = status.getBounds().expanded(1, 1).toFloat();
    g.setColour(juce::Colour(0xfff1f1f1));
    g.fillRect(statusPlate);
    g.setColour(juce::Colour(0xff333333));
    g.drawRect(statusPlate, 1.0f);
    g.setFont(juce::Font(11.0f));
    g.drawText("v3.0.0", closeEditorButton.getX() - 43, closeEditorButton.getY(),
               40, closeEditorButton.getHeight(), juce::Justification::centred);
    g.setColour(juce::Colour(0xff444444));
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 7; ++column)
            g.fillEllipse(static_cast<float>(mainTab.getX() - 34 + column * 5),
                          static_cast<float>(8 + row * 6), 2.0f, 2.0f);

    if (alert.isVisible())
    {
        auto alertPlate = alert.getBounds().expanded(4, 1).toFloat();
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xffeeeeee), alertPlate.getX(),
                                               alertPlate.getY(), juce::Colour(0xffaaaaaa),
                                               alertPlate.getX(), alertPlate.getBottom(), false));
        g.fillRoundedRectangle(alertPlate, 2.0f);
        g.setColour(juce::Colour(0xff333333));
        g.drawRoundedRectangle(alertPlate, 2.0f, 1.0f);
    }

    if (selectedTab == 0)
    {
        paintXpPanel(g, samplePanelBounds, "SAMPLES");
        paintXpPanel(g, sourcePanelBounds);
        paintXpPanel(g, globalPanelBounds);
        auto globalTag = globalPanelBounds.reduced(5).removeFromLeft(85).toFloat();
        g.setColour(juce::Colour(0xff242424));
        g.fillRoundedRectangle(globalTag, 2.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(14.0f, juce::Font::bold));
        g.drawText("GLOBAL", globalTag.toNearestInt(), juce::Justification::centred);
        g.setColour(juce::Colour(0xff222222));
        g.setFont(juce::Font(12.0f, juce::Font::bold));
        g.drawText("CHORDS", chordsOffButton.getX() - 57, chordsOffButton.getY(),
                   55, chordsOffButton.getHeight(), juce::Justification::centred);
        g.setColour(juce::Colour(0xff777777));
        for (const auto x : { sourceTransposeLabel.getX() - 4,
                              sourceFineTuneLabel.getX() - 4, sourceGainLabel.getX() - 4 })
            g.drawVerticalLine(x, static_cast<float>(sourceKeyLabel.getY()),
                               static_cast<float>(sourcePanelBounds.getBottom() - 9));
        if (list.hasKeyboardFocus(true))
        {
            g.setColour(juce::Colour(0xff222222));
            g.drawRect(list.getBounds().expanded(2), 1);
        }
    }
    if (selectedTab <= 1)
    {
        paintXpPanel(g, scramblePanelBounds, "   SCRAMBLE");
        paintXpPanel(g, meltPanelBounds, "   MELT");
        paintXpPanel(g, smearPanelBounds, "   SMEAR");
        paintXpPanel(g, spectralPanelBounds, "   SPECTRAL DRAW");
        paintXpPanel(g, outputPanelBounds, "   OUTPUT");
        g.setColour(juce::Colour(0xff222222));
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        for (const auto panel : { scramblePanelBounds, meltPanelBounds, smearPanelBounds })
            g.drawText("MODE", panel.getX() + 9, panel.getBottom() - 32, 48, 24,
                       juce::Justification::centredLeft);
    }
    if (selectedTab >= 2)
        paintXpPanel(g, pagePaneBounds);

    if (selectedTab == 0 && !sampleDropBounds.isEmpty())
    {
        const auto drop = sampleDropBounds.toFloat();
        g.setColour(dragActive ? juce::Colour(0xffeeeeee) : juce::Colour(0xffdcdcdc));
        g.fillRoundedRectangle(drop, 2.0f);
        g.setColour(juce::Colour(0xff333333));
        const float dashes[] { 5.0f, 4.0f };
        g.drawDashedLine({ drop.getX(), drop.getY(), drop.getRight(), drop.getY() },
                         dashes, 2, 1.0f);
        g.drawDashedLine({ drop.getX(), drop.getBottom(), drop.getRight(), drop.getBottom() },
                         dashes, 2, 1.0f);
        const auto arrowX = drop.getX() + 13.0f;
        const auto arrowY = drop.getCentreY();
        juce::Path arrow;
        arrow.startNewSubPath(arrowX, arrowY - 7.0f);
        arrow.lineTo(arrowX, arrowY + 3.0f);
        arrow.lineTo(arrowX - 4.0f, arrowY - 1.0f);
        arrow.startNewSubPath(arrowX, arrowY + 3.0f);
        arrow.lineTo(arrowX + 4.0f, arrowY - 1.0f);
        g.strokePath(arrow, juce::PathStrokeType(1.6f));
        g.drawHorizontalLine(juce::roundToInt(arrowY + 6.0f), arrowX - 6.0f, arrowX + 6.0f);
        g.setColour(juce::Colour(0xff333333));
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        g.drawFittedText(dragActive ? "RELEASE TO ADD SAMPLES" : "DRAG & DROP SAMPLES  /  WAV, AIFF, MP3 OR FLAC",
                         sampleDropBounds.reduced(28, 5),
                         juce::Justification::centred, 1);
    }

    g.setColour(juce::Colour(0xffececec));
    g.fillRect(footerBounds);
    g.setColour(juce::Colour(0xff555555));
    g.drawHorizontalLine(footerBounds.getY(), 1.0f, static_cast<float>(getWidth() - 1));
    g.setColour(juce::Colour(0xff222222));
    g.setFont(juce::Font(10.0f));
    g.drawText("RECOMPILER.DLL", footerBounds.withTrimmedLeft(55).removeFromLeft(170),
               juce::Justification::centredLeft);
    g.drawText("SAMPLES FIND NEW MEANINGS", footerBounds,
               juce::Justification::centred);
    g.drawText("v3.0.0", footerBounds.withTrimmedRight(50).removeFromRight(45),
               juce::Justification::centredRight);
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 5; ++column)
        {
            g.fillEllipse(static_cast<float>(13 + column * 6),
                          static_cast<float>(footerBounds.getY() + 4 + row * 6), 2.0f, 2.0f);
            g.fillEllipse(static_cast<float>(getWidth() - 42 + column * 6),
                          static_cast<float>(footerBounds.getY() + 4 + row * 6), 2.0f, 2.0f);
        }
    g.setColour(juce::Colour(0xff222222));
    g.drawRoundedRectangle(full.reduced(1.0f), 4.0f, 1.4f);
}

void RandomChopSamplerAudioProcessorEditor::resized()
{
    constexpr int gap = 6;
    const auto verticalScale = juce::jlimit(0.86f, 1.5f,
        static_cast<float>(getHeight()) / 683.0f);
    const auto scaled = [verticalScale](int value)
    {
        return juce::jmax(1, juce::roundToInt(static_cast<float>(value) * verticalScale));
    };

    titleBarBounds = getLocalBounds().removeFromTop(scaled(64));
    footerBounds = getLocalBounds().removeFromBottom(scaled(25));
    auto header = titleBarBounds.reduced(8, 5).removeFromTop(scaled(34));
    closeEditorButton.setBounds(header.removeFromRight(26).reduced(1));
    header.removeFromRight(8);
    header.removeFromRight(37); // version text is painted in the title bar.
    nextSourceButton.setBounds(header.removeFromRight(25).reduced(1));
    status.setBounds(header.removeFromRight(156).reduced(1));
    previousSourceButton.setBounds(header.removeFromRight(25).reduced(1));
    header.removeFromRight(5);
    alert.setBounds(header.removeFromRight(juce::jmin(150, header.getWidth() / 3)).reduced(3));
    auto brand = header;
    brand.removeFromLeft(67);
    title.setBounds(brand.removeFromLeft(juce::jmin(225, brand.getWidth() / 2)));
    subtitle.setBounds(brand);
    auto tabs = titleBarBounds.withTrimmedTop(scaled(38)).reduced(8, 2);
    const auto tabWidth = juce::jmin(94, juce::jmax(60, getWidth() / 10));
    settingsTab.setBounds(tabs.removeFromRight(tabWidth).reduced(2, 0));
    seqTab.setBounds(tabs.removeFromRight(tabWidth).reduced(2, 0));
    fxTab.setBounds(tabs.removeFromRight(tabWidth).reduced(2, 0));
    mainTab.setBounds(tabs.removeFromRight(tabWidth).reduced(2, 0));
    infoButton.setBounds(0, 0, 0, 0);

    auto area = getLocalBounds().withTrimmedTop(titleBarBounds.getHeight())
        .withTrimmedBottom(footerBounds.getHeight()).reduced(8, 6);
    pagePaneBounds = area;
    const auto topHeight = juce::jlimit(scaled(258), scaled(430),
                                        juce::roundToInt(area.getHeight() * 0.50f));
    auto topRow = area.removeFromTop(topHeight);
    area.removeFromTop(gap);
    auto globalRow = area.removeFromTop(scaled(48));
    area.removeFromTop(gap);
    auto bottomRow = area;
    if (selectedTab == 1)
        bottomRow = pagePaneBounds;

    samplePanelBounds = topRow.removeFromLeft(juce::roundToInt(topRow.getWidth() * 0.29f));
    topRow.removeFromLeft(gap);
    sourcePanelBounds = topRow;

    auto sampleHeader = samplePanelBounds.reduced(6).removeFromTop(25);
    sampleMenuButton.setBounds(sampleHeader.removeFromRight(24).reduced(1));
    addButton.setBounds(sampleHeader.removeFromRight(78).reduced(2, 1));
    auto sampleContent = samplePanelBounds.reduced(7).withTrimmedTop(29);
    sampleDropBounds = sampleContent.removeFromBottom(scaled(38)).reduced(2, 4);
    list.setBounds(sampleContent.reduced(1));
    list.setRowHeight(juce::jlimit(29, 52, juce::jmax(1, list.getHeight() / 6)));

    auto sourceContent = sourcePanelBounds.reduced(7);
    auto sourceControlRow = sourceContent.removeFromBottom(scaled(72));
    waveform.setBounds(sourceContent.reduced(1));
    auto waveTools = waveform.getBounds().reduced(5).removeFromRight(40);
    waveTools.removeFromTop(26);
    const auto toolHeight = juce::jmax(23, waveTools.getHeight() / 4);
    zoomInButton.setBounds(waveTools.removeFromTop(toolHeight).reduced(2));
    zoomOutButton.setBounds(waveTools.removeFromTop(toolHeight).reduced(2));
    focusRegionButton.setBounds(waveTools.removeFromTop(toolHeight).reduced(2));
    fitButton.setBounds(waveTools.reduced(2));
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
    auto globalContent = globalPanelBounds.reduced(7);
    globalContent.removeFromLeft(93);
    regenerateButton.setBounds(globalContent.removeFromRight(36).reduced(2));
    randomSourceButton.setBounds(globalContent.removeFromRight(36).reduced(2));
    auto keyCell = globalContent.removeFromLeft(juce::jmax(205, globalContent.getWidth() / 3));
    targetKeyLabel.setBounds(keyCell.removeFromLeft(82));
    targetKey.setBounds(keyCell.reduced(2, 1));
    auto chordsCell = globalContent.removeFromLeft(globalContent.getWidth() / 2);
    chordsCell.removeFromLeft(57);
    const auto halfChords = chordsCell.getWidth() / 2;
    chordsOffButton.setBounds(chordsCell.removeFromLeft(halfChords).reduced(1));
    chordsOnButton.setBounds(chordsCell.reduced(1));
    auto voicesCell = globalContent;
    voiceModeLabel.setBounds(voicesCell.removeFromLeft(88));
    const auto halfVoices = voicesCell.getWidth() / 2;
    polyButton.setBounds(voicesCell.removeFromLeft(halfVoices).reduced(1));
    monoButton.setBounds(voicesCell.reduced(1));
    midiPitch.setBounds(0, 0, 0, 0);
    voiceMode.setBounds(0, 0, 0, 0);

    const auto available = bottomRow.getWidth() - gap * 4;
    const auto effectWidth = juce::roundToInt(static_cast<float>(available) * 0.19f);
    const auto spectralWidth = juce::roundToInt(static_cast<float>(available) * 0.29f);
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
                                        juce::Slider& slider, CreativeVisualizer& visual,
                                        juce::TextButton& mode, juce::TextButton& fold,
                                        juce::TextButton& power)
    {
        label.setBounds(0, 0, 0, 0);
        fold.setBounds(panel.getX() + 5, panel.getY() + 4, 19, 19);
        power.setBounds(panel.getRight() - 27, panel.getY() + 5, 18, 18);
        auto content = panel.reduced(7).withTrimmedTop(25);
        auto modeRow = content.removeFromBottom(scaled(27));
        mode.setBounds(modeRow.removeFromRight(juce::jmax(92, modeRow.getWidth() - 55)).reduced(1));
        auto knobArea = content.removeFromTop(juce::jmin(scaled(95), content.getHeight() / 2));
        slider.setBounds(knobArea.withSizeKeepingCentre(juce::jmin(114, knobArea.getWidth()),
                                                        knobArea.getHeight()));
        visual.setBounds(content.reduced(1, 2));
    };
    layoutEffect(scramblePanelBounds, scrambleAmountLabel, scrambleAmount, scrambleVisual,
                 scrambleModeButton, scrambleFoldButton, scramblePowerButton);
    layoutEffect(meltPanelBounds, meltAmountLabel, meltAmount, meltVisual,
                 meltModeButton, meltFoldButton, meltPowerButton);
    layoutEffect(smearPanelBounds, smearAmountLabel, smearAmount, smearVisual,
                 smearModeButton, smearFoldButton, smearPowerButton);

    spectralDrawLabel.setBounds(0, 0, 0, 0);
    spectralFoldButton.setBounds(spectralPanelBounds.getX() + 5, spectralPanelBounds.getY() + 4, 19, 19);
    spectralPowerButton.setBounds(spectralPanelBounds.getRight() - 27, spectralPanelBounds.getY() + 5, 18, 18);
    auto spectralContent = spectralPanelBounds.reduced(7).withTrimmedTop(25);
    auto spectralTools = spectralContent.removeFromBottom(scaled(58));
    spectralCanvas.setBounds(spectralContent.reduced(1, 2));
    auto depthArea = spectralTools.reduced(4, 3);
    spectralResetButton.setBounds(depthArea.removeFromRight(68).reduced(2, 10));
    spectralDepthLabel.setBounds(depthArea.removeFromLeft(112));
    spectralDepth.setBounds(depthArea.removeFromLeft(77));

    outputLabel.setBounds(0, 0, 0, 0);
    outputFoldButton.setBounds(outputPanelBounds.getX() + 5, outputPanelBounds.getY() + 4, 19, 19);
    outputPowerButton.setBounds(outputPanelBounds.getRight() - 27, outputPanelBounds.getY() + 5, 18, 18);
    auto outputContent = outputPanelBounds.reduced(7).withTrimmedTop(25);
    auto outputActions = outputContent.removeFromBottom(scaled(30));
    muteButton.setBounds(outputActions.removeFromLeft(outputActions.getWidth() / 2).reduced(2));
    moreButton.setBounds(outputActions.reduced(2));
    outputMeter.setBounds(outputContent.removeFromRight(35).reduced(1, 4));
    output.setBounds(outputContent.withSizeKeepingCentre(
        juce::jmin(105, outputContent.getWidth()), juce::jmin(138, outputContent.getHeight())));

    const auto dialogWidth = juce::jmin(350, getWidth() - 48);
    const auto dialogHeight = juce::jmin(180, getHeight() - 48);
    modalOverlay.setBounds(getLocalBounds());
    infoPanel.setBounds(getLocalBounds().withSizeKeepingCentre(dialogWidth, dialogHeight));

    const auto onMain = selectedTab == 0;
    const auto onEffects = selectedTab == 0 || selectedTab == 1;
    for (auto* component : { static_cast<juce::Component*>(&list),
                             static_cast<juce::Component*>(&waveform),
                             static_cast<juce::Component*>(&addButton),
                             static_cast<juce::Component*>(&sampleMenuButton),
                             static_cast<juce::Component*>(&zoomInButton),
                             static_cast<juce::Component*>(&zoomOutButton),
                             static_cast<juce::Component*>(&focusRegionButton),
                             static_cast<juce::Component*>(&fitButton),
                             static_cast<juce::Component*>(&sourceKey),
                             static_cast<juce::Component*>(&sourceTranspose),
                             static_cast<juce::Component*>(&sourceFineTune),
                             static_cast<juce::Component*>(&sourceGain),
                             static_cast<juce::Component*>(&sourceKeyLabel),
                             static_cast<juce::Component*>(&sourceTransposeLabel),
                             static_cast<juce::Component*>(&sourceFineTuneLabel),
                             static_cast<juce::Component*>(&sourceGainLabel),
                             static_cast<juce::Component*>(&targetKey),
                             static_cast<juce::Component*>(&targetKeyLabel),
                             static_cast<juce::Component*>(&voiceModeLabel),
                             static_cast<juce::Component*>(&chordsOffButton),
                             static_cast<juce::Component*>(&chordsOnButton),
                             static_cast<juce::Component*>(&polyButton),
                             static_cast<juce::Component*>(&monoButton),
                             static_cast<juce::Component*>(&randomSourceButton),
                             static_cast<juce::Component*>(&regenerateButton) })
        component->setVisible(onMain);
    for (auto* component : { static_cast<juce::Component*>(&scrambleAmount),
                             static_cast<juce::Component*>(&meltAmount),
                             static_cast<juce::Component*>(&smearAmount),
                             static_cast<juce::Component*>(&spectralDepth),
                             static_cast<juce::Component*>(&spectralResetButton),
                             static_cast<juce::Component*>(&output),
                             static_cast<juce::Component*>(&scrambleFoldButton),
                             static_cast<juce::Component*>(&meltFoldButton),
                             static_cast<juce::Component*>(&smearFoldButton),
                             static_cast<juce::Component*>(&spectralFoldButton),
                             static_cast<juce::Component*>(&outputFoldButton),
                             static_cast<juce::Component*>(&scramblePowerButton),
                             static_cast<juce::Component*>(&meltPowerButton),
                             static_cast<juce::Component*>(&smearPowerButton),
                             static_cast<juce::Component*>(&spectralPowerButton),
                             static_cast<juce::Component*>(&outputPowerButton) })
        component->setVisible(onEffects);
    scrambleVisual.setVisible(onEffects && expandedCards[0]);
    meltVisual.setVisible(onEffects && expandedCards[1]);
    smearVisual.setVisible(onEffects && expandedCards[2]);
    scrambleModeButton.setVisible(onEffects && expandedCards[0]);
    meltModeButton.setVisible(onEffects && expandedCards[1]);
    smearModeButton.setVisible(onEffects && expandedCards[2]);
    spectralCanvas.setVisible(onEffects && expandedCards[3]);
    spectralDepth.setVisible(onEffects && expandedCards[3]);
    spectralDepthLabel.setVisible(onEffects && expandedCards[3]);
    spectralResetButton.setVisible(onEffects && expandedCards[3]);
    outputMeter.setVisible(onEffects && expandedCards[4]);
    muteButton.setVisible(onEffects && expandedCards[4]);
    moreButton.setVisible(onEffects && expandedCards[4]);
    midiPitch.setVisible(false);
    voiceMode.setVisible(false);
    pageMessage.setBounds(pagePaneBounds.reduced(40).withTrimmedBottom(52));
    pageActionButton.setBounds(pagePaneBounds.withSizeKeepingCentre(155, 34)
        .translated(0, pagePaneBounds.getHeight() / 4));
    pageMessage.setVisible(selectedTab >= 2);
    pageActionButton.setVisible(selectedTab >= 2);
}

bool RandomChopSamplerAudioProcessorEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& path : files) if (SampleManager::isSupported(juce::File(path))) return true;
    return false;
}

void RandomChopSamplerAudioProcessorEditor::fileDragEnter(const juce::StringArray& files,
                                                           int, int)
{
    dragActive = isInterestedInFileDrag(files);
    repaint(sampleDropBounds);
}

void RandomChopSamplerAudioProcessorEditor::fileDragExit(const juce::StringArray&)
{
    dragActive = false;
    repaint(sampleDropBounds);
}

void RandomChopSamplerAudioProcessorEditor::filesDropped(const juce::StringArray& files, int, int)
{
    dragActive = false;
    addFiles(files);
}

void RandomChopSamplerAudioProcessorEditor::addFiles(const juce::StringArray& files)
{
    const auto errors = processor.samples.addFiles(files);
    transientMessage = errors.empty() ? juce::String()
                                      : juce::String(errors.size()) + " file(s) rejected";
    transientMessageTicks = errors.empty() ? 0 : 60;
    refresh();
}

void RandomChopSamplerAudioProcessorEditor::openFileChooser()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Add samples", juce::File {}, "*.wav;*.aif;*.aiff;*.mp3;*.flac", true);
    juce::Component::SafePointer<RandomChopSamplerAudioProcessorEditor> safe(this);
    fileChooser->launchAsync(juce::FileBrowserComponent::openMode
        | juce::FileBrowserComponent::canSelectFiles
        | juce::FileBrowserComponent::canSelectMultipleItems,
        [safe](const juce::FileChooser& chooser)
        {
            if (safe == nullptr) return;
            juce::StringArray paths;
            for (const auto& file : chooser.getResults())
                paths.add(file.getFullPathName());
            if (!paths.isEmpty()) safe->addFiles(paths);
        });
}

void RandomChopSamplerAudioProcessorEditor::selectRelativeSource(int delta)
{
    const auto count = getNumRows();
    if (count == 0) return;
    const auto current = list.getSelectedRow();
    const auto next = current < 0 ? 0 : (current + delta + count) % count;
    list.selectRow(next);
    list.scrollToEnsureRowIsOnscreen(next);
}

void RandomChopSamplerAudioProcessorEditor::showSampleMenu()
{
    juce::PopupMenu menu;
    const auto hasSelection = selectedSourceId.isNotEmpty();
    menu.addItem(1, "Remove selected source", hasSelection);
    menu.addItem(2, "Reveal selected file", hasSelection);
    menu.addSeparator();
    menu.addItem(3, "About recompiler.dll");
    juce::Component::SafePointer<RandomChopSamplerAudioProcessorEditor> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&sampleMenuButton),
        [safe](int choice)
        {
            if (safe == nullptr) return;
            if (choice == 1 && safe->selectedSourceId.isNotEmpty())
            {
                safe->processor.requestSourcePreview(0);
                safe->processor.samples.remove(safe->selectedSourceId);
                safe->refresh();
            }
            else if (choice == 2 && safe->displayPool)
            {
                for (const auto& source : *safe->displayPool)
                    if (source->settings.id == safe->selectedSourceId)
                        juce::File(source->settings.filePath).revealToUser();
            }
            else if (choice == 3) safe->infoButton.triggerClick();
        });
}

void RandomChopSamplerAudioProcessorEditor::showEffectModeMenu(int effect)
{
    struct Option { const char* name; uint32_t flag; };
    const Option scramble[] {
        { "Octave pitch", randomchop::ScrambleFeatures::pitch },
        { "Hold and micro-loop", randomchop::ScrambleFeatures::hold },
        { "Reverse", randomchop::ScrambleFeatures::reverse },
        { "Jump and reorder", randomchop::ScrambleFeatures::jump },
        { "Motif repeats", randomchop::ScrambleFeatures::motif }
    };
    const Option melt[] {
        { "Time stretch", randomchop::MeltFeatures::stretch },
        { "Reverse", randomchop::MeltFeatures::reverse },
        { "Slice variation", randomchop::MeltFeatures::sliceVariation }
    };
    const Option smear[] {
        { "Pitched intervals", randomchop::SmearFeatures::pitch },
        { "Time scatter", randomchop::SmearFeatures::scatter },
        { "Pitch and pan motion", randomchop::SmearFeatures::orbit },
        { "Stereo spread", randomchop::SmearFeatures::stereo },
        { "Bright particles", randomchop::SmearFeatures::brightness },
        { "Feedback", randomchop::SmearFeatures::feedback }
    };
    const Option* options = effect == 0 ? scramble : effect == 1 ? melt : smear;
    const auto optionCount = effect == 0 ? 5 : effect == 1 ? 3 : 6;
    const auto all = effect == 0 ? randomchop::ScrambleFeatures::all
        : effect == 1 ? randomchop::MeltFeatures::all : randomchop::SmearFeatures::all;
    const auto current = effect == 0 ? processor.getScrambleFeatures()
        : effect == 1 ? processor.getMeltFeatures() : processor.getSmearFeatures();
    juce::PopupMenu menu;
    menu.addItem(100, "All gestures", true, current == all);
    menu.addSeparator();
    for (int index = 0; index < optionCount; ++index)
        menu.addItem(index + 1, options[index].name, true,
                     (current & options[index].flag) != 0);
    juce::Component* target = effect == 0 ? static_cast<juce::Component*>(&scrambleModeButton)
        : effect == 1 ? static_cast<juce::Component*>(&meltModeButton)
                      : static_cast<juce::Component*>(&smearModeButton);
    juce::Component::SafePointer<RandomChopSamplerAudioProcessorEditor> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(target),
        [safe, effect, current, all](int choice)
        {
            if (safe == nullptr || choice == 0) return;
            const uint32_t next = choice == 100 ? all
                : current ^ (uint32_t { 1 } << static_cast<uint32_t>(choice - 1));
            if (effect == 0) safe->processor.setScrambleFeatures(next);
            else if (effect == 1) safe->processor.setMeltFeatures(next);
            else safe->processor.setSmearFeatures(next);
            safe->repaint();
        });
}

void RandomChopSamplerAudioProcessorEditor::selectTab(int tab)
{
    selectedTab = juce::jlimit(0, 3, tab);
    mainTab.setToggleState(selectedTab == 0, juce::dontSendNotification);
    fxTab.setToggleState(selectedTab == 1, juce::dontSendNotification);
    seqTab.setToggleState(selectedTab == 2, juce::dontSendNotification);
    settingsTab.setToggleState(selectedTab == 3, juce::dontSendNotification);
    if (selectedTab == 2)
    {
        pageMessage.setText("SEQ  /  AUTOMATIC TIMING\n\nCreative slices follow the host tempo automatically.\n"
                            "There is no programmable sequencer in this instrument.",
                            juce::dontSendNotification);
        pageActionButton.setButtonText("BACK TO MAIN");
    }
    else if (selectedTab == 3)
    {
        pageMessage.setText("SETTINGS  /  RECOMPILER.DLL\n\n"
                            "Drop or add up to 20 samples, shape them, then play from MIDI.\n"
                            "Use the About button for plugin information.",
                            juce::dontSendNotification);
        pageActionButton.setButtonText("ABOUT");
    }
    resized();
    repaint();
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
    g.fillAll(selected ? juce::Colour(0xffbcbcbc)
                       : recent ? juce::Colour(0xffe2e2e2) : juce::Colour(0xffeeeeee));
    g.setColour(juce::Colour(0xffaaaaaa));
    g.drawHorizontalLine(height - 1, 0.0f, static_cast<float>(width));
    const auto thumb = juce::Rectangle<int>(59, 3, juce::jmin(62, width / 5),
                                            juce::jmax(1, height - 6));
    g.setColour(juce::Colour(0xff3c3c3c));
    g.fillRect(thumb);
    if (source->waveformPeaks && !source->waveformPeaks->empty())
    {
        const auto& peaks = *source->waveformPeaks;
        const auto centre = static_cast<float>(thumb.getCentreY());
        const auto halfHeight = thumb.getHeight() * 0.42f;
        g.setColour(juce::Colour(0xfff0f0f0));
        for (int x = 1; x < thumb.getWidth() - 1; ++x)
        {
            const auto index = static_cast<size_t>(x) * peaks.size()
                / static_cast<size_t>(thumb.getWidth());
            const auto& peak = peaks[juce::jmin(index, peaks.size() - 1)];
            g.drawVerticalLine(thumb.getX() + x,
                centre - peak.maximum * halfHeight,
                centre - peak.minimum * halfHeight);
        }
    }
    const auto textX = thumb.getRight() + 8;
    g.setColour(juce::Colour(xpInk));
    g.setFont(juce::Font(12.0f, juce::Font::bold));
    g.drawText(source->settings.displayName
                   + (source->settings.missing ? " [MISSING]" : ""),
               textX, 2, juce::jmax(1, width - textX - 5), height / 2,
               juce::Justification::centredLeft, true);
    auto format = juce::File(source->settings.filePath).getFileExtension()
        .fromFirstOccurrenceOf(".", false, false).toUpperCase();
    const auto seconds = source->audio != nullptr
        ? static_cast<double>(source->audio->getNumSamples()) / juce::jmax(1.0, source->sampleRate)
        : 0.0;
    g.setFont(juce::Font(10.0f));
    g.drawText(format + "  |  " + juce::String(seconds, 1) + " s",
               textX, height / 2, juce::jmax(1, width - textX - 5), height / 2,
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
        const auto& settings = (*displayPool)[static_cast<size_t>(row)]->settings;
        controls->enabled.setToggleState(settings.enabled,
                                         juce::dontSendNotification);
        controls->enabled.setButtonText({});
        controls->enabled.setHelpText(juce::String("Include ") + settings.displayName
                                      + " in random selection");
        controls->preview.setHelpText(juce::String("Audition ") + settings.displayName);
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
    controls->preview.onClick = [this, controls]
    {
        if (!displayPool || controls->row < 0
            || controls->row >= static_cast<int>(displayPool->size())) return;
        const auto id = (*displayPool)[static_cast<size_t>(controls->row)]->runtimeId;
        processor.requestSourcePreview(processor.getPreviewingSourceId() == id ? 0 : id);
        if (controls->onSelect) controls->onSelect(controls->row);
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
    status.setText(juce::String(count).paddedLeft('0', 2) + " / 20 SOURCES",
                   juce::dontSendNotification);
    juce::String alertMessage;
    if (processor.triggeredWhileEmpty.load(std::memory_order_relaxed))
        alertMessage = "NO PLAYABLE SOURCES";
    else if (transientMessageTicks > 0 && transientMessage.isNotEmpty())
        alertMessage = transientMessage.toUpperCase();
    alert.setText(alertMessage, juce::dontSendNotification);
    alert.setVisible(alertMessage.isNotEmpty());
    if (transientMessageTicks > 0)
        --transientMessageTicks;
    voiceMode.setButtonText(voiceMode.getToggleState() ? "MONO" : "POLY");
    midiPitch.setButtonText(midiPitch.getToggleState() ? "CHORDS ON" : "CHORDS OFF");
    chordsOffButton.setToggleState(!midiPitch.getToggleState(), juce::dontSendNotification);
    chordsOnButton.setToggleState(midiPitch.getToggleState(), juce::dontSendNotification);
    polyButton.setToggleState(!voiceMode.getToggleState(), juce::dontSendNotification);
    monoButton.setToggleState(voiceMode.getToggleState(), juce::dontSendNotification);
    for (int effect = 0; effect < 4; ++effect)
    {
        juce::TextButton* buttons[] { &scramblePowerButton, &meltPowerButton,
            &smearPowerButton, &spectralPowerButton };
        buttons[effect]->setToggleState(processor.isEffectEnabled(effect),
                                        juce::dontSendNotification);
    }
    outputPowerButton.setToggleState(!processor.isOutputMuted(), juce::dontSendNotification);
    muteButton.setToggleState(processor.isOutputMuted(), juce::dontSendNotification);
    muteButton.setButtonText(processor.isOutputMuted() ? "UNMUTE" : "MUTE");
    scrambleModeButton.setButtonText(processor.getScrambleFeatures() == randomchop::ScrambleFeatures::all
        ? "Random" : "Custom");
    meltModeButton.setButtonText(processor.getMeltFeatures() == randomchop::MeltFeatures::all
        ? "Stretch" : "Custom");
    smearModeButton.setButtonText(processor.getSmearFeatures() == randomchop::SmearFeatures::all
        ? "Diffuse" : "Custom");
    outputMeter.setPeak(processor.getOutputPeak());
    const auto canvasGeneration = processor.getSpectralCanvasGeneration();
    if (canvasGeneration != lastSpectralCanvasGeneration)
    {
        spectralCanvas.setCanvas(processor.getSpectralCanvas());
        lastSpectralCanvasGeneration = canvasGeneration;
    }
    spectralCanvas.setScanPosition(processor.getSpectralScanPosition());
    spectralCanvas.setSpectrum(processor.getDisplaySpectrum());
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
    repaint(scramblePanelBounds.withHeight(25));
    repaint(meltPanelBounds.withHeight(25));
    repaint(smearPanelBounds.withHeight(25));
    repaint(spectralPanelBounds.withHeight(25));
    repaint(alert.getBounds().expanded(5, 2));
}

