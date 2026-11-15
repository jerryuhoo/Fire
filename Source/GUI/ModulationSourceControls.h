#pragma once
#include "PrimarySlider.h"
#include "PrimaryButton.h"
#include "ContextAwareComboBox.h"
#include "LookAndFeel.h"
#include "Skin.h"
#include "../PluginProcessor.h"

struct EnvelopeControlsTestAccess;

namespace fire::ui
{
// Listening is deliberately momentary. A normal click must not leave the
// detector replacing the processed output after the dialog has disappeared.
class EnvelopeListenButton final : public PrimaryTextButton
{
public:
    EnvelopeListenButton() : PrimaryTextButton("Hold to listen")
    {
        setComponentID("envelope_listen");
        setTitle("Listen to the selected envelope detector");
        setTooltip("Hold the primary mouse button, Space or Return to hear the filtered detector. "
                   "Release or Escape returns to Fire. An accessibility press previews for one second.");
        setClickingTogglesState(false);
        setToggleable(false);
    }
    ~EnvelopeListenButton() override { cancel(); }

    std::function<void(bool)> onListeningChanged;

    void cancel()
    {
        const auto wasActive = mode != Mode::none;
        if (mode == Mode::keyboard) rejectedKeyCode = keyCode;
        mode = Mode::none;
        keyCode = 0;
        pointerSourceIndex = -1;
        setToggleState(false, juce::dontSendNotification);
        dismissPointerGesture();
        if (wasActive && onListeningChanged) onListeningChanged(false);
    }

    // Accessibility activation has no matching release event. A bounded
    // preview provides the same operation without making the button latching.
    void triggerClick() override
    {
        if (! canActivateButton(*this)) return;
        begin(Mode::preview);
        previewDeadline = juce::Time::getMillisecondCounterHiRes() + 1000.0;
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        if (mode == Mode::pointer && ! ownsPointer(event)) return;
        cancel();
        PrimaryTextButton::mouseDown(event);
        if (! isEnabled() || ! isShowing() || ! event.mods.isLeftButtonDown()
            || event.mods.isRightButtonDown() || event.mods.isMiddleButtonDown()
            || event.mods.isPopupMenu()) return;
        pointerSourceType = event.source.getType();
        pointerSourceIndex = event.source.getIndex();
        begin(Mode::pointer);
    }
    void mouseUp(const juce::MouseEvent& event) override
    {
        if (mode == Mode::pointer && ! ownsPointer(event)) return;
        cancel();
        PrimaryTextButton::mouseUp(event);
    }
    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (mode == Mode::pointer && ownsPointer(event)
            && ! event.mods.isLeftButtonDown()) cancel();
        PrimaryTextButton::mouseDrag(event);
    }
    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key.isKeyCode(juce::KeyPress::escapeKey))
        {
            cancel();
            return false; // Preserve the dialog's Escape-to-close action.
        }
        if (key.isKeyCode(juce::KeyPress::spaceKey)
            || key.isKeyCode(juce::KeyPress::returnKey))
        {
            if (! canActivateButton(*this)) return false;
            if (key.getKeyCode() == rejectedKeyCode) return true;
            if (mode == Mode::none)
            {
                keyCode = key.getKeyCode();
                begin(Mode::keyboard);
            }
            return true;
        }
        return PrimaryTextButton::keyPressed(key);
    }
    bool keyStateChanged(bool isKeyDown) override
    {
        if (! isKeyDown && rejectedKeyCode != 0 && ! juce::KeyPress::isKeyCurrentlyDown(rejectedKeyCode)) rejectedKeyCode = 0;
        if (mode == Mode::keyboard)
        {
            if (! isKeyDown && ! juce::KeyPress::isKeyCurrentlyDown(keyCode)) { cancel(); rejectedKeyCode = 0; }
            return true;
        }
        return PrimaryTextButton::keyStateChanged(isKeyDown);
    }
    void visibilityChanged() override
    {
        if (! isShowing()) cancel();
        PrimaryTextButton::visibilityChanged();
    }
    void parentHierarchyChanged() override
    {
        if (! isShowing() || (mode != Mode::none && peerID != currentPeerID())) cancel();
        PrimaryTextButton::parentHierarchyChanged();
    }
    void enablementChanged() override
    {
        cancel();
        PrimaryTextButton::enablementChanged();
    }
    void focusLost(FocusChangeType cause) override
    {
        cancel();
        PrimaryTextButton::focusLost(cause);
    }
    void poll()
    {
        if (mode == Mode::none) return;
        if (! isShowing() || peerID != currentPeerID()
            || (mode == Mode::pointer && ! juce::ModifierKeys::getCurrentModifiersRealtime().isLeftButtonDown())
            || (mode == Mode::keyboard && ! juce::KeyPress::isKeyCurrentlyDown(keyCode))
            || (mode == Mode::preview && juce::Time::getMillisecondCounterHiRes() >= previewDeadline))
            cancel();
    }

private:
    friend struct ::EnvelopeControlsTestAccess;
    enum class Mode { none, pointer, keyboard, preview };
    void begin(Mode requested)
    {
        mode = requested;
        peerID = currentPeerID();
        setToggleState(true, juce::dontSendNotification);
        if (onListeningChanged) onListeningChanged(true);
    }
    bool ownsPointer(const juce::MouseEvent& event) const noexcept
    {
        return event.source.getIndex() == pointerSourceIndex
            && event.source.getType() == pointerSourceType;
    }
    juce::uint32 currentPeerID() const noexcept
    {
        if (const auto* peer = getPeer()) return peer->getUniqueID();
        return 0;
    }
    Mode mode = Mode::none;
    int keyCode = 0, rejectedKeyCode = 0, pointerSourceIndex = -1;
    juce::uint32 peerID = 0;
    juce::MouseInputSource::InputSourceType pointerSourceType = juce::MouseInputSource::mouse;
    double previewDeadline = 0;
};
class ModulationSourceControls final : public juce::Component, private juce::Timer
{
public:
    explicit ModulationSourceControls(FireAudioProcessor& owner) : processor(owner)
    {
        setLookAndFeel(&lookAndFeel);
        setOpaque(true);
        setTitle("Envelope detectors and performance macros");
        for (size_t index = 0; index < sliders.size(); ++index)
        {
            auto& slider = sliders[index];
            auto& label = labels[index];
            configureSlider(slider);
            slider.setComponentID(mod_sources::ids[index]);
            const auto title = index == 0 ? juce::String("Attack") : index == 1 ? juce::String("Release")
                : index == 2 ? juce::String("Sensitivity") : processor.getMacroDisplayName(static_cast<int>(index) - 3);
            slider.setTitle(title);
            label.setText(title, juce::dontSendNotification);
            label.setJustificationType(juce::Justification::centred);
            addAndMakeVisible(label);
            addAndMakeVisible(slider);
            if (index >= 3)
                attachments[index] = std::make_unique<SliderAttachment>(processor.treeState, mod_sources::ids[index], slider);
            slider.textFromValueFunction = [index](double value)
            { return juce::String(index >= 3 ? value * 100.0 : value, 1)
                + (index < 2 ? " ms" : index == 2 ? " dB" : " %"); };
            slider.valueFromTextFunction = [index](const juce::String& text)
            { return text.getDoubleValue() * (index >= 3 ? 0.01 : 1.0); };
            slider.updateText();
            slider.setDoubleClickReturnValue(true, mod_sources::defaults[index]);
            if (index >= 3)
                slider.setTooltip("Assign this macro to destinations or routing depth in the modulation matrix. Automate it in the DAW.");
        }
        for (int index = 0; index < envelope_params::count; ++index)
        {
            auto& tab = envelopeTabs[static_cast<size_t>(index)];
            tab.setButtonText("ENV " + juce::String(index + 1));
            tab.setTitle("Edit Envelope " + juce::String(index + 1));
            tab.setComponentID("envelope_tab_" + juce::String(index + 1));
            tab.setRadioGroupId(0x454e56);
            tab.setClickingTogglesState(false);
            tab.setTooltip("Edit this envelope detector. Each envelope has its own source, filters and timing.");
            tab.onClick = [this, index] { selectEnvelope(index); };
            addAndMakeVisible(tab);
        }
        sourceMenu.setComponentID("envelope_detector_source");
        sourceMenu.setTitle("Envelope detector source");
        sourceMenu.setTooltip("Main input or a band before Fire's effects, or the external sidechain routed by your host. An unavailable source is silent.");
        sourceMenu.addItemList(envelope_params::sourceNames(), 1);
        sourceMenu.onChange = [this] { listenButton.cancel(); updateControlState(); };
        addAndMakeVisible(sourceMenu);
        sourceLabel.setText("DETECTOR", juce::dontSendNotification);
        sourceLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(sourceLabel);
        statusLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(statusLabel);

        for (size_t index = 0; index < filterSliders.size(); ++index)
        {
            auto& slider = filterSliders[index];
            auto& enabled = filterEnabled[index];
            configureSlider(slider);
            slider.textFromValueFunction = [](double value)
            { return value >= 1000.0 ? juce::String(value / 1000.0, 2) + " kHz" : juce::String(juce::roundToInt(value)) + " Hz"; };
            slider.valueFromTextFunction = [](const juce::String& text)
            { return text.getDoubleValue() * (text.containsIgnoreCase("k") ? 1000.0 : 1.0); };
            slider.setDoubleClickReturnValue(true, index == 0 ? 80.0 : 12000.0);
            slider.setTooltip(index == 0 ? "Remove low frequencies from the detector. This filter does not EQ the processed signal."
                                        : "Remove high frequencies from the detector. This filter does not EQ the processed signal.");
            enabled.setButtonText(index == 0 ? "High pass" : "Low pass");
            enabled.onClick = [this] { updateControlState(); };
            addAndMakeVisible(slider);
            addAndMakeVisible(enabled);
        }
        listenButton.onListeningChanged = [this](bool active)
        {
            if (active)
            {
                if (auditionToken != 0) processor.endEnvelopeAudition(auditionToken);
                auditionToken = processor.beginEnvelopeAudition(selectedEnvelope);
            }
            else if (auditionToken != 0)
            {
                processor.endEnvelopeAudition(auditionToken);
                auditionToken = 0;
            }
        };
        addAndMakeVisible(listenButton);
        selectEnvelope(0);
        setSize(740, 480);
    }

    ~ModulationSourceControls() override
    {
        stopTimer();
        dismissInteractions();
        listenButton.onListeningChanged = {};
        setLookAndFeel(nullptr);
    }

    int getSelectedEnvelope() const noexcept { return selectedEnvelope; }

    void selectEnvelope(int index)
    {
        if (index < 0 || index >= envelope_params::count || index == selectedEnvelope) return;
        const juce::Component::SafePointer<ModulationSourceControls> safeThis(this);
        dismissInteractions();
        if (safeThis == nullptr) return;
        ++bindingRevision;
        selectedEnvelope = index;
        for (auto& attachment : filterAttachments) attachment.reset();
        for (auto& attachment : filterEnabledAttachments) attachment.reset();
        sourceAttachment.reset();
        for (int field = 0; field < 3; ++field)
        {
            auto& slider = sliders[static_cast<size_t>(field)];
            attachments[static_cast<size_t>(field)].reset();
            const auto parameter = envelope_params::parameterID(index, envelope_params::attack + field);
            slider.setComponentID(parameter);
            slider.setTitle("Envelope " + juce::String(index + 1) + " " + labels[static_cast<size_t>(field)].getText());
            slider.setTooltip("Shape Envelope " + juce::String(index + 1) + " before it modulates your matrix destinations.");
            attachments[static_cast<size_t>(field)] = std::make_unique<SliderAttachment>(processor.treeState, parameter, slider);
            slider.textFromValueFunction = [field](double value)
            { return juce::String(value, 1) + (field < 2 ? " ms" : " dB"); };
            slider.valueFromTextFunction = [](const juce::String& text) { return text.getDoubleValue(); };
            slider.updateText();
        }
        for (int filter = 0; filter < 2; ++filter)
        {
            const auto slot = static_cast<size_t>(filter);
            const int enabledField = filter == 0 ? envelope_params::highPassEnabled : envelope_params::lowPassEnabled;
            const auto enabledID = envelope_params::parameterID(index, enabledField);
            const auto frequencyID = envelope_params::parameterID(index, enabledField + 1);
            filterEnabled[slot].setComponentID(enabledID);
            filterEnabled[slot].setTitle("Envelope " + juce::String(index + 1) + (filter == 0 ? " high pass enabled" : " low pass enabled"));
            filterSliders[slot].setComponentID(frequencyID);
            filterSliders[slot].setTitle("Envelope " + juce::String(index + 1) + (filter == 0 ? " high pass frequency" : " low pass frequency"));
            filterAttachments[slot] = std::make_unique<SliderAttachment>(processor.treeState, frequencyID, filterSliders[slot]);
            filterSliders[slot].textFromValueFunction = [](double value)
            { return value >= 1000.0 ? juce::String(value / 1000.0, 2) + " kHz" : juce::String(juce::roundToInt(value)) + " Hz"; };
            filterSliders[slot].valueFromTextFunction = [](const juce::String& text)
            { return text.getDoubleValue() * (text.containsIgnoreCase("k") ? 1000.0 : 1.0); };
            filterSliders[slot].updateText();
            filterEnabledAttachments[slot] = std::make_unique<ButtonAttachment>(processor.treeState, enabledID, filterEnabled[slot]);
        }
        const auto sourceID = envelope_params::parameterID(index, envelope_params::source);
        sourceMenu.setTitle("Envelope " + juce::String(index + 1) + " detector source");
        sourceMenu.configurePopupSession(
            [safeThis] { return safeThis != nullptr ? safeThis->bindingRevision : 0; },
            [safeThis] { return safeThis != nullptr && safeThis->isShowing(); },
            processor.treeState.getParameter(sourceID));
        sourceAttachment = std::make_unique<ComboAttachment>(processor.treeState, sourceID, sourceMenu);
        for (int tab = 0; tab < envelope_params::count; ++tab)
            envelopeTabs[static_cast<size_t>(tab)].setToggleState(tab == index, juce::dontSendNotification);
        level = processor.getLfoManager().getLfoOutput(mod_sources::envelopeSource(index));
        updateControlState();
        applyColours();
        repaint();
    }

    void lookAndFeelChanged() override
    {
        lookAndFeel.setSkin(skinFor(*this));
        applyColours();
        repaint();
    }
    void parentHierarchyChanged() override { lookAndFeelChanged(); visibilityChanged(); }
    void visibilityChanged() override
    {
        if (isShowing()) startTimerHz(30);
        else { stopTimer(); dismissInteractions(); }
    }
    void resized() override
    {
        auto area = getLocalBounds().reduced(14);
        const int macroHeight = juce::jlimit(110, 154, area.getHeight() / 3);
        macrosArea = area.removeFromBottom(macroHeight);
        area.removeFromBottom(12);
        envelopeArea = area;
        auto body = envelopeArea.reduced(12, 8);
        auto tabs = body.removeFromTop(28);
        tabs.removeFromLeft(juce::jmin(140, tabs.getWidth() / 4));
        const int tabWidth = juce::jmin(78, tabs.getWidth() / envelope_params::count);
        for (auto& tab : envelopeTabs)
        {
            tab.setBounds(tabs.removeFromLeft(tabWidth).reduced(3, 0));
        }
        body.removeFromTop(8);
        auto detector = body.removeFromTop(28);
        sourceLabel.setBounds(detector.removeFromLeft(74));
        const int sourceWidth = juce::jlimit(145, 230, detector.getWidth() / 2);
        sourceMenu.setBounds(detector.removeFromLeft(sourceWidth));
        detector.removeFromLeft(12);
        listenButton.setBounds(detector.removeFromRight(108));
        meterArea = detector.reduced(8, 0);
        body.removeFromTop(6);
        auto footer = body.removeFromBottom(18);
        statusLabel.setBounds(footer);
        body.removeFromBottom(3);
        const int cellWidth = body.getWidth() / 5;
        for (int column = 0; column < 5; ++column)
        {
            auto cell = body.removeFromLeft(cellWidth);
            auto title = cell.removeFromTop(22);
            const int knobWidth = juce::jmin(94, cellWidth - 6, cell.getHeight() + 4);
            const auto knob = cell.withSizeKeepingCentre(juce::jmax(40, knobWidth), juce::jmin(116, cell.getHeight()));
            if (column < 2)
            {
                filterEnabled[static_cast<size_t>(column)].setBounds(title.reduced(1, 0));
                filterSliders[static_cast<size_t>(column)].setBounds(knob);
            }
            else
            {
                labels[static_cast<size_t>(column - 2)].setBounds(title);
                sliders[static_cast<size_t>(column - 2)].setBounds(knob);
            }
        }
        auto macroBody = macrosArea.reduced(12, 6);
        macroBody.removeFromTop(24);
        const int macroWidth = macroBody.getWidth() / 4;
        for (int index = 3; index < 7; ++index)
        {
            auto cell = macroBody.removeFromLeft(macroWidth);
            labels[static_cast<size_t>(index)].setBounds(cell.removeFromTop(20));
            sliders[static_cast<size_t>(index)].setBounds(cell.withSizeKeepingCentre(94, juce::jmin(106, cell.getHeight())));
        }
    }
    void paint(juce::Graphics& g) override
    {
        const auto skin = skinFor(*this);
        const auto& palette = paletteFor(*this);
        const auto accent = isLineSkin(skin) ? palette.accent : modulationSourceColour(mod_sources::envelopeSource(juce::jmax(0, selectedEnvelope)));
        drawCanvas(g, getLocalBounds().toFloat(), skin);
        if (isLineSkin(skin))
        {
            g.setColour(palette.hairline);
            g.drawHorizontalLine(macrosArea.getY(), float(macrosArea.getX()), float(macrosArea.getRight()));
        }
        else
        {
            drawPanel(g, envelopeArea.toFloat(), accent, false, skin);
            drawPanel(g, macrosArea.toFloat(), colours::modulation, false, skin);
        }
        g.setFont(isLineSkin(skin) ? portfolioMonoFont(11) : labelFont(12));
        g.setColour(palette.textPrimary);
        g.drawText("ENVELOPES", envelopeArea.reduced(12, 0).withHeight(42), juce::Justification::centredLeft);
        g.drawText("MACROS  /  ASSIGN IN MATRIX", macrosArea.reduced(12, 0).withHeight(30), juce::Justification::centredLeft);
        auto bar = meterArea.withHeight(5).withY(meterArea.getY() + 5).toFloat();
        g.setColour(palette.hairline.withAlpha(0.6f));
        g.fillRoundedRectangle(bar, 2);
        g.setColour(accent);
        g.fillRoundedRectangle(bar.withWidth(bar.getWidth() * level), 2);
        g.setFont(portfolioMonoFont(10));
        g.setColour(palette.textSecondary);
        g.drawText(juce::String(juce::roundToInt(level * 100)) + "%", meterArea.withTrimmedTop(12), juce::Justification::centredLeft);
    }
    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key.isKeyCode(juce::KeyPress::escapeKey)) { listenButton.cancel(); return false; }
        if (! key.getModifiers().isCommandDown() && ! key.getModifiers().isCtrlDown()) return false;
        if (juce::CharacterFunctions::toLowerCase(key.getTextCharacter()) == 'z')
            return key.getModifiers().isShiftDown() ? processor.redoEdit() : processor.undoEdit();
        if (juce::CharacterFunctions::toLowerCase(key.getTextCharacter()) == 'y') return processor.redoEdit();
        return false;
    }

private:
    friend struct ::EnvelopeControlsTestAccess;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    static void configureSlider(PrimarySlider& slider)
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 96, 22);
    }
    void dismissInteractions()
    {
        const juce::Component::SafePointer<ModulationSourceControls> safeThis(this);
        ++bindingRevision;
        listenButton.cancel();
        sourceMenu.dismissTransientInteraction();
        for (auto& button : envelopeTabs) button.dismissPointerGesture();
        for (auto& button : filterEnabled) button.dismissPointerGesture();
        for (auto& slider : sliders)
        {
            slider.dismissTransientInteraction();
            if (safeThis == nullptr) return;
        }
        for (auto& slider : filterSliders)
        {
            slider.dismissTransientInteraction();
            if (safeThis == nullptr) return;
        }
    }
    void applyColours()
    {
        const auto& palette = paletteFor(*this);
        const auto isLine = isLineSkin(*this);
        const auto envelopeColour = isLine ? palette.accent : modulationSourceColour(mod_sources::envelopeSource(juce::jmax(0, selectedEnvelope)));
        auto colourSlider = [&](auto& slider, juce::Colour accent)
        {
            slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
            slider.setColour(juce::Slider::textBoxTextColourId, palette.textPrimary);
            slider.setColour(juce::Slider::textBoxBackgroundColourId, isLine ? juce::Colours::transparentBlack : palette.surface0);
            slider.setColour(juce::Slider::textBoxOutlineColourId, isLine ? juce::Colours::transparentBlack : palette.hairline);
        };
        for (size_t index = 0; index < sliders.size(); ++index)
        {
            colourSlider(sliders[index], index < 3 ? envelopeColour : isLine ? palette.accent : modulationSourceColour(mod_sources::firstMacro + static_cast<int>(index) - 3));
            labels[index].setColour(juce::Label::textColourId, palette.textSecondary);
            labels[index].setFont(isLine ? portfolioFont(12) : bodyFont(12));
        }
        for (auto& slider : filterSliders) colourSlider(slider, envelopeColour);
        for (auto& toggle : filterEnabled)
        {
            toggle.setColour(juce::ToggleButton::textColourId, palette.textSecondary);
            toggle.setColour(juce::ToggleButton::tickColourId, envelopeColour);
        }
        for (int index = 0; index < envelope_params::count; ++index)
        {
            auto& tab = envelopeTabs[static_cast<size_t>(index)];
            tab.setColour(juce::TextButton::buttonColourId, palette.surface1);
            tab.setColour(juce::TextButton::buttonOnColourId, isLine ? palette.accent : modulationSourceColour(mod_sources::envelopeSource(index)));
            tab.setColour(juce::TextButton::textColourOffId, palette.textSecondary);
        }
        sourceMenu.setColour(juce::ComboBox::backgroundColourId, palette.surface0);
        sourceMenu.setColour(juce::ComboBox::textColourId, palette.textPrimary);
        sourceMenu.setColour(juce::ComboBox::outlineColourId, palette.hairline);
        sourceMenu.setColour(juce::ComboBox::arrowColourId, envelopeColour);
        sourceLabel.setColour(juce::Label::textColourId, palette.textSecondary);
        sourceLabel.setFont(isLine ? portfolioMonoFont(10) : labelFont(11));
        statusLabel.setColour(juce::Label::textColourId, palette.textMuted);
        statusLabel.setFont(isLine ? portfolioFont(10.5f) : bodyFont(10.5f));
        listenButton.setColour(juce::TextButton::buttonOnColourId, envelopeColour);
    }
    void updateControlState()
    {
        for (size_t index = 0; index < filterSliders.size(); ++index)
            filterSliders[index].setEnabled(filterEnabled[index].getToggleState());
        const int source = sourceMenu.getSelectedId() - 1;
        statusLabel.setText(source == envelope_params::externalInput
            ? "Route audio to Fire's sidechain input in your host. No sidechain signal means no envelope."
            : source >= envelope_params::band1
                ? "Band detector before effects. An inactive band is silent. Filters only affect the detector."
                : "Main input before effects. Filters shape the envelope detector.", juce::dontSendNotification);
    }
    void timerCallback() override
    {
        listenButton.poll();
        if (auditionToken != 0 && processor.getEnvelopeAuditionIndex() != selectedEnvelope) listenButton.cancel();
        level = juce::jlimit(0.0f, 1.0f, processor.getLfoManager().getLfoOutput(mod_sources::envelopeSource(selectedEnvelope)));
        repaint(meterArea);
        updateControlState();
        const auto names = processor.getMacroDisplayNames();
        for (size_t i = 0; i < names.size(); ++i)
        {
            labels[i + 3].setText(names[i], juce::dontSendNotification);
            sliders[i + 3].setTitle("Macro " + juce::String(static_cast<int>(i) + 1) + ": " + names[i]);
        }
    }
    FireAudioProcessor& processor;
    FireLookAndFeel lookAndFeel;
    std::array<PrimarySlider, 7> sliders;
    std::array<juce::Label, 7> labels;
    std::array<std::unique_ptr<SliderAttachment>, 7> attachments;
    std::array<PrimaryTextButton, envelope_params::count> envelopeTabs;
    ContextAwareComboBox sourceMenu;
    juce::Label sourceLabel, statusLabel;
    std::unique_ptr<ComboAttachment> sourceAttachment;
    std::array<PrimarySlider, 2> filterSliders;
    std::array<PrimaryToggleButton, 2> filterEnabled;
    std::array<std::unique_ptr<SliderAttachment>, 2> filterAttachments;
    std::array<std::unique_ptr<ButtonAttachment>, 2> filterEnabledAttachments;
    EnvelopeListenButton listenButton;
    juce::Rectangle<int> envelopeArea, macrosArea, meterArea;
    std::uint64_t bindingRevision = 0, auditionToken = 0;
    int selectedEnvelope = -1;
    float level = 0;
};
}
