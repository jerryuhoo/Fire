/*
  ==============================================================================

    ModulationMatrixPanel.cpp
    Created: 4 Aug 2025 4:44:22pm
    Author:  Yifeng Yu

  ==============================================================================
*/

#include "ModulationMatrixPanel.h"
#include "../../GUI/Skin.h"
#include "../../Utility/AudioHelpers.h"
#include "../../GUI/ModulationSourceControls.h"

#include <utility>
#include <map>

namespace
{
struct MatrixColumns
{
    juce::Rectangle<int> source, arrow, destination, amount, polarity, enabled, remove;
};

MatrixColumns matrixColumns(juce::Rectangle<int> bounds)
{
    auto area = bounds.reduced(10, 0);
    const auto extra = juce::jmax(0, area.getWidth() - 548);
    const auto sourceWidth = 88 + juce::jmin(24, extra / 6);
    const auto amountWidth = 120 + juce::jmin(56, extra / 3);
    const auto destinationWidth = juce::jmax(0, area.getWidth()
        - sourceWidth - amountWidth - 44 - 30 - 28 - 18 - 32);
    area = area.withSizeKeepingCentre(area.getWidth(), juce::jlimit(0, 32, bounds.getHeight() - 12));
    MatrixColumns columns;
    columns.source = area.removeFromLeft(sourceWidth);
    columns.arrow = area.removeFromLeft(18);
    columns.destination = area.removeFromLeft(destinationWidth);
    area.removeFromLeft(8);
    columns.amount = area.removeFromLeft(amountWidth);
    area.removeFromLeft(8);
    columns.polarity = area.removeFromLeft(44);
    area.removeFromLeft(8);
    columns.enabled = area.removeFromLeft(30);
    area.removeFromLeft(8);
    columns.remove = area.removeFromLeft(28);
    return columns;
}

bool haveSameRoutingState(const ModulationRouting& lhs,
                          const ModulationRouting& rhs) noexcept
{
    return lhs.hasSameState(rhs);
}
// A separate, owned look-and-feel keeps the transient window safe if a routing
// or its editor disappears. All selections still pass through the row's edit epoch.
class ModulationTargetPicker final : public juce::Component,
                                     private juce::ListBoxModel,
                                     private juce::KeyListener
{
public:
    using Choice = ModulationMatrixRoutingComboBox::TargetChoice;
    ModulationTargetPicker(std::vector<Choice> choices, int selected,
                           fire::ui::Skin skin, std::function<void(int)> finish, std::function<void(int)> reveal)
        : allChoices(std::move(choices)), selectedId(selected), onFinish(std::move(finish)), onReveal(std::move(reveal))
    {
        fire::ui::setSkin(*this,skin); look.setSkin(skin); setLookAndFeel(&look);
        setTitle("Choose modulation destination"); setComponentID("modulation_target_picker");
        setFocusContainerType(FocusContainerType::keyboardFocusContainer);
        const auto& tones=fire::ui::paletteFor(*this);
        addAndMakeVisible(search); search.setTitle("Search modulation targets");
        search.setComponentID("target_search"); search.setTextToShowWhenEmpty("Search parameter, module or band...",tones.textMuted);
        search.setColour(juce::TextEditor::backgroundColourId,tones.surface1);
        search.setColour(juce::TextEditor::textColourId,tones.textPrimary);
        search.setColour(juce::TextEditor::outlineColourId,tones.hairline);
        search.setColour(juce::TextEditor::focusedOutlineColourId,tones.accent);
        search.setIndents(10,8); search.addKeyListener(this);
        search.onTextChange=[this] { rebuild(); };
        search.onReturnKey=[this] { finishSelection(list.getSelectedRow()); };
        search.onEscapeKey=[this] { finishSelection(-1); };
        addAndMakeVisible(scope); scope.setTitle("Target band or master"); scope.setComponentID("target_scope");
        scope.addItem("All scopes",1); scope.addItem("Master",2);
        for(int band=1;band<=4;++band) scope.addItem("Band "+juce::String(band),band+2);
        scope.setSelectedId(1,juce::dontSendNotification);
        scope.onChange=[this] { rebuildModules(); rebuild(); };
        addAndMakeVisible(module); module.setTitle("Target module"); module.setComponentID("target_module");
        module.onChange=[this] { rebuild(); };
        addAndMakeVisible(active); addAndMakeVisible(all);
        active.setButtonText("Active chain"); active.setComponentID("target_active_chain");
        active.setTitle("Show targets in current chains, plus the existing assignment");
        all.setButtonText("All targets"); all.setComponentID("target_all_targets");
        all.setTitle("Include removed modules and inactive bands");
        active.onClick=[this] { activeOnly=true; rebuildModules(); rebuild(); };
        all.onClick=[this] { activeOnly=false; rebuildModules(); rebuild(); };
        addAndMakeVisible(revealButton); revealButton.setButtonText("Show control"); revealButton.setComponentID("target_reveal");
        revealButton.setTitle("Show the selected destination in the reactor");
        revealButton.onClick=[this]
        {
            const int row=list.getSelectedRow(); if(!juce::isPositiveAndBelow(row,getNumRows())) return;
            auto callback=onReveal; const auto choice=allChoices[visible[static_cast<size_t>(row)]];
            if(callback && choice.active && choice.itemId!=1) callback(choice.itemId);
        };
        addAndMakeVisible(list); list.setModel(this); list.setRowHeight(44); list.setMultipleSelectionEnabled(false);
        list.setTitle("Modulation destinations"); list.setComponentID("target_results"); list.addKeyListener(this);
        list.setColour(juce::ListBox::backgroundColourId,tones.canvas);
        list.setColour(juce::ListBox::outlineColourId,tones.hairline);
        list.setOutlineThickness(1);
        for(auto* box:{&scope,&module})
        {
            box->setColour(juce::ComboBox::backgroundColourId,tones.surface1);
            box->setColour(juce::ComboBox::textColourId,tones.textPrimary);
            box->setColour(juce::ComboBox::outlineColourId,tones.hairline);
        }
        search.setExplicitFocusOrder(1); scope.setExplicitFocusOrder(2); module.setExplicitFocusOrder(3);
        active.setExplicitFocusOrder(4); all.setExplicitFocusOrder(5); list.setExplicitFocusOrder(6);
        rebuildModules(); rebuild(); setSize(460,420);
    }
    ~ModulationTargetPicker() override
    {
        list.setModel(nullptr); list.removeKeyListener(this); search.removeKeyListener(this); setLookAndFeel(nullptr);
        if(onFinish) {auto callback=std::move(onFinish);callback(0);}
    }
    void focusSearch() { search.grabKeyboardFocus(); }
    void styleCallout(juce::CallOutBox& callout)
    {fire::ui::setSkin(callout,fire::ui::skinFor(*this));callout.setLookAndFeel(&calloutLook);}
    void resized() override
    {
        auto area=getLocalBounds().reduced(14); area.removeFromTop(23);
        search.setBounds(area.removeFromTop(34)); area.removeFromTop(9);
        auto groups=area.removeFromTop(30); scope.setBounds(groups.removeFromLeft(126)); groups.removeFromLeft(8); module.setBounds(groups);
        area.removeFromTop(9); auto filters=area.removeFromTop(28);
        active.setBounds(filters.removeFromLeft(126)); filters.removeFromLeft(8); all.setBounds(filters.removeFromLeft(110));
        area.removeFromTop(10); footer=area.removeFromBottom(31); revealButton.setBounds(footer.removeFromRight(104).reduced(0,3)); list.setBounds(area);
    }
    void paint(juce::Graphics& g) override
    {
        const auto& tones=fire::ui::paletteFor(*this); g.fillAll(tones.canvas);
        g.setColour(tones.textPrimary); g.setFont(fire::ui::labelFont(12));
        g.drawText("MODULATION DESTINATION",14,10,getWidth()-28,20,juce::Justification::centredLeft);
        g.setColour(tones.textMuted); g.setFont(fire::ui::bodyFont(11));
        g.drawText(juce::String(visible.size())+" targets  ·  Enter selects",footer,juce::Justification::centredLeft);
        if(visible.empty()) g.drawText("No matching destinations",list.getBounds(),juce::Justification::centred);
    }
private:
    bool scopeMatches(const Choice& choice) const {return scope.getSelectedId()==1 || choice.scope==scope.getText();}
    bool chainMatches(const Choice& choice) const {return !activeOnly || choice.active || choice.itemId==selectedId;}
    void rebuildModules()
    {
        const auto previous=module.getText(); juce::StringArray groups;
        for(const auto& choice:allChoices) if(scopeMatches(choice) && chainMatches(choice) && choice.module.isNotEmpty()) groups.addIfNotAlreadyThere(choice.module);
        groups.sortNatural(); module.clear(juce::dontSendNotification); module.addItem("All modules",1);
        int next=2,selected=1;
        for(const auto& name:groups) {if(name==previous) selected=next;module.addItem(name,next++);}
        module.setSelectedId(selected,juce::dontSendNotification);
    }
    void rebuild()
    {
        const auto query=juce::StringArray::fromTokens(search.getText().trim()," ","");
        visible.clear();
        for(size_t index=0;index<allChoices.size();++index)
        {
            const auto& choice=allChoices[index];
            if(!scopeMatches(choice) || !chainMatches(choice) || (module.getSelectedId()!=1 && choice.module!=module.getText())) continue;
            const auto searchable=choice.scope+" "+choice.module+" "+choice.label+" "+choice.parameterID;
            if(std::any_of(query.begin(),query.end(),[&](const auto& word) {return !searchable.containsIgnoreCase(word);})) continue;
            visible.push_back(index);
        }
        active.setToggleState(activeOnly,juce::dontSendNotification); all.setToggleState(!activeOnly,juce::dontSendNotification);
        list.updateContent(); list.deselectAllRows();
        const auto selected=std::find_if(visible.begin(),visible.end(),[this](size_t index) {return allChoices[index].itemId==selectedId;});
        if(!visible.empty()) list.selectRow(selected==visible.end()?0:static_cast<int>(selected-visible.begin()));
        repaint();
    }
    int getNumRows() override {return static_cast<int>(visible.size());}
    juce::String getNameForRow(int row) override
    {
        if(!juce::isPositiveAndBelow(row,getNumRows())) return {};
        const auto& choice=allChoices[visible[static_cast<size_t>(row)]];
        return choice.scope+", "+choice.module+", "+choice.label+(choice.active?"":", inactive");
    }
    void paintListBoxItem(int row,juce::Graphics& g,int width,int height,bool highlighted) override
    {
        if(!juce::isPositiveAndBelow(row,getNumRows())) return;
        const auto& choice=allChoices[visible[static_cast<size_t>(row)]];
        const auto& tones=fire::ui::paletteFor(*this);
        if(highlighted) {g.setColour(tones.accent.withAlpha(.13f));g.fillRect(0,0,width,height);}
        g.setColour(tones.textPrimary); g.setFont(fire::ui::bodyFont(13));
        g.drawText(choice.label,12,3,width-24,20,juce::Justification::centredLeft);
        auto context=choice.scope+(choice.module.isEmpty()?"":" / "+choice.module);
        if(!choice.active && choice.itemId!=1) context+="  ·  "+juce::String(choice.itemId==selectedId?"current · inactive":"inactive");
        g.setColour(tones.textMuted);g.setFont(fire::ui::bodyFont(10));g.drawText(context,12,24,width-24,15,juce::Justification::centredLeft);
    }
    void selectedRowsChanged(int row) override
    {revealButton.setEnabled(onReveal && juce::isPositiveAndBelow(row,getNumRows()) && allChoices[visible[static_cast<size_t>(row)]].active && allChoices[visible[static_cast<size_t>(row)]].itemId!=1);}
    void listBoxItemClicked(int row,const juce::MouseEvent& event) override
    {if(event.mods.isLeftButtonDown() && !event.mods.isPopupMenu()) finishSelection(row);}
    void returnKeyPressed(int row) override {finishSelection(row);}
    void finishSelection(int row)
    {
        const int result=juce::isPositiveAndBelow(row,getNumRows())?allChoices[visible[static_cast<size_t>(row)]].itemId:0;
        if(onFinish) {auto callback=std::move(onFinish);callback(result);}
    }
    bool keyPressed(const juce::KeyPress& key,juce::Component* origin) override
    {
        if(key==juce::KeyPress::escapeKey) {finishSelection(-1);return true;}
        if(origin==&search && key==juce::KeyPress::downKey) {list.grabKeyboardFocus();return true;}
        return false;
    }
    class CalloutLook final : public juce::LookAndFeel_V4
    {
        void drawCallOutBoxBackground(juce::CallOutBox& box,juce::Graphics& g,const juce::Path& path,juce::Image&) override
        {
            const auto& tones=fire::ui::paletteFor(box);
            juce::DropShadow(juce::Colours::black.withAlpha(.18f),7,{0,2}).drawForPath(g,path);
            g.setColour(tones.canvas);g.fillPath(path);g.setColour(tones.hairline);g.strokePath(path,juce::PathStrokeType(1));
        }
    };
    FireLookAndFeel look;
    CalloutLook calloutLook;
    std::vector<Choice> allChoices;
    std::vector<size_t> visible;
    int selectedId;
    bool activeOnly=true;
    std::function<void(int)> onFinish,onReveal;
    juce::TextEditor search;
    FocusAwareComboBox scope,module;
    PrimaryTextButton active,all,revealButton;
    juce::ListBox list;
    juce::Rectangle<int> footer;
};

} // namespace

ModulationMatrixRoutingComboBox::~ModulationMatrixRoutingComboBox()
{ dismissTransientInteraction(); }

void ModulationMatrixRoutingComboBox::configurePopupSession(
    EditContextProvider contextProvider,
    EditContextValidator contextValidator,
    SelectionCommitter selectionCommitter)
{
    getCurrentEditContext = std::move(contextProvider);
    isEditContextValid = std::move(contextValidator);
    commitSelection = std::move(selectionCommitter);
}

bool ModulationMatrixRoutingComboBox::capturePopupRequest()
{
    if (getCurrentEditContext == nullptr)
        return false;

    popupRequestContext = getCurrentEditContext();
    popupRequestInteractionGeneration = interactionGeneration;
    popupRequestArmed = true;
    return true;
}

bool ModulationMatrixRoutingComboBox::isContextCurrent(
    const EditContext& context) const
{
    return isEnabled()
        && isShowing()
        && isEditContextValid != nullptr
        && isEditContextValid(context);
}

bool ModulationMatrixRoutingComboBox::isPopupContextCurrent(
    const EditContext& context,
    std::uint64_t expectedInteractionGeneration) const
{
    return interactionGeneration == expectedInteractionGeneration
        && isContextCurrent(context);
}

bool ModulationMatrixRoutingComboBox::keyPressed(
    const juce::KeyPress& key)
{
    noteKeyboardInteraction();

    const bool movesBackward = key == juce::KeyPress::upKey
                               || key == juce::KeyPress::leftKey;
    const bool movesForward = key == juce::KeyPress::downKey
                              || key == juce::KeyPress::rightKey;
    if (movesBackward || movesForward)
    {
        if (isPopupActive() || popupRequestArmed)
            return true;

        if (getCurrentEditContext == nullptr)
            return true;

        const auto context = getCurrentEditContext();
        if (! isContextCurrent(context))
            return true;

        const auto delta = movesBackward ? -1 : 1;
        for (int itemIndex = getSelectedItemIndex() + delta;
             juce::isPositiveAndBelow(itemIndex, getNumItems());
             itemIndex += delta)
        {
            const auto itemId = getItemId(itemIndex);
            if (itemId == 0 || ! isItemEnabled(itemId))
                continue;

            return commitKeyboardSelection(itemId, context);
        }

        return true;
    }

    if (key == juce::KeyPress::returnKey && ! isPopupActive())
    {
        if (popupRequestArmed)
            return true;

        if (! capturePopupRequest())
            return true;
    }

    return juce::ComboBox::keyPressed(key);
}

bool ModulationMatrixRoutingComboBox::commitKeyboardSelection(
    int itemId,
    const EditContext& context)
{
    if (! isContextCurrent(context)
        || itemId == 0
        || ! isItemEnabled(itemId))
        return true;

    popupSessionActive = false;
    ++popupSessionRevision;
    setSelectedId(itemId, juce::dontSendNotification);

    auto selectionCommitter = commitSelection;
    if (selectionCommitter != nullptr)
        selectionCommitter(*this, itemId, context);

    return true;
}

void ModulationMatrixRoutingComboBox::mouseDown(
    const juce::MouseEvent& event)
{
    if (! isEnabled()
        || ! isShowing()
        || ! isCompletePrimaryDown(event))
        return;

    notePointerInteraction();

    if (popupRequestArmed && ! isPopupActive())
        return;

    const juce::Component::SafePointer<ModulationMatrixRoutingComboBox>
        safeThis(this);
    if (pointerInteractionActive)
    {
        if (! isPointerSource(event))
            return;

        releasePointerInteractionWithoutSelection(event);
        if (safeThis == nullptr)
            return;
    }

    pointerInteractionActive = true;
    cancelPendingPointerRelease = false;
    pointerSourceType = event.source.getType();
    pointerSourceIndex = event.source.getIndex();

    if (! isPopupActive() && ! capturePopupRequest())
    {
        clearPointerInteraction();
        return;
    }

    juce::ComboBox::mouseDown(event);
    if (safeThis != nullptr && ! isPopupActive())
        popupRequestArmed = false;
}

void ModulationMatrixRoutingComboBox::mouseDrag(
    const juce::MouseEvent& event)
{
    if (! pointerInteractionActive
        || ! isPointerSource(event)
        || cancelPendingPointerRelease)
        return;

    const bool mayQueuePopup = ! isPopupActive();
    if (mayQueuePopup && popupRequestArmed)
        return;

    if (mayQueuePopup)
    {
        if (! capturePopupRequest())
            return;
    }

    const juce::Component::SafePointer<ModulationMatrixRoutingComboBox>
        safeThis(this);
    juce::ComboBox::mouseDrag(event);
    if (safeThis != nullptr && mayQueuePopup && ! isPopupActive())
        popupRequestArmed = false;
}

void ModulationMatrixRoutingComboBox::mouseEnter(
    const juce::MouseEvent& event)
{
    const juce::Component::SafePointer<ModulationMatrixRoutingComboBox>
        safeThis(this);
    juce::ComboBox::mouseEnter(event);
    if (safeThis != nullptr)
    {
        recoverMissingPointerUp(event);

        if (safeThis != nullptr)
            repaint();
    }
}

void ModulationMatrixRoutingComboBox::mouseMove(
    const juce::MouseEvent& event)
{
    const juce::Component::SafePointer<ModulationMatrixRoutingComboBox>
        safeThis(this);
    juce::ComboBox::mouseMove(event);
    if (safeThis != nullptr)
    {
        recoverMissingPointerUp(event);

        if (safeThis != nullptr)
            repaint();
    }
}

void ModulationMatrixRoutingComboBox::mouseExit(
    const juce::MouseEvent& event)
{
    const juce::Component::SafePointer<ModulationMatrixRoutingComboBox>
        safeThis(this);
    juce::ComboBox::mouseExit(event);
    if (safeThis != nullptr)
    {
        recoverMissingPointerUp(event);

        if (safeThis != nullptr)
            repaint();
    }
}

void ModulationMatrixRoutingComboBox::mouseUp(
    const juce::MouseEvent& event)
{
    if (! pointerInteractionActive || ! isPointerSource(event))
        return;

    if (cancelPendingPointerRelease)
    {
        releasePointerInteractionWithoutSelection(event);
        return;
    }

    const bool mayQueuePopup = ! isPopupActive();
    if (mayQueuePopup && popupRequestArmed)
    {
        releasePointerInteractionWithoutSelection(event);
        return;
    }

    if (mayQueuePopup && ! capturePopupRequest())
    {
        releasePointerInteractionWithoutSelection(event);
        return;
    }

    const juce::Component::SafePointer<ModulationMatrixRoutingComboBox>
        safeThis(this);
    juce::ComboBox::mouseUp(event);
    if (safeThis == nullptr)
        return;

    if (mayQueuePopup && ! isPopupActive())
        popupRequestArmed = false;

    clearPointerInteraction();
}

void ModulationMatrixRoutingComboBox::mouseWheelMove(
    const juce::MouseEvent& event,
    const juce::MouseWheelDetails& wheel)
{
    juce::Component::mouseWheelMove(event, wheel);
}

void ModulationMatrixRoutingComboBox::visibilityChanged()
{
    const juce::Component::SafePointer<ModulationMatrixRoutingComboBox>
        safeThis(this);
    juce::ComboBox::visibilityChanged();
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
    if (safeThis != nullptr && lifecycleReentrancyHookForTesting != nullptr)
    {
        auto hook = lifecycleReentrancyHookForTesting;
        hook();
    }
#endif
    if (safeThis == nullptr)
        return;

    if (! isShowing())
        dismissTransientInteraction();
}

void ModulationMatrixRoutingComboBox::enablementChanged()
{
    const juce::Component::SafePointer<ModulationMatrixRoutingComboBox>
        safeThis(this);
    dismissTransientInteraction();
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
    if (safeThis != nullptr && lifecycleReentrancyHookForTesting != nullptr)
    {
        auto hook = lifecycleReentrancyHookForTesting;
        hook();
    }
#endif
    if (safeThis == nullptr)
        return;

    juce::ComboBox::enablementChanged();
}

void ModulationMatrixRoutingComboBox::parentHierarchyChanged()
{
    const juce::Component::SafePointer<ModulationMatrixRoutingComboBox>
        safeThis(this);
    juce::ComboBox::parentHierarchyChanged();
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
    if (safeThis != nullptr && lifecycleReentrancyHookForTesting != nullptr)
    {
        auto hook = lifecycleReentrancyHookForTesting;
        hook();
    }
#endif
    if (safeThis == nullptr)
        return;

    dismissTransientInteraction();
}

bool ModulationMatrixRoutingComboBox::isCompletePrimaryDown(
    const juce::MouseEvent& event) const noexcept
{
    return event.mods.isLeftButtonDown()
        && ! event.mods.isRightButtonDown()
        && ! event.mods.isMiddleButtonDown()
        && ! event.mods.isPopupMenu();
}

bool ModulationMatrixRoutingComboBox::isPointerSource(
    const juce::MouseEvent& event) const noexcept
{
    return event.source.getType() == pointerSourceType
        && event.source.getIndex() == pointerSourceIndex;
}

void ModulationMatrixRoutingComboBox::recoverMissingPointerUp(
    const juce::MouseEvent& event)
{
    if (pointerInteractionActive
        && isPointerSource(event)
        && ! event.mods.isLeftButtonDown())
        releasePointerInteractionWithoutSelection(event);
}

void ModulationMatrixRoutingComboBox::releasePointerInteractionWithoutSelection(
    const juce::MouseEvent& event)
{
    clearPointerInteraction();
    juce::ComboBox::mouseUp(
        event.getEventRelativeTo(this).withNewPosition(
            juce::Point<float> { -1.0f, -1.0f }));
}

void ModulationMatrixRoutingComboBox::clearPointerInteraction() noexcept
{
    pointerInteractionActive = false;
    cancelPendingPointerRelease = false;
    pointerSourceIndex = -1;
}

void ModulationMatrixRoutingComboBox::closePopupWindow() noexcept
{
    auto callout=targetCallout; targetCallout=nullptr;
    if(callout) {callout->exitModalState(0);if(callout) callout->dismiss();}
    juce::ComboBox::hidePopup();
}

void ModulationMatrixRoutingComboBox::dismissTransientInteraction() noexcept
{
    popupSessionActive = false;
    ++popupSessionRevision;
    ++interactionGeneration;
    cancelPendingPointerRelease = cancelPendingPointerRelease
                                  || pointerInteractionActive;
    closePopupWindow();
}

std::function<void(int)>
ModulationMatrixRoutingComboBox::createPopupResultHandler()
{
    if (getCurrentEditContext == nullptr)
        return [] (int) {};

    return createPopupResultHandler(getCurrentEditContext(),
                                    interactionGeneration);
}

std::function<void(int)>
ModulationMatrixRoutingComboBox::createPopupResultHandler(
    EditContext context,
    std::uint64_t expectedInteractionGeneration)
{
    popupSessionActive = true;
    const auto sessionRevision = ++popupSessionRevision;

    return [safeThis =
                juce::Component::SafePointer<ModulationMatrixRoutingComboBox>(this),
            capturedContext = std::move(context),
            expectedInteractionGeneration,
            sessionRevision](int result)
    {
        if (safeThis == nullptr
            || ! safeThis->popupSessionActive
            || safeThis->popupSessionRevision != sessionRevision)
            return;

        const bool mayCommit = result != 0
                               && safeThis->isPopupContextCurrent(
                                   capturedContext,
                                   expectedInteractionGeneration)
                               && safeThis->indexOfItemId(result) >= 0
                               && safeThis->isItemEnabled(result);

        safeThis->popupSessionActive = false;
        ++safeThis->popupSessionRevision;
        safeThis->cancelPendingPointerRelease =
            safeThis->cancelPendingPointerRelease
            || safeThis->pointerInteractionActive;
        safeThis->closePopupWindow();

        if (! mayCommit
            || safeThis == nullptr
            || ! safeThis->isPopupContextCurrent(
                capturedContext,
                expectedInteractionGeneration))
            return;

        safeThis->setSelectedId(result, juce::dontSendNotification);
        auto selectionCommitter = safeThis->commitSelection;
        if (selectionCommitter != nullptr)
            selectionCommitter(*safeThis, result, capturedContext);
    };
}

void ModulationMatrixRoutingComboBox::showPopup()
{
    if (! popupRequestArmed)
    {
        if (isPopupActive() || ! capturePopupRequest())
            return;

        juce::ComboBox::keyPressed(
            juce::KeyPress { juce::KeyPress::returnKey });
        return;
    }

    auto requestContext = popupRequestContext;
    const auto requestInteractionGeneration =
        popupRequestInteractionGeneration;
    popupRequestArmed = false;

    if (! isPopupContextCurrent(requestContext,
                                requestInteractionGeneration))
    {
        popupSessionActive = false;
        ++popupSessionRevision;
        closePopupWindow();
        return;
    }

    if(getTargetChoices)
    {
        auto finish=createPopupResultHandler(requestContext,requestInteractionGeneration);
        auto reveal=[safe=juce::Component::SafePointer<ModulationMatrixRoutingComboBox>(this),requestContext,requestInteractionGeneration,finish](int itemId)
        {
            if(!safe || !safe->isPopupContextCurrent(requestContext,requestInteractionGeneration)) return;
            finish(0);
            if(!safe || !safe->isContextCurrent(requestContext)) return;
            auto callback=safe->onRevealTarget; if(callback) callback(itemId);
        };
        auto picker=std::make_unique<ModulationTargetPicker>(getTargetChoices(),getSelectedId(),fire::ui::skinFor(*this),std::move(finish),std::move(reveal));
        auto* content=picker.get();
        if(auto* display=juce::Desktop::getInstance().getDisplays().getDisplayForRect(getScreenBounds()))
            picker->setSize(juce::jmin(460,display->userBounds.getLargestIntegerWithin().getWidth()-40),juce::jmin(420,display->userBounds.getLargestIntegerWithin().getHeight()-70));
        auto& callout=juce::CallOutBox::launchAsynchronously(std::move(picker),getScreenBounds(),nullptr);
        targetCallout=&callout; content->styleCallout(callout);
        callout.setDismissalMouseClicksAreAlwaysConsumed(true);
        content->focusSearch();
        return;
    }

    auto menu = *getRootMenu();
    if (menu.getNumItems() > 0)
    {
        const auto selectedId = getSelectedId();
        for (juce::PopupMenu::MenuItemIterator iterator(menu, true);
             iterator.next();)
        {
            auto& item = iterator.getItem();
            if (item.itemID != 0)
                item.isTicked = item.itemID == selectedId;
        }
    }
    else
    {
        menu.addItem(1, getTextWhenNoChoicesAvailable(), false, false);
    }

    auto& lookAndFeel = getLookAndFeel();
    menu.setLookAndFeel(&lookAndFeel);
    auto options = juce::PopupMenu::Options()
                       .withTargetComponent(this)
                       .withItemThatMustBeVisible(getSelectedId())
                       .withInitiallySelectedItem(getSelectedId())
                       .withMinimumWidth(getWidth())
                       .withMaximumNumColumns(1)
                       .withStandardItemHeight(getHeight());

    for (auto* child : getChildren())
    {
        if (auto* label = dynamic_cast<juce::Label*>(child))
        {
            options = lookAndFeel.getOptionsForComboBoxPopupMenu(*this,
                                                                 *label);
            break;
        }
    }

    menu.showMenuAsync(options,
                       createPopupResultHandler(
                           std::move(requestContext),
                           requestInteractionGeneration));
}

void ModulationMatrixPrimaryButton::paintButton(juce::Graphics& g, bool, bool)
{
    using namespace fire::ui;
    const auto bounds = getLocalBounds().toFloat().reduced(0.75f);
    if (bounds.isEmpty()) return;
    const auto hover = getHoverAnimation();
    const auto focus = getFocusAnimation();
    const auto press = getPressAnimation();
    const auto enabledAlpha = isEnabled() ? 1.0f : 0.42f;
    const bool active = getToggleState();
    if (isLineSkin(*this))
    {
        const auto skin = skinFor(*this);
        const auto& palette = paletteFor(*this);
        const auto unit = juce::jlimit(.75f, 2.5f, getHeight() / 30.0f);
        const auto emphasis = juce::jmax(hover, press);
        const bool iconOnly = appearance == Appearance::enable || appearance == Appearance::remove;
        if (!iconOnly || emphasis > .01f)
        {
            auto face = active || appearance == Appearance::add ? lineSelectionSurface(skin) : palette.surface1;
            face = face.interpolatedWith(lineHoverSurface(skin), emphasis * .65f);
            g.setColour(face.withMultipliedAlpha(enabledAlpha));
            g.fillRoundedRectangle(bounds, 3.0f * unit);
        }
        if (!iconOnly || focus > .01f)
        {
            g.setColour((focus > .01f ? palette.accent : palette.hairline).withMultipliedAlpha(enabledAlpha));
            g.drawRoundedRectangle(bounds, 3.0f * unit, 1.0f);
        }
        if (appearance == Appearance::enable)
        {
            drawLineLamp(g, bounds.withSizeKeepingCentre(20.0f * unit, 20.0f * unit),
                         skin, active, isEnabled(), juce::jmax(hover, focus), false, unit);
        }
        else if (appearance == Appearance::remove)
        {
            const auto centre = bounds.getCentre();
            g.setColour(palette.textSecondary.interpolatedWith(lineInk(colours::danger, skin), emphasis)
                .withMultipliedAlpha(enabledAlpha));
            juce::Path minus;
            minus.startNewSubPath(centre.x - 4.5f * unit, centre.y);
            minus.lineTo(centre.x + 4.5f * unit, centre.y);
            g.strokePath(minus, juce::PathStrokeType(1.1f * unit,
                juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        else
        {
            const auto ink = appearance == Appearance::add ? palette.accent
                : active ? lineInk(findColour(juce::TextButton::textColourOnId), skin) : palette.textPrimary;
            g.setColour(ink.withMultipliedAlpha(enabledAlpha));
            auto textBounds = bounds.reduced(6.0f * unit, 2.0f * unit);
            if (appearance == Appearance::add)
            {
                const auto x = bounds.getX() + 16.0f * unit;
                const auto y = bounds.getCentreY();
                g.drawLine(x - 3.5f * unit, y, x + 3.5f * unit, y, 1.1f * unit);
                g.drawLine(x, y - 3.5f * unit, x, y + 3.5f * unit, 1.1f * unit);
                textBounds.removeFromLeft(22.0f * unit);
            }
            g.setFont(portfolioFont((appearance == Appearance::add ? 11.5f : 10.5f) * unit));
            g.drawFittedText(getButtonText(), textBounds.toNearestInt(), juce::Justification::centred, 1);
        }
        return;
    }
    auto ink = findColour(active ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId);
    auto fill = paletteFor(*this).surface1.interpolatedWith(paletteFor(*this).raised, hover * 0.65f + press * 0.25f);
    auto outline = paletteFor(*this).hairline.withAlpha(0.70f + hover * 0.30f);
    if (appearance == Appearance::add)
    {
        ink = colours::whiteHot;
        fill = colours::modulation.withAlpha(0.16f + hover * 0.08f + press * 0.08f);
        outline = colours::modulation.withAlpha(0.38f + hover * 0.25f);
    }
    else if (appearance == Appearance::enable)
    {
        ink = active ? colours::positive : paletteFor(*this).textMuted;
        fill = active ? colours::positive.withAlpha(0.08f + hover * 0.05f) : fill;
        outline = active ? colours::positive.withAlpha(0.22f + hover * 0.25f) : outline;
    }
    else if (appearance == Appearance::remove)
    {
        ink = paletteFor(*this).textMuted.interpolatedWith(colours::danger, juce::jmax(hover, press));
        fill = colours::danger.withAlpha(hover * 0.09f + press * 0.07f);
        outline = paletteFor(*this).hairline.withAlpha(hover * 0.60f);
    }
    g.setColour(fill.withMultipliedAlpha(enabledAlpha));
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(outline.withMultipliedAlpha(enabledAlpha));
    g.drawRoundedRectangle(bounds, 6.0f, 1.0f);
    if (focus > 0.001f)
    {
        g.setColour(colours::gold.withAlpha(focus * 0.70f));
        g.drawRoundedRectangle(bounds.reduced(1.5f), 4.5f, 1.25f);
    }
    g.setColour(ink.withMultipliedAlpha(enabledAlpha));
    const auto centre = bounds.getCentre();
    const auto stroke = juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    if (appearance == Appearance::enable)
    {
        juce::Path symbol;
        symbol.addCentredArc(centre.x, centre.y + 0.5f, 5.7f, 5.7f, 0.0f,
                            0.55f, juce::MathConstants<float>::twoPi - 0.55f, true);
        symbol.startNewSubPath(centre.x, centre.y - 6.0f);
        symbol.lineTo(centre.x, centre.y + 0.5f);
        g.strokePath(symbol, stroke);
    }
    else if (appearance == Appearance::remove)
    {
        juce::Path symbol;
        symbol.startNewSubPath(centre.x - 4.0f, centre.y - 3.0f);
        symbol.lineTo(centre.x - 3.3f, centre.y + 5.5f);
        symbol.lineTo(centre.x + 3.3f, centre.y + 5.5f);
        symbol.lineTo(centre.x + 4.0f, centre.y - 3.0f);
        symbol.startNewSubPath(centre.x - 5.5f, centre.y - 5.0f);
        symbol.lineTo(centre.x + 5.5f, centre.y - 5.0f);
        symbol.startNewSubPath(centre.x - 2.0f, centre.y - 5.0f);
        symbol.lineTo(centre.x - 1.5f, centre.y - 7.0f);
        symbol.lineTo(centre.x + 1.5f, centre.y - 7.0f);
        symbol.lineTo(centre.x + 2.0f, centre.y - 5.0f);
        g.strokePath(symbol, stroke);
    }
    else
    {
        auto textBounds = getLocalBounds().reduced(6, 2);
        if (appearance == Appearance::add)
        {
            const auto x = bounds.getX() + 16.0f;
            g.drawLine(x - 3.5f, centre.y, x + 3.5f, centre.y, 1.5f);
            g.drawLine(x, centre.y - 3.5f, x, centre.y + 3.5f, 1.5f);
            textBounds.removeFromLeft(22);
        }
        g.setFont(labelFont(appearance == Appearance::add ? 11.5f : 10.5f));
        g.drawFittedText(getButtonText(), textBounds, juce::Justification::centred, 1);
    }
}

//==============================================================================
// ModulationMatrixHeader Implementation
//==============================================================================
ModulationMatrixHeader::ModulationMatrixHeader()
{
    // Initialize and add labels for the column titles.
    addAndMakeVisible(sourceLabel);
    sourceLabel.setText("SOURCE", juce::dontSendNotification);
    sourceLabel.setJustificationType(juce::Justification::centredLeft);

    addAndMakeVisible(amountLabel);
    amountLabel.setText("DEPTH", juce::dontSendNotification);
    amountLabel.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(polarityLabel);
    polarityLabel.setText("POLARITY", juce::dontSendNotification);
    polarityLabel.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(bypassLabel);
    bypassLabel.setText("ACTIVE", juce::dontSendNotification);
    bypassLabel.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(destinationLabel);
    destinationLabel.setText("DESTINATION", juce::dontSendNotification);
    destinationLabel.setJustificationType(juce::Justification::centredLeft);

    for (auto* label : { &sourceLabel, &amountLabel, &polarityLabel, &bypassLabel, &destinationLabel })
    {
        label->setColour(juce::Label::textColourId, fire::ui::paletteFor(*this).textMuted);
        label->setFont(fire::ui::labelFont(9.0f));
        label->setBorderSize({});
    }
}

void ModulationMatrixHeader::paint(juce::Graphics& g)
{
    g.setColour(fire::ui::paletteFor(*this).hairline.withAlpha(0.55f));
    g.drawHorizontalLine(getHeight() - 1, 0.0f, static_cast<float>(getWidth()));
}

void ModulationMatrixHeader::resized()
{
    const auto columns = matrixColumns(getLocalBounds());
    sourceLabel.setBounds(columns.source);
    destinationLabel.setBounds(columns.destination);
    amountLabel.setBounds(columns.amount);
    polarityLabel.setBounds(columns.polarity.expanded(3, 0));
    bypassLabel.setBounds(columns.enabled.expanded(3, 0));
}

//==============================================================================
// ModulationMatrixRow Implementation
//==============================================================================
ModulationMatrixRow::PrimaryButtonSlider::PrimaryButtonSlider()
{
    setWantsKeyboardFocus(true);
    setMouseClickGrabsKeyboardFocus(true);
}

ModulationMatrixRow::PrimaryButtonSlider::~PrimaryButtonSlider()
{
    dismissTransientInteraction();
    stopTimer();
}

float ModulationMatrixRow::PrimaryButtonSlider::getHoverAnimation() const noexcept
{
    return hoverAnimation.current;
}

float ModulationMatrixRow::PrimaryButtonSlider::getPressAnimation() const noexcept
{
    return pressAnimation.current;
}

float ModulationMatrixRow::PrimaryButtonSlider::getFocusAnimation() const noexcept
{
    return focusAnimation.current;
}

float ModulationMatrixRow::PrimaryButtonSlider::getDisabledAnimation() const noexcept
{
    return disabledAnimation.current;
}

std::unique_ptr<juce::AccessibilityHandler>
ModulationMatrixRow::PrimaryButtonSlider::createAccessibilityHandler()
{
    return std::make_unique<fire::ui::GuardedSliderAccessibilityHandler>(
        *this);
}

bool ModulationMatrixRow::PrimaryButtonSlider::keyPressed(
    const juce::KeyPress& key)
{
    if (hasKeyboardFocus(true) && ! focusModality.isKeyboardVisible())
    {
        focusModality.noteKeyboard();
        updateAnimationTargets();
    }

    // A keyboard edit may synchronously notify code that deletes this row, so
    // dispatching to JUCE remains the final operation.
    return juce::Slider::keyPressed(key);
}

void ModulationMatrixRow::PrimaryButtonSlider::mouseDown(
    const juce::MouseEvent& event)
{
    const juce::Component::SafePointer<PrimaryButtonSlider> safeThis(this);
    if (primaryGestureInProgress)
    {
        if (! isPointerSource(event))
            return;

        finishActivePointerGesture(&event);
        if (safeThis == nullptr)
            return;
    }

    if (! isEnabled()
        || ! event.mods.isLeftButtonDown()
        || event.mods.isRightButtonDown()
        || event.mods.isMiddleButtonDown()
        || event.mods.isPopupMenu())
        return;

    focusModality.notePointer();
    primaryGestureInProgress = true;
    pointerSourceType = event.source.getType();
    pointerSourceIndex = event.source.getIndex();
    lastAcceptedPointerEvent.emplace(event);
    updateAnimationTargets();

    pointerDispatchInProgress = true;
    juce::Slider::mouseDown(event);
    if (safeThis == nullptr)
        return;

    updateAnimationTargets();
    completePointerDispatch();
}

void ModulationMatrixRow::PrimaryButtonSlider::mouseDrag(
    const juce::MouseEvent& event)
{
    if (! primaryGestureInProgress || ! isPointerSource(event))
        return;

    lastAcceptedPointerEvent.emplace(event);
    const juce::Component::SafePointer<PrimaryButtonSlider> safeThis(this);
    pointerDispatchInProgress = true;
    juce::Slider::mouseDrag(event);
    if (safeThis == nullptr)
        return;

    updateAnimationTargets();
    completePointerDispatch();
}

void ModulationMatrixRow::PrimaryButtonSlider::mouseEnter(
    const juce::MouseEvent& event)
{
    const juce::Component::SafePointer<PrimaryButtonSlider> safeThis(this);
    juce::Slider::mouseEnter(event);
    if (safeThis == nullptr)
        return;

    updateAnimationTargets();
    recoverMissingPointerUp(event);
}

void ModulationMatrixRow::PrimaryButtonSlider::mouseMove(
    const juce::MouseEvent& event)
{
    const juce::Component::SafePointer<PrimaryButtonSlider> safeThis(this);
    juce::Slider::mouseMove(event);
    if (safeThis == nullptr)
        return;

    updateAnimationTargets();
    recoverMissingPointerUp(event);
}

void ModulationMatrixRow::PrimaryButtonSlider::mouseExit(
    const juce::MouseEvent& event)
{
    const juce::Component::SafePointer<PrimaryButtonSlider> safeThis(this);
    juce::Slider::mouseExit(event);
    if (safeThis == nullptr)
        return;

    updateAnimationTargets();
    recoverMissingPointerUp(event);
}

void ModulationMatrixRow::PrimaryButtonSlider::mouseUp(
    const juce::MouseEvent& event)
{
    if (! primaryGestureInProgress || ! isPointerSource(event))
        return;

    finishActivePointerGesture(&event);
}

void ModulationMatrixRow::PrimaryButtonSlider::visibilityChanged()
{
    const juce::Component::SafePointer<PrimaryButtonSlider> safeThis(this);
    juce::Slider::visibilityChanged();
    if (safeThis == nullptr)
        return;

    updateAnimationTargets();
    if (! isShowing())
        dismissTransientInteraction();
}

void ModulationMatrixRow::PrimaryButtonSlider::enablementChanged()
{
    const juce::Component::SafePointer<PrimaryButtonSlider> safeThis(this);
    juce::Slider::enablementChanged();
    if (safeThis == nullptr)
        return;

    updateAnimationTargets();
    if (! isEnabled())
        dismissTransientInteraction();
}

void ModulationMatrixRow::PrimaryButtonSlider::parentHierarchyChanged()
{
    const juce::Component::SafePointer<PrimaryButtonSlider> safeThis(this);
    juce::Slider::parentHierarchyChanged();
    if (safeThis == nullptr)
        return;

    updateAnimationTargets();
    if (! isShowing())
        dismissTransientInteraction();
}

void ModulationMatrixRow::PrimaryButtonSlider::focusGained(
    juce::Component::FocusChangeType cause)
{
    const juce::Component::SafePointer<PrimaryButtonSlider> safeThis(this);
    juce::Slider::focusGained(cause);
    if (safeThis != nullptr)
    {
        focusModality.focusGained(cause);
        updateAnimationTargets();
    }
}

void ModulationMatrixRow::PrimaryButtonSlider::focusLost(
    juce::Component::FocusChangeType cause)
{
    const juce::Component::SafePointer<PrimaryButtonSlider> safeThis(this);
    juce::Slider::focusLost(cause);
    if (safeThis != nullptr)
    {
        updateAnimationTargets();
    }
}

bool ModulationMatrixRow::PrimaryButtonSlider::isPointerSource(
    const juce::MouseEvent& event) const noexcept
{
    return event.source.getType() == pointerSourceType
        && event.source.getIndex() == pointerSourceIndex;
}

void ModulationMatrixRow::PrimaryButtonSlider::recoverMissingPointerUp(
    const juce::MouseEvent& event)
{
    if (primaryGestureInProgress
        && isPointerSource(event)
        && ! event.mods.isLeftButtonDown())
        finishActivePointerGesture(&event);
}

void ModulationMatrixRow::PrimaryButtonSlider::finishActivePointerGesture(
    const juce::MouseEvent* releaseEventOverride)
{
    std::optional<juce::MouseEvent> releaseEvent;
    if (releaseEventOverride != nullptr)
        releaseEvent.emplace(*releaseEventOverride);
    else if (lastAcceptedPointerEvent.has_value())
        releaseEvent.emplace(*lastAcceptedPointerEvent);

    const auto wasActive = primaryGestureInProgress;
    primaryGestureInProgress = false;
    pointerSourceIndex = -1;
    lastAcceptedPointerEvent.reset();
    pressAnimation.setTarget(0.0f);
    updateAnimationTargets();

    if (! wasActive || ! releaseEvent.has_value())
        return;

    const juce::Component::SafePointer<PrimaryButtonSlider> safeThis(this);
    pointerDispatchInProgress = true;
    // A value listener may request a host notification here. Keep it deferred
    // until Slider has finished using its Pimpl, then make it the final action.
    if (releaseEvent.has_value())
        juce::Slider::mouseUp(*releaseEvent);
    if (safeThis == nullptr)
        return;

    completePointerDispatch();
}

bool ModulationMatrixRow::PrimaryButtonSlider::deferPointerDispatchCompletion(
    bool requestRebuild,
    bool notifyHost) noexcept
{
    if (! pointerDispatchInProgress)
        return false;

    rebuildAfterPointerDispatch = rebuildAfterPointerDispatch || requestRebuild;
    notifyHostAfterPointerDispatch = notifyHostAfterPointerDispatch || notifyHost;
    return true;
}

void ModulationMatrixRow::PrimaryButtonSlider::completePointerDispatch()
{
    jassert(pointerDispatchInProgress);
    pointerDispatchInProgress = false;
    const auto requestRebuild = rebuildAfterPointerDispatch;
    const auto notifyHost = notifyHostAfterPointerDispatch;
    rebuildAfterPointerDispatch = false;
    notifyHostAfterPointerDispatch = false;
    auto completion = onPointerDispatchComplete;

    // The completion can synchronously delete this Slider through a host
    // listener. It must therefore remain the final operation in this method.
    if ((requestRebuild || notifyHost) && completion)
        completion(requestRebuild, notifyHost);
}

void ModulationMatrixRow::PrimaryButtonSlider::updateAnimationTargets() noexcept
{
    if (! isShowing())
    {
        stopTimer();
        focusModality.resetSession();
        hoverAnimation.snapTo(0.0f);
        pressAnimation.snapTo(0.0f);
        focusAnimation.snapTo(0.0f);
        disabledAnimation.snapTo(isEnabled() ? 0.0f : 1.0f);
        repaint();
        return;
    }

    const auto interactive = isEnabled();
    if (! interactive)
        focusModality.resetSession();

    hoverAnimation.setTarget(interactive && isMouseOver(true) ? 1.0f : 0.0f);
    pressAnimation.setTarget(interactive && primaryGestureInProgress ? 1.0f : 0.0f);
    focusAnimation.setTarget(interactive
                                 && focusModality.isKeyboardVisible()
                                 && hasKeyboardFocus(true)
                             ? 1.0f
                             : 0.0f);
    disabledAnimation.setTarget(interactive ? 0.0f : 1.0f);
    if (! animationsSettled() && ! isTimerRunning())
        startTimerHz(60);
    repaint();
}

bool ModulationMatrixRow::PrimaryButtonSlider::animationsSettled() const noexcept
{
    return hoverAnimation.isSettled() && pressAnimation.isSettled()
        && focusAnimation.isSettled() && disabledAnimation.isSettled();
}

bool ModulationMatrixRow::PrimaryButtonSlider::advanceAnimation(
    float deltaSeconds) noexcept
{
    auto changed = hoverAnimation.advance(deltaSeconds, 0.10f);
    changed = pressAnimation.advance(deltaSeconds, 0.065f) || changed;
    changed = focusAnimation.advance(deltaSeconds, 0.11f) || changed;
    changed = disabledAnimation.advance(deltaSeconds, 0.13f) || changed;
    return changed;
}

void ModulationMatrixRow::PrimaryButtonSlider::timerCallback()
{
    if (! isShowing())
    {
        updateAnimationTargets();
        return;
    }

    updateAnimationTargets();
    const auto changed = advanceAnimation(1.0f / 60.0f);
    if (changed)
        repaint();
    if (animationsSettled())
        stopTimer();
}

void ModulationMatrixRow::PrimaryButtonSlider::dismissTransientInteraction()
{
    finishActivePointerGesture();
}

ModulationMatrixRow::ModulationMatrixRow(FireAudioProcessor& p,
                                         int routingIndex,
                                         const ModulationRouting& routing,
                                         std::shared_ptr<ModulationRoutingEditSession> editSession,
                                         std::function<void(std::uint64_t,
                                                            ModulationRouting)> onDelete)
    : processor(p),
      index(routingIndex),
      expectedRouting(routing),
      routingEditSession(std::move(editSession)),
      onDeleteCallback(std::move(onDelete))
{
    setOpaque(false);
    setLookAndFeel(&fireLookAndFeel);
    const auto routingNumber = juce::String(index + 1);
    const auto routingName = "Modulation routing " + routingNumber;
    const auto sourceColour =
        fire::ui::modulationSourceColour(routing.sourceLfoIndex);

    // SOURCE MENU
    addAndMakeVisible(sourceMenu);
    sourceMenu.setTitle(routingName + " source");
    sourceMenu.setComponentID("matrix_source");
    sourceMenu.setTooltip("Select the modulation source for routing "
                          + routingNumber);
    for (int sourceIndex = 0; sourceIndex < fire::mod_sources::sourceCount; ++sourceIndex)
        if (processor.isModulationSourcePresent(sourceIndex))
            sourceMenu.getRootMenu()->addColouredItem(
                sourceIndex + 1,
                fire::mod_sources::isMacro(sourceIndex)
                    && processor.getMacroDisplayName(sourceIndex-fire::mod_sources::firstMacro) != fire::mod_sources::name(sourceIndex)
                        ? fire::mod_sources::name(sourceIndex)+" · "+processor.getMacroDisplayName(sourceIndex-fire::mod_sources::firstMacro)
                        : fire::mod_sources::name(sourceIndex),
                fire::ui::modulationSourceColour(sourceIndex));
    sourceMenu.setTextWhenNothingSelected("Choose source");
    sourceMenu.setSelectedId(processor.isModulationSourcePresent(routing.sourceLfoIndex)
                                 ? routing.sourceLfoIndex + 1 : 0,
                             juce::dontSendNotification);
    sourceMenu.addListener(this);
    sourceMenu.setColour(juce::ComboBox::backgroundColourId, fire::ui::paletteFor(*this).surface0);
    sourceMenu.setColour(juce::ComboBox::outlineColourId,
                         sourceColour.withAlpha(0.45f));
    sourceMenu.setColour(juce::ComboBox::textColourId, sourceColour);
    sourceMenu.setColour(juce::ComboBox::arrowColourId, sourceColour);

    // AMOUNT SLIDER
    addAndMakeVisible(amountSlider);
    amountSlider.setTitle(routingName + " depth");
    amountSlider.setComponentID("matrix_amount");
    amountSlider.setTooltip("Set the modulation depth for modulation routing "
                            + routingNumber);
    amountSlider.setRange(-1.0, 1.0, 0.01);
    amountSlider.textFromValueFunction = [](double value)
    {
        const auto percent = juce::roundToInt(value * 100.0);
        return (percent > 0 ? "+" : "") + juce::String(percent) + "%";
    };
    amountSlider.valueFromTextFunction = [](const juce::String& text)
    {
        return text.trim().trimCharactersAtEnd("%").getDoubleValue() * 0.01;
    };
    amountSlider.setValue(routing.depth, juce::dontSendNotification);
    amountSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    amountSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 24);
    amountSlider.updateText();
    amountSlider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    amountSlider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    amountSlider.setColour(juce::Slider::textBoxTextColourId, fire::ui::paletteFor(*this).textPrimary);
    amountSlider.setColour(juce::Slider::trackColourId, sourceColour);
    amountSlider.setScrollWheelEnabled(false);
    amountSlider.onPointerDispatchComplete =
        [this](bool requestRebuild, bool notifyHost)
        {
            completeAmountPointerDispatch(requestRebuild, notifyHost);
        };
    amountSlider.addListener(this);

    // BIPOLAR BUTTON
    addAndMakeVisible(bipolarButton);
    bipolarButton.setTitle(routingName + " polarity");
    bipolarButton.setTooltip("Switch modulation routing " + routingNumber
                             + " between bipolar and unipolar");
    bipolarButton.setComponentID("rounded");
    bipolarButton.setAppearance(ModulationMatrixPrimaryButton::Appearance::polarity);
    bipolarButton.setColour(juce::TextButton::buttonColourId, fire::ui::paletteFor(*this).surface0);
    bipolarButton.setColour(juce::TextButton::buttonOnColourId, fire::ui::paletteFor(*this).surface2);
    bipolarButton.setColour(juce::TextButton::textColourOnId, sourceColour);
    bipolarButton.setColour(juce::TextButton::textColourOffId, fire::ui::paletteFor(*this).textMuted);
    bipolarButton.setColour(juce::ComboBox::outlineColourId, fire::ui::paletteFor(*this).hairline);
    bipolarButton.setClickingTogglesState(true);
    bipolarButton.setToggleState(routing.isBipolar, juce::dontSendNotification);
    bipolarButton.setButtonText(bipolarButton.getToggleState() ? "Bi" : "Uni");
    bipolarButton.addListener(this);

    // BYPASS BUTTON
    addAndMakeVisible(bypassButton);
    bypassButton.setTitle(routingName + " active");
    bypassButton.setTooltip("Enable or bypass modulation routing " + routingNumber);
    bypassButton.setComponentID("rounded");
    bypassButton.setAppearance(ModulationMatrixPrimaryButton::Appearance::enable);
    bypassButton.setColour(juce::TextButton::buttonColourId, fire::ui::paletteFor(*this).surface0);
    bypassButton.setColour(juce::TextButton::buttonOnColourId, fire::ui::paletteFor(*this).surface2);
    bypassButton.setColour(juce::TextButton::textColourOnId, fire::ui::colours::positive);
    bypassButton.setColour(juce::TextButton::textColourOffId, fire::ui::paletteFor(*this).textMuted);
    bypassButton.setColour(juce::ComboBox::outlineColourId, fire::ui::paletteFor(*this).hairline);
    bypassButton.setClickingTogglesState(true);
    bypassButton.setToggleState(! routing.isBypassed, juce::dontSendNotification);
    bypassButton.setButtonText(bypassButton.getToggleState() ? "On" : "Off");
    bypassButton.addListener(this);

    // === DESTINATION MENU ===
    addAndMakeVisible(destinationMenu);
    destinationMenu.setTitle(routingName + " destination");
    destinationMenu.setComponentID("matrix_destination");
    destinationMenu.setTooltip("Select the destination for modulation routing "
                               + routingNumber);

    // 1. get all possible modulation targets
    allPossibleTargets = ParameterIDAndName::getAllModulatableTargets();
    // Keep the host's stable names/IDs intact, but spell the OTT destination
    // captions as the module and control names shown in the main editor.
    for (auto& target : allPossibleTargets)
        if (target.displayText.startsWith("Ott"))
            target.displayText = "OTT " + target.displayText.substring(3);

    // 2. Populate the destination menu
    destinationMenu.addItem("None", 1);
    for (size_t i = 0; i < allPossibleTargets.size(); ++i)
    {
        // Use displayText as the menu item, and the menu ID is the index + 2 (because "None" is 1)
        destinationMenu.addItem(allPossibleTargets[i].displayText, static_cast<int>(i) + 2);
    }

    destinationMenu.setTargetChoiceProvider([this] { return buildTargetChoices(); });
    for(const auto& choice:buildTargetChoices()) if(choice.itemId>1)
        destinationMenu.changeItemText(choice.itemId,choice.scope+" / "+choice.module+" / "+choice.label);
    destinationMenu.onRevealTarget=[this](int itemId)
    {
        if(!juce::isPositiveAndBelow(itemId-2,static_cast<int>(allPossibleTargets.size()))) return;
        const auto parameterID=allPossibleTargets[static_cast<size_t>(itemId-2)].parameterID;
        auto callback=onRevealTarget; if(callback) callback(parameterID);
    };

    // 3. Set the currently selected destination
    const auto& currentTargetId = routing.targetParameterID;
    if (currentTargetId.isNotEmpty())
    {
        for (size_t i = 0; i < allPossibleTargets.size(); ++i)
        {
            if (allPossibleTargets[i].parameterID == currentTargetId)
            {
                destinationMenu.setSelectedId(static_cast<int>(i) + 2, juce::dontSendNotification);
                break;
            }
        }
    }
    else
    {
        destinationMenu.setSelectedId(1, juce::dontSendNotification); // "None"
    }

    destinationMenu.addListener(this);
    destinationMenu.setColour(juce::ComboBox::backgroundColourId, fire::ui::paletteFor(*this).surface0);
    destinationMenu.setColour(juce::ComboBox::outlineColourId, fire::ui::paletteFor(*this).hairline);
    destinationMenu.setColour(juce::ComboBox::textColourId, fire::ui::paletteFor(*this).textPrimary);
    destinationMenu.setColour(juce::ComboBox::arrowColourId, fire::ui::paletteFor(*this).textSecondary);

    const auto configureRoutingMenu = [this](auto& menu)
    {
        menu.configurePopupSession(
            [this]
            {
                return captureComboBoxEditContext();
            },
            [this](const auto& context)
            {
                return isComboBoxEditContextCurrent(context);
            },
            [this](auto& comboBox, int selectedId, const auto& context)
            {
                commitComboBoxSelection(comboBox, selectedId, context);
            });
    };
    configureRoutingMenu(sourceMenu);
    configureRoutingMenu(destinationMenu);

    const char* mappingNames[] {"FROM", "TO", "CURVE", "DEPTH VIA"};
    for(size_t i=0;i<mappingLabels.size();++i)
    {
        auto& label=mappingLabels[i];addAndMakeVisible(label);
        label.setText(mappingNames[i],juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centredLeft);
        label.setFont(fire::ui::labelFont(9));label.setColour(juce::Label::textColourId,fire::ui::paletteFor(*this).textMuted);
        label.setInterceptsMouseClicks(false,false);
    }
    for(auto* control:{&rangeStartSlider,&rangeEndSlider})
    {
        const bool start=control==&rangeStartSlider;
        addAndMakeVisible(*control);control->setComponentID(start?"matrix_range_start":"matrix_range_end");
        control->setTitle(routingName+(start?" range start":" range end"));
        control->setTooltip("Signed offset from the destination's base knob position, in percent of its normalised range. Depth scales both endpoints. Editing From or To enables a custom range; Bi/Uni restores its default range.");
        control->setRange(-1.0,1.0,.001);
        control->textFromValueFunction=[](double value)
        {
            const auto tenths = juce::roundToInt(value * 1000.0);
            return (tenths > 0 ? "+" : "") + juce::String(tenths / 10.0, 1) + "%";
        };
        control->valueFromTextFunction=[](const juce::String& text) {return text.getDoubleValue()*.01;};
        control->setValue(start?routing.minimumOffset():routing.maximumOffset(),juce::dontSendNotification);
        control->setSliderStyle(juce::Slider::LinearHorizontal);control->setTextBoxStyle(juce::Slider::TextBoxRight,false,58,24);
        control->updateText();
        control->setColour(juce::Slider::textBoxBackgroundColourId,juce::Colours::transparentBlack);
        control->setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
        control->setColour(juce::Slider::textBoxTextColourId,fire::ui::paletteFor(*this).textPrimary);
        control->setColour(juce::Slider::trackColourId,sourceColour);control->setScrollWheelEnabled(false);
        control->onPointerDispatchComplete=[this](bool rebuild,bool notify) {completeAmountPointerDispatch(rebuild,notify);};
        control->addListener(this);
    }
    addAndMakeVisible(curveMenu);curveMenu.setComponentID("matrix_curve");curveMenu.setTitle(routingName+" response curve");
    curveMenu.setTooltip("Curve applied to the source before interpolation between From and To.");
    const juce::StringArray curves{"Linear","Ease in","Ease out","S curve"};
    curveMenu.addItemList(curves,1);curveMenu.setSelectedId(routing.curve+1,juce::dontSendNotification);
    addAndMakeVisible(depthMacroMenu);depthMacroMenu.setComponentID("matrix_depth_macro");depthMacroMenu.setTitle(routingName+" depth macro");
    depthMacroMenu.setTooltip("Fixed uses the full Depth value. A macro scales the route from zero to full Depth; it does not replace the modulation source.");
    depthMacroMenu.addItem("Fixed",1);
    for(int macro=0;macro<fire::mod_sources::macroCount;++macro)
        depthMacroMenu.addItem("M"+juce::String(macro+1)+" · "+processor.getMacroDisplayName(macro),macro+2);
    depthMacroMenu.setSelectedId(routing.depthMacro+2,juce::dontSendNotification);
    for(auto* menu:{&curveMenu,&depthMacroMenu})
    {
        menu->setColour(juce::ComboBox::backgroundColourId,fire::ui::paletteFor(*this).surface0);
        menu->setColour(juce::ComboBox::outlineColourId,fire::ui::paletteFor(*this).hairline);
        menu->setColour(juce::ComboBox::textColourId,fire::ui::paletteFor(*this).textPrimary);
        configureRoutingMenu(*menu);menu->addListener(this);
    }

    // REMOVE BUTTON
    addAndMakeVisible(removeButton);
    removeButton.setTitle("Remove modulation routing " + routingNumber);
    removeButton.setTooltip("Remove modulation routing " + routingNumber);
    removeButton.setComponentID("remove_button");
    removeButton.setAppearance(ModulationMatrixPrimaryButton::Appearance::remove);
    removeButton.setColour(juce::TextButton::buttonColourId, fire::ui::paletteFor(*this).surface0);
    removeButton.setColour(juce::TextButton::textColourOnId, fire::ui::colours::danger);
    removeButton.setColour(juce::TextButton::textColourOffId, fire::ui::paletteFor(*this).textMuted);
    removeButton.setColour(juce::ComboBox::outlineColourId, fire::ui::paletteFor(*this).hairline);
    removeButton.addListener(this);
    sourceMenu.setExplicitFocusOrder(1);
    destinationMenu.setExplicitFocusOrder(2);
    amountSlider.setExplicitFocusOrder(3);
    bipolarButton.setExplicitFocusOrder(4);
    bypassButton.setExplicitFocusOrder(5);
    removeButton.setExplicitFocusOrder(6);
    rangeStartSlider.setExplicitFocusOrder(7);rangeEndSlider.setExplicitFocusOrder(8);
    curveMenu.setExplicitFocusOrder(9);depthMacroMenu.setExplicitFocusOrder(10);
    updateVisualState();
}

std::vector<ModulationMatrixRoutingComboBox::TargetChoice> ModulationMatrixRow::buildTargetChoices() const
{
    using Choice=ModulationMatrixRoutingComboBox::TargetChoice;
    using Type=fire::effects::Type;
    std::map<juce::String,Choice> details;
    const auto* bandCount=processor.treeState.getRawParameterValue(NUM_BANDS_ID);
    const int activeBands=bandCount?juce::jlimit(1,4,juce::roundToInt(bandCount->load())):1;
    auto add=[&](const juce::String& id,int scope,const juce::String& module,const juce::String& label,bool active)
    {details[id]={0,scope==0?"Master":"Band "+juce::String(scope),module,label,id,active && scope<=activeBands};};
    for(int scope=1;scope<=4;++scope)
        for(const auto& parameter:ParameterIDAndName::getModulatableParameterInfo())
        {
            if(parameter.idBase.startsWith("bandFx")) continue;
            int node=-1; juce::String module="Output",label=parameter.name;
            if(parameter.idBase==DRIVE_ID) {node=0;module="Drive";}
            else if(parameter.idBase==BIAS_ID || parameter.idBase==REC_ID || parameter.idBase==SHAPE_MIX_ID) {node=1;module="Shape";}
            else if(parameter.idBase.startsWith("comp")) {node=2;module="Compressor";}
            else if(parameter.idBase==WIDTH_ID || parameter.idBase==PAN_ID || parameter.idBase==WIDTH_MIX_ID) {node=3;module="Stereo";}
            else if(parameter.idBase.startsWith("ott")) {node=4;module="OTT"; if(label.startsWithIgnoreCase("ott")) label=label.substring(3).trim();}
            add(ParameterIDAndName::getIDString(parameter.idBase,scope-1),scope,module,label,node<0 || processor.isModulePresent(scope,node));
        }
    for(const auto& parameter:ParameterIDAndName::getGlobalParameterInfo())
    {
        if(parameter.idBase.startsWith("masterFx")) continue;
        const bool output=parameter.idBase==OUTPUT_ID || parameter.idBase==MIX_ID;
        bool lofi=parameter.idBase==DOWNSAMPLE_ID || parameter.idBase==BIT_DEPTH_ID || parameter.idBase==JITTER_ID || parameter.idBase==DOWNSAMPLE_MIX_ID;
        for(auto* id:fire::effects::tapeIDs) lofi=lofi || parameter.idBase==id;
        add(parameter.idBase,0,output?"Output":lofi?"Lo-Fi":"EQ",parameter.name,output || processor.isModulePresent(0,lofi?1:0));
    }
    for(int scope=0;scope<fire::effects::scopeCount;++scope)
        for(int slot=0;slot<fire::effects::slotCount;++slot)
        {
            const auto type=processor.getInsertEffectType(scope,slot);
            const bool present=type!=Type::none && processor.isModulePresent(scope,fire::module_order::firstInsert+slot);
            const auto module="FX "+juce::String(slot+1)+" · "+fire::effects::name(type);
            for(int control=0;control<static_cast<int>(fire::effects::controlCount);++control)
            {
                const juce::String name=fire::effects::controls(type)[static_cast<size_t>(control)].name;
                const bool discrete=(type==Type::drive && control>=1 && control<=3)
                    || (type==Type::shape && (control==0 || control==3))
                    || (type==Type::delay && control==4)
                    || (type==Type::chordResonator && control==1);
                add(fire::effects::parameterID(scope,slot,control),scope,module,
                    type==Type::none || name.isEmpty()?"Control "+juce::String(control+1):name,present && name.isNotEmpty() && !discrete);
            }
            for(int field=fire::clouds_params::spreadField;field<fire::clouds_params::fieldCount;++field)
                add(fire::clouds_params::parameterID(scope,slot,field),scope,module,
                    "Granular "+juce::String(fire::clouds_params::fieldNames[static_cast<size_t>(field)]),present && type==Type::granular);
            add(fire::analog_params::driveID(scope,slot),scope,module,"Analog Drive",present && type==Type::shape && processor.getShapeMode(scope,slot)>=fire::analog::legacyCount);
            add(fire::core_modules::parameterID(scope,slot,fire::core_modules::jitterField),scope,module,"Jitter",present && type==Type::lofi);
            for(int node=0;node<fire::eq::maxNodes;++node)
                for(auto field:{fire::eq::Field::frequency,fire::eq::Field::gain,fire::eq::Field::q})
                    add(fire::core_modules::eqParameterID(scope,slot,node,field),scope,module,
                        "Point "+juce::String(node+1)+(field==fire::eq::Field::frequency?" Frequency":field==fire::eq::Field::gain?" Gain":" Q"),present && type==Type::eq && processor.getEqNodeState(node,scope,slot).present);
        }
    for(int node=0;node<fire::eq::maxNodes;++node)
        if(!processor.getEqNodeState(node).present)
            for(auto field:{fire::eq::Field::frequency,fire::eq::Field::gain,fire::eq::Field::q})
                if(auto found=details.find(fire::eq::parameterID(node,field));found!=details.end()) found->second.active=false;
    std::vector<Choice> result {{1,"","","None","",true}};
    for(size_t index=0;index<allPossibleTargets.size();++index)
    {
        const auto& target=allPossibleTargets[index];
        auto found=details.find(target.parameterID);
        Choice entry=found!=details.end()?found->second:Choice{0,"Master","Other",target.displayText,target.parameterID,false};
        entry.itemId=static_cast<int>(index)+2; result.push_back(std::move(entry));
    }
    std::stable_sort(result.begin()+1,result.end(),[](const Choice& left,const Choice& right)
    {
        if(left.scope!=right.scope) return left.scope=="Master" || (right.scope!="Master" && left.scope.compareNatural(right.scope)<0);
        if(left.module!=right.module) return left.module.compareNatural(right.module)<0;
        return left.label.compareNatural(right.label)<0;
    });
    return result;
}

void ModulationMatrixRow::lookAndFeelChanged()
{
    const auto nextSkin = fire::ui::skinFor(*this);
    const auto previousSkin = fireLookAndFeel.getSkin();
    fireLookAndFeel.setSkin(nextSkin);
    fire::ui::remapSkinColours(*this, previousSkin, nextSkin);
    repaint();
}

void ModulationMatrixRow::paint(juce::Graphics& g)
{
    const bool bypassed = ! bypassButton.getToggleState();
    const auto accent = fire::ui::modulationSourceColour(expectedRouting.sourceLfoIndex);
    const auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(bypassed ? fire::ui::paletteFor(*this).surface0 : fire::ui::paletteFor(*this).surface1);
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(fire::ui::paletteFor(*this).hairline.withAlpha(bypassed ? 0.35f : 0.60f));
    g.drawRoundedRectangle(bounds, 8.0f, 1.0f);
    g.setColour(accent.withAlpha(bypassed ? 0.22f : 0.85f));
    g.fillRoundedRectangle(3.0f, 12.0f, 2.0f, juce::jmax(0.0f, getHeight() - 24.0f), 1.0f);
    if(targetHighlighted)
    {g.setColour(fire::ui::paletteFor(*this).accent.withAlpha(.65f));g.drawRoundedRectangle(bounds.reduced(1),7,1);}
    if(getHeight()>=96)
    {g.setColour(fire::ui::paletteFor(*this).hairline.withAlpha(.45f));g.drawHorizontalLine(49,12.0f,getWidth()-12.0f);}
    const auto arrow = matrixColumns(getLocalBounds().withHeight(juce::jmin(48,getHeight()))).arrow.toFloat();
    const auto centre = arrow.getCentre();
    juce::Path direction;
    direction.startNewSubPath(centre.x - 4.0f, centre.y);
    direction.lineTo(centre.x + 4.0f, centre.y);
    direction.startNewSubPath(centre.x + 1.0f, centre.y - 3.0f);
    direction.lineTo(centre.x + 4.0f, centre.y);
    direction.lineTo(centre.x + 1.0f, centre.y + 3.0f);
    g.setColour(fire::ui::paletteFor(*this).textMuted.withAlpha(bypassed ? 0.35f : 0.80f));
    g.strokePath(direction, juce::PathStrokeType(1.25f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

ModulationMatrixRow::~ModulationMatrixRow()
{
    dismissTransientInteractions();
    sourceMenu.removeListener(this);
    amountSlider.removeListener(this);rangeStartSlider.removeListener(this);rangeEndSlider.removeListener(this);
    curveMenu.removeListener(this);depthMacroMenu.removeListener(this);
    bipolarButton.removeListener(this);
    bypassButton.removeListener(this);
    destinationMenu.removeListener(this);
    removeButton.removeListener(this);
    setLookAndFeel(nullptr);
}

void ModulationMatrixRow::resized()
{
    const auto columns = matrixColumns(getLocalBounds().withHeight(juce::jmin(48,getHeight())));
    sourceMenu.setBounds(columns.source);
    destinationMenu.setBounds(columns.destination);
    amountSlider.setBounds(columns.amount);
    bipolarButton.setBounds(columns.polarity);
    bypassButton.setBounds(columns.enabled);
    removeButton.setBounds(columns.remove);
    const bool showMapping=getHeight()>=96;
    auto area=getLocalBounds().reduced(12,0).withY(54).withHeight(38);
    const int cellWidth=(area.getWidth()-24)/4;
    const std::array<juce::Component*,4> controls{&rangeStartSlider,&rangeEndSlider,&curveMenu,&depthMacroMenu};
    const juce::Component::SafePointer<ModulationMatrixRow> safe(this);
    for(size_t i=0;i<controls.size();++i)
    {
        auto cell=area.removeFromLeft(i+1==controls.size()?area.getWidth():cellWidth);
        mappingLabels[i].setBounds(cell.removeFromTop(12));controls[i]->setBounds(cell);
        mappingLabels[i].setVisible(showMapping);if(!safe) return;
        controls[i]->setVisible(showMapping);if(!safe) return;
        area.removeFromLeft(8);
    }
}

void ModulationMatrixRow::updateVisualState()
{
    const auto alpha = bypassButton.getToggleState() ? 1.0f : 0.48f;
    const juce::Component::SafePointer<ModulationMatrixRow> safeThis(this);
    for (auto* control : { static_cast<juce::Component*>(&sourceMenu),
                          static_cast<juce::Component*>(&destinationMenu),
                          static_cast<juce::Component*>(&amountSlider),
                          static_cast<juce::Component*>(&bipolarButton),
                          static_cast<juce::Component*>(&rangeStartSlider),static_cast<juce::Component*>(&rangeEndSlider),
                          static_cast<juce::Component*>(&curveMenu),static_cast<juce::Component*>(&depthMacroMenu) })
    {
        control->setAlpha(alpha);
        if (! safeThis) return;
    }
    repaint();
}

void ModulationMatrixRow::visibilityChanged()
{
    juce::Component::visibilityChanged();
    if (! isShowing())
        dismissTransientInteractions();
}

void ModulationMatrixRow::enablementChanged()
{
    juce::Component::enablementChanged();
    if (! isEnabled())
        dismissTransientInteractions();
}

void ModulationMatrixRow::parentHierarchyChanged()
{
    juce::Component::parentHierarchyChanged();
    lookAndFeelChanged();
    if (! isShowing())
        dismissTransientInteractions();
}

void ModulationMatrixRow::dismissTransientInteractions() noexcept
{
    const juce::Component::SafePointer<ModulationMatrixRow> safeThis(this);
    sourceMenu.dismissTransientInteraction();
    if (safeThis == nullptr)
        return;
    amountSlider.dismissTransientInteraction();
    if (safeThis == nullptr) return;
    rangeStartSlider.dismissTransientInteraction();if(safeThis==nullptr) return;
    rangeEndSlider.dismissTransientInteraction();if(safeThis==nullptr) return;
    curveMenu.dismissTransientInteraction();if(safeThis==nullptr) return;
    depthMacroMenu.dismissTransientInteraction();if(safeThis==nullptr) return;
    bipolarButton.dismissPointerGesture();
    if (safeThis == nullptr)
        return;
    bypassButton.dismissPointerGesture();
    if (safeThis == nullptr)
        return;
    destinationMenu.dismissTransientInteraction();
    if (safeThis == nullptr)
        return;
    removeButton.dismissPointerGesture();
}

void ModulationMatrixRow::buttonClicked(juce::Button* button)
{
    if (button == &bipolarButton || button == &bypassButton)
    {
        const juce::Component::SafePointer<ModulationMatrixRow> safeThis(this);
        bipolarButton.setButtonText(bipolarButton.getToggleState() ? "Bi" : "Uni");
        if (! safeThis) return;
        bypassButton.setButtonText(bypassButton.getToggleState() ? "On" : "Off");
        if (! safeThis) return;
        updateVisualState();
        if (! safeThis) return;

        if (isParentRebuildPending())
        {
            requestParentRebuild();
            return;
        }

        auto editSession = routingEditSession;
        if (editSession == nullptr)
        {
            requestParentRebuild();
            return;
        }

        auto replacementRouting = expectedRouting;
        if (button == &bipolarButton)
        {replacementRouting.isBipolar = bipolarButton.getToggleState();replacementRouting.customRange=false;}
        else
            replacementRouting.isBypassed = ! bypassButton.getToggleState();

        auto& manager = processor.getLfoManager();
        const auto result = manager.updateModulationRoutingIfRevisionMatches(
            index,
            editSession->revision,
            expectedRouting,
            replacementRouting);
        if (! result.accepted)
        {
            requestParentRebuild();
            return;
        }

        editSession->revision = result.revision;
        expectedRouting = result.routing;
        rangeStartSlider.setValue(expectedRouting.minimumOffset(),juce::dontSendNotification);if(!safeThis) return;
        rangeEndSlider.setValue(expectedRouting.maximumOffset(),juce::dontSendNotification);if(!safeThis) return;
        if (result.changed)
        {
            auto& processorToNotify = processor;
            processorToNotify.lfoDataHasChanged();
        }

        return;
    }

    if (button == &removeButton)
    {
        if (isParentRebuildPending())
        {
            requestParentRebuild();
            return;
        }

        auto callback = onDeleteCallback;
        auto editSession = routingEditSession;
        const auto routingToDelete = expectedRouting;
        if (callback && editSession != nullptr)
            callback(editSession->revision, routingToDelete);
        return;
    }
}

void ModulationMatrixRow::sliderValueChanged(juce::Slider* slider)
{
    if (slider == &amountSlider || slider == &rangeStartSlider || slider == &rangeEndSlider)
    {
        auto& editingSlider=*static_cast<PrimaryButtonSlider*>(slider);
        if (isParentRebuildPending())
        {
            if (editingSlider.deferPointerDispatchCompletion(true, false))
                return;

            requestParentRebuild();
            return;
        }

        auto editSession = routingEditSession;
        if (editSession == nullptr)
        {
            if (editingSlider.deferPointerDispatchCompletion(true, false))
                return;

            requestParentRebuild();
            return;
        }

        auto replacementRouting = expectedRouting;
        if(slider==&amountSlider) replacementRouting.depth=static_cast<float>(amountSlider.getValue());
        else
        {
            replacementRouting.rangeStart=expectedRouting.minimumOffset();
            replacementRouting.rangeEnd=expectedRouting.maximumOffset();
            replacementRouting.customRange=true;
            if(slider==&rangeStartSlider) replacementRouting.rangeStart=static_cast<float>(rangeStartSlider.getValue());
            else replacementRouting.rangeEnd=static_cast<float>(rangeEndSlider.getValue());
        }
        auto& manager = processor.getLfoManager();
        const auto result = manager.updateModulationRoutingIfRevisionMatches(
            index,
            editSession->revision,
            expectedRouting,
            replacementRouting);
        if (! result.accepted)
        {
            if (editingSlider.deferPointerDispatchCompletion(true, false))
                return;

            requestParentRebuild();
            return;
        }

        editSession->revision = result.revision;
        expectedRouting = result.routing;
        if (result.changed)
        {
            if (editingSlider.deferPointerDispatchCompletion(false, true))
                return;

            auto& processorToNotify = processor;
            processorToNotify.lfoDataHasChanged();
            return;
        }
    }
}

void ModulationMatrixRow::comboBoxChanged(juce::ComboBox* comboBox)
{
    auto* routingMenu = dynamic_cast<ModulationMatrixRoutingComboBox*>(comboBox);
    if (routingMenu == &sourceMenu || routingMenu == &destinationMenu || routingMenu == &curveMenu || routingMenu == &depthMacroMenu)
        commitComboBoxSelection(*routingMenu,
                                routingMenu->getSelectedId(),
                                captureComboBoxEditContext());
}

ModulationMatrixRoutingComboBox::EditContext
ModulationMatrixRow::captureComboBoxEditContext() const
{
    return { routingEditSession,
             routingEditSession != nullptr ? routingEditSession->revision : 0,
             expectedRouting };
}

bool ModulationMatrixRow::isComboBoxEditContextCurrent(
    const ModulationMatrixRoutingComboBox::EditContext& context)
{
    return context.editSession != nullptr
        && context.editSession == routingEditSession
        && routingEditSession != nullptr
        && context.revision == routingEditSession->revision
        && haveSameRoutingState(context.expectedRouting, expectedRouting)
        && ! isParentRebuildPending();
}

void ModulationMatrixRow::commitComboBoxSelection(
    ModulationMatrixRoutingComboBox& comboBox,
    int selectedId,
    const ModulationMatrixRoutingComboBox::EditContext& context)
{
    if (! isComboBoxEditContextCurrent(context))
    {
        requestParentRebuild();
        return;
    }

    if(&comboBox==&curveMenu || &comboBox==&depthMacroMenu)
    {
        auto replacement=context.expectedRouting;
        if(&comboBox==&curveMenu)
        {if(selectedId<1 || selectedId>4) return;replacement.curve=selectedId-1;}
        else
        {if(selectedId<1 || selectedId>fire::mod_sources::macroCount+1) return;replacement.depthMacro=selectedId-2;}
        const auto result=processor.getLfoManager().updateModulationRoutingIfRevisionMatches(index,context.revision,context.expectedRouting,replacement);
        if(!result.accepted) {requestParentRebuild();return;}
        routingEditSession->revision=result.revision;expectedRouting=result.routing;
        if(result.changed) processor.lfoDataHasChanged();
        return;
    }

    auto selectedSourceIndex = context.expectedRouting.sourceLfoIndex;
    auto selectedTargetId = context.expectedRouting.targetParameterID;

    if (&comboBox == &sourceMenu)
    {
        if (! processor.isModulationSourcePresent(selectedId - 1))
        {
            requestParentRebuild();
            return;
        }

        selectedSourceIndex = selectedId - 1;
    }
    else if (&comboBox == &destinationMenu)
    {
        if (selectedId == 1)
        {
            selectedTargetId.clear();
        }
        else
        {
            const auto targetIndex = selectedId - 2;
            if (! juce::isPositiveAndBelow(
                    targetIndex,
                    static_cast<int>(allPossibleTargets.size())))
            {
                requestParentRebuild();
                return;
            }

            selectedTargetId =
                allPossibleTargets[static_cast<size_t>(targetIndex)]
                    .parameterID;
        }
    }
    else
    {
        return;
    }

    auto& processorToNotify = processor;
    auto& manager = processorToNotify.getLfoManager();
    const auto result = manager.assignModulationRoutingIfRevisionMatches(
        index,
        context.revision,
        context.expectedRouting,
        selectedSourceIndex,
        selectedTargetId);
    if (! result.accepted)
    {
        requestParentRebuild();
        return;
    }

    // Source/destination edits change the row's visible identity. Invalidate all rows
    // before the host can synchronously destroy or re-enter the editor.
    requestParentRebuild();
    if (result.changed)
        processorToNotify.lfoDataHasChanged();
}

bool ModulationMatrixRow::isParentRebuildPending()
{
    if (auto* panel = findParentComponentOfClass<ModulationMatrixPanel>())
        return panel->isUiRebuildPending();

    return false;
}

void ModulationMatrixRow::requestParentRebuild()
{
    if (auto* panel = findParentComponentOfClass<ModulationMatrixPanel>())
        panel->requestUiRebuild();
}

void ModulationMatrixRow::completeAmountPointerDispatch(
    bool requestRebuild,
    bool notifyHost)
{
    auto* processorToNotify = &processor;
    if (requestRebuild)
    {
        const juce::Component::SafePointer<ModulationMatrixRow> safeThis(this);
        requestParentRebuild();

        if (safeThis == nullptr)
        {
            if (notifyHost)
                processorToNotify->lfoDataHasChanged();
            return;
        }
    }

    // requestParentRebuild() dismisses transient controls and may
    // synchronously delete this row. The processor outlives its editor, so use
    // the captured processor pointer and do not touch row members afterwards.
    if (notifyHost)
        processorToNotify->lfoDataHasChanged();
}

//==============================================================================
// ModulationMatrixPanel Implementation
//==============================================================================
ModulationMatrixPanel::ModulationMatrixPanel(FireAudioProcessor& p) : processor(p)
{
    setOpaque(true);
    setLookAndFeel(&fireLookAndFeel);
    setTitle("Modulation matrix");
    processor.addChangeListener(this);
    processor.addModulationUiChangeListener(this);
    addAndMakeVisible(header);
    addAndMakeVisible(viewport);
    viewport.setScrollBarsShown(true, false);
    viewport.setScrollBarThickness(10);
    viewport.getVerticalScrollBar().setColour(juce::ScrollBar::thumbColourId,
                                             fire::ui::paletteFor(*this).textMuted.withAlpha(0.24f));
    viewport.setViewedComponent(&contentComponent, false);
    addAndMakeVisible(addButton);
    addButton.addListener(this);
    addButton.setButtonText("Add routing");
    addButton.setComponentID("matrix_add_route");
    addButton.setTitle("Add modulation routing");
    addButton.setTooltip("Connect an LFO, the input envelope or a macro to a control.");
    addButton.setAppearance(ModulationMatrixPrimaryButton::Appearance::add);
    addButton.setColour(juce::TextButton::buttonColourId, fire::ui::paletteFor(*this).surface1);
    addButton.setColour(juce::TextButton::textColourOnId, fire::ui::colours::modulation);
    addButton.setColour(juce::TextButton::textColourOffId, fire::ui::colours::modulation);
    addButton.setColour(juce::ComboBox::outlineColourId, fire::ui::paletteFor(*this).hairline);
    addAndMakeVisible(sourceControlsButton);
    sourceControlsButton.setComponentID("modulation_source_controls");
    sourceControlsButton.setTooltip("Open input envelope settings and four automatable macros.");
    sourceControlsButton.addListener(this);
    addAndMakeVisible(emptyTitle);
    addAndMakeVisible(emptyDescription);
    emptyTitle.setText("No modulation routings yet", juce::dontSendNotification);
    emptyDescription.setText("Connect an LFO, envelope or macro to a control.", juce::dontSendNotification);
    emptyTitle.setFont(fire::ui::displayFont(16.0f));
    emptyDescription.setFont(fire::ui::bodyFont(11.5f));
    emptyTitle.setColour(juce::Label::textColourId, fire::ui::paletteFor(*this).textPrimary);
    emptyDescription.setColour(juce::Label::textColourId, fire::ui::paletteFor(*this).textSecondary);
    for (auto* label : { &emptyTitle, &emptyDescription })
    {
        label->setJustificationType(juce::Justification::centred);
        label->setInterceptsMouseClicks(false, false);
    }
    buildUiFromProcessorState();
}

ModulationMatrixPanel::~ModulationMatrixPanel()
{
    dismissTransientInteractions();
    processor.removeChangeListener(this);
    processor.removeModulationUiChangeListener(this);
    cancelPendingUpdate();
    addButton.removeListener(this);
    sourceControlsButton.removeListener(this);
    setLookAndFeel(nullptr);
}

void ModulationMatrixPanel::lookAndFeelChanged()
{
    const auto nextSkin = fire::ui::skinFor(*this);
    const auto previousSkin = fireLookAndFeel.getSkin();
    fireLookAndFeel.setSkin(nextSkin);
    fire::ui::remapSkinColours(*this, previousSkin, nextSkin);
    if (auto* dialog = sourceControlsDialog.getComponent())
        if (auto* content = dialog->getContentComponent())
        {
            fire::ui::setSkin(*content, nextSkin);
            content->sendLookAndFeelChange();
        }
    repaint();
}

void ModulationMatrixPanel::parentHierarchyChanged()
{
    juce::Component::parentHierarchyChanged();
    lookAndFeelChanged();
}

void ModulationMatrixPanel::paint(juce::Graphics& g)
{
    g.fillAll(fire::ui::paletteFor(*this).canvas);
    auto heading = titleArea.withTrimmedRight(300);
    g.setFont(fire::ui::displayFont(20.0f));
    g.setColour(fire::ui::paletteFor(*this).textPrimary);
    g.drawText("Modulation", heading.removeFromTop(28), juce::Justification::centredLeft);
    g.setFont(fire::ui::bodyFont(11.5f));
    g.setColour(fire::ui::paletteFor(*this).textMuted);
    g.drawText("Layer sources; shape each route's range and response.", heading.removeFromTop(20), juce::Justification::centredLeft);
    g.setFont(fire::ui::labelFont(9.5f));
    g.drawText(juce::String(static_cast<int>(rows.size())) + (rows.size() == 1 ? " routing" : " routings")
               + "  /  " + juce::String(activeRouteCount) + " active",
               summaryArea, juce::Justification::centredLeft);
    if (rows.empty())
    {
        const auto centre = viewport.getBounds().toFloat().getCentre().translated(0.0f, -40.0f);
        const auto source = juce::Rectangle<float>(56.0f, 34.0f).withCentre(centre.translated(-46.0f, 0.0f));
        const auto target = juce::Rectangle<float>(40.0f, 34.0f).withCentre(centre.translated(50.0f, 0.0f));
        g.setColour(fire::ui::colours::modulation.withAlpha(0.10f));
        g.fillRoundedRectangle(source, 8.0f);
        g.setColour(fire::ui::colours::modulation.withAlpha(0.45f));
        g.drawRoundedRectangle(source, 8.0f, 1.0f);
        g.setFont(fire::ui::labelFont(11.0f));
        g.setColour(fire::ui::colours::modulation);
        g.drawText("LFO", source, juce::Justification::centred);
        g.setColour(fire::ui::paletteFor(*this).surface2);
        g.fillRoundedRectangle(target, 8.0f);
        g.setColour(fire::ui::paletteFor(*this).textSecondary);
        g.drawEllipse(target.withSizeKeepingCentre(14.0f, 14.0f), 1.25f);
        g.drawLine(target.getCentreX(), target.getCentreY(), target.getCentreX() + 3.0f,
                   target.getCentreY() - 5.0f, 1.25f);
        juce::Path arrow;
        arrow.startNewSubPath(source.getRight() + 10.0f, centre.y);
        arrow.lineTo(target.getX() - 10.0f, centre.y);
        arrow.startNewSubPath(target.getX() - 14.0f, centre.y - 3.0f);
        arrow.lineTo(target.getX() - 10.0f, centre.y);
        arrow.lineTo(target.getX() - 14.0f, centre.y + 3.0f);
        g.setColour(fire::ui::paletteFor(*this).textMuted);
        g.strokePath(arrow, juce::PathStrokeType(1.25f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

void ModulationMatrixPanel::resized()
{
    auto bounds = getLocalBounds().reduced(16);
    titleArea = bounds.removeFromTop(62);
    addButton.setBounds(titleArea.withWidth(136).withRightX(titleArea.getRight())
                           .withSizeKeepingCentre(136, 34));
    sourceControlsButton.setBounds(addButton.getX() - 156, addButton.getY(), 146, 34);
    summaryArea = bounds.removeFromBottom(22);

    // Position the header at the top.
    auto headerArea = bounds.removeFromTop(28);
    bounds.removeFromTop(8);
    viewport.setBounds(bounds);

    // Setting a taller content component can make the vertical scrollbar
    // appear, which reduces the available width synchronously. Apply the
    // resulting width once more so rows never extend underneath that bar.
    // Horizontal scrolling is intentionally disabled for this column layout.
    const auto contentHeight =
        juce::jmax(viewport.getHeight(), static_cast<int>(rows.size()) * rowPitch);
    for (int layoutPass = 0; layoutPass < 2; ++layoutPass)
        contentComponent.setBounds(0, 0,
                                   viewport.getMaximumVisibleWidth(),
                                   contentHeight);

    header.setBounds(headerArea.withWidth(contentComponent.getWidth()));
    for (size_t i = 0; i < rows.size(); ++i)
        rows[i]->setBounds(0, static_cast<int>(i) * rowPitch, contentComponent.getWidth(), rowHeight);
    const auto emptyCentreY = viewport.getBounds().getCentreY();
    emptyTitle.setBounds(viewport.getX(), emptyCentreY - 3, viewport.getWidth(), 24);
    emptyDescription.setBounds(viewport.getX(), emptyCentreY + 24, viewport.getWidth(), 32);
}

void ModulationMatrixPanel::visibilityChanged()
{
    juce::Component::visibilityChanged();
    if (! isShowing())
        dismissTransientInteractions();
}

void ModulationMatrixPanel::enablementChanged()
{
    juce::Component::enablementChanged();
    if (! isEnabled())
        dismissTransientInteractions();
}

void ModulationMatrixPanel::dismissTransientInteractions() noexcept
{
    const juce::Component::SafePointer<ModulationMatrixPanel> sourcePanelAlive(this);
    auto dialog = sourceControlsDialog;
    sourceControlsDialog = nullptr;
    if (dialog != nullptr) dialog->closeButtonPressed();
    if (sourcePanelAlive == nullptr) return;
    const juce::Component::SafePointer<ModulationMatrixPanel> safeThis(this);
    for (auto& row : rows)
    {
        if (row != nullptr)
            row->dismissTransientInteractions();
        if (safeThis == nullptr)
            return;
    }

    addButton.dismissPointerGesture();
}

void ModulationMatrixPanel::buttonClicked(juce::Button* button)
{
    if (button == &sourceControlsButton)
    {
        if (sourceControlsDialog != nullptr) { sourceControlsDialog->toFront(true); return; }
        juce::DialogWindow::LaunchOptions options;
        options.content.setOwned(new fire::ui::ModulationSourceControls(processor));
        fire::ui::setSkin(*options.content, fire::ui::skinFor(*this));
        options.content->sendLookAndFeelChange();
        options.dialogTitle = "Envelopes and Macros";
        options.dialogBackgroundColour = fire::ui::paletteFor(*this).canvas;
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true; options.resizable = false;
        sourceControlsDialog = options.launchAsync();
        return;
    }
    if (button == &addButton)
    {
        if (isUiRebuildPending())
            return;

        auto editSession = routingEditSession;
        if (editSession == nullptr)
        {
            requestUiRebuild();
            return;
        }

        auto& manager = processor.getLfoManager();
        const auto result =
            manager.addEmptyModulationRoutingIfRevisionMatches(
                editSession->revision);
        if (! result.accepted)
        {
            requestUiRebuild();
            return;
        }

        // Adding changes the visible routing set. Mark every old row stale
        // before a synchronous host listener can re-enter this panel.
        auto* const processorToNotify = &processor;
        requestUiRebuild();
        if (result.changed)
            processorToNotify->lfoDataHasChanged();
        return;
    }

}

void ModulationMatrixPanel::buildUiFromProcessorState()
{
    const juce::Component::SafePointer<ModulationMatrixPanel> safePanel(this);
    dismissTransientInteractions();
    if (safePanel == nullptr)
        return;

    rows.clear();
    contentComponent.removeAllChildren();

    // Build from one coherent snapshot. Component construction and callbacks
    // must not happen while the shared routing lock is held.
    const auto routingState =
        processor.getLfoManager().getModulationRoutingStateSnapshot();
    activeRouteCount = static_cast<int>(std::count_if(routingState.routings.begin(), routingState.routings.end(),
                                    [](const auto& routing) { return ! routing.isBypassed; }));
    bool hasSource = false;
    for (int sourceIndex = 0; sourceIndex < fire::mod_sources::sourceCount; ++sourceIndex)
        hasSource = hasSource || processor.isModulationSourcePresent(sourceIndex);
    addButton.setEnabled(hasSource && routingState.routings.size()
                                      < LfoManager::maximumModulationRoutings);
    if (safePanel == nullptr)
        return;
    emptyDescription.setText(hasSource
                                 ? "Connect an LFO, envelope or macro to a control."
                                 : "Add an LFO in Mod Forge to create a routing.",
                             juce::dontSendNotification);
    if (safePanel == nullptr)
        return;
    routingEditSession = std::make_shared<ModulationRoutingEditSession>();
    routingEditSession->revision = routingState.revision;
    const auto editSession = routingEditSession;
    for (int i = 0; i < routingState.routings.size(); ++i)
    {
        // When creating a row, pass a lambda function that captures the index 'i'.
        // This lambda will be called when the row's remove button is clicked.
        auto onDelete = [this,
                         index = i](std::uint64_t expectedRevision,
                                    ModulationRouting expectedRouting)
        {
            juce::Component::SafePointer<ModulationMatrixPanel> safeThis(this);
            auto& processorToNotify = processor;

            auto& manager = processorToNotify.getLfoManager();
            const auto result =
                manager.removeModulationRoutingIfRevisionMatches(
                    index,
                    expectedRevision,
                    expectedRouting);
            if (! result.accepted)
            {
                if (safeThis != nullptr)
                    safeThis->requestUiRebuild();
                return;
            }

            // Removal shifts every following index. Do not advance the old
            // rows' shared session to the new model revision: invalidate and
            // queue their rebuild before notifying any re-entrant listener.
            if (safeThis != nullptr)
                safeThis->requestUiRebuild();
            if (result.changed)
                processorToNotify.lfoDataHasChanged();
        };

        rows.push_back(std::make_unique<ModulationMatrixRow>(
            processor,
            i,
            routingState.routings.getReference(i),
            editSession,
            std::move(onDelete)));
        rows.back()->onRevealTarget=[safePanel](const juce::String& parameterID)
        {if(safePanel && safePanel->onRevealTarget) {auto callback=safePanel->onRevealTarget;callback(parameterID);}};
        contentComponent.addAndMakeVisible(*rows.back());
    }

    emptyTitle.setVisible(rows.empty());
    if (! safePanel) return;
    emptyDescription.setVisible(rows.empty());
    if (! safePanel) return;
    resized();
    if(safePanel && focusedTarget.isNotEmpty()) focusTarget(focusedTarget);
    if (safePanel) repaint();
}

void ModulationMatrixPanel::focusTarget(const juce::String& parameterID)
{
    focusedTarget=parameterID;
    const juce::Component::SafePointer<ModulationMatrixPanel> safe(this);
    ModulationMatrixRow* first=nullptr;
    for(auto& row:rows)
    {
        const bool match=parameterID.isNotEmpty() && row->getTargetParameterID()==parameterID;
        row->setTargetHighlight(match);
        if(match && first==nullptr) first=row.get();
    }
    if(first!=nullptr)
    {
        viewport.setViewPosition(0,first->getY());
        if(safe && first->isShowing()) first->focusDestination();
    }
}

void ModulationMatrixPanel::requestUiRebuild()
{
    const juce::Component::SafePointer<ModulationMatrixPanel> safeThis(this);
    dismissTransientInteractions();
    if (safeThis != nullptr)
        triggerAsyncUpdate();
}

bool ModulationMatrixPanel::isUiRebuildPending() const noexcept
{
    return isUpdatePending();
}

void ModulationMatrixPanel::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &processor)
        requestUiRebuild();
    else if (processor.isModulationUiChangeSource(source))
    {
        // Shape edits and this panel's current depth gesture must keep their
        // existing rows. A bank edit or external routing edit invalidates the
        // session even when deletion/re-creation reused the same source slot.
        const auto state = processor.getLfoManager().getModulationRoutingStateSnapshot();
        if (routingEditSession == nullptr || state.revision != routingEditSession->revision)
            requestUiRebuild();
    }
}

void ModulationMatrixPanel::handleAsyncUpdate()
{
    buildUiFromProcessorState();
}
