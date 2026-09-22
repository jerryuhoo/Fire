#include <PluginEditor.h>
#include <Panels/ControlPanel/LfoPanel.h>
#include <GUI/EffectRackNavigation.h>
#include <catch2/catch_test_macros.hpp>

namespace
{
template <class T, class Predicate>
T* bankFind(juce::Component& root, Predicate predicate)
{
    if (auto* found = dynamic_cast<T*>(&root); found && predicate(*found)) return found;
    for (auto* child : root.getChildren())
        if (auto* found = bankFind<T>(*child, predicate)) return found;
    return nullptr;
}
void pumpBank() { juce::MessageManager::getInstance()->runDispatchLoopUntil(30); }
void clickBank(juce::Component& root, const juce::String& id)
{
    auto* button = bankFind<juce::Button>(root, [&](auto& b) {return b.getComponentID() == id;});
    REQUIRE(button != nullptr); button->triggerClick(); pumpBank();
}
}

TEST_CASE("LFO bank adds scrollable stable slots and returns to an editable empty state", "[lfo-bank][ui]")
{
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    LfoPanel panel(processor);
    panel.setBounds(0, 0, 1000, 280);
    panel.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    panel.setVisible(true);
    const auto rowPitch = panel.getBankRowPitch();
    for (int slot = 4; slot < fire::lfo_bank::capacity; ++slot)
    {
        clickBank(panel, "addLfo");
        REQUIRE(processor.isLfoPresent(slot));
        CHECK(panel.getCurrentLfoIndex() == slot);
        CHECK(panel.getBankRowPitch() == rowPitch);
    }
    CHECK(panel.getBankViewport().getViewPositionY() > 0);
    auto* add = bankFind<juce::Button>(panel, [](auto& b) {return b.getComponentID() == "addLfo";});
    REQUIRE(add != nullptr); CHECK_FALSE(add->isEnabled());
    REQUIRE(processor.assignLfoToTarget(15, "drive1") == LfoManager::AssignmentResult::changed);
    int removedSource = -1;
    panel.onLfoRemoved = [&](int source) {removedSource = source;};
    auto* remove = bankFind<CloseButton>(panel, [](auto& b) {return b.getComponentID() == "lfoBankRemove16";});
    REQUIRE(remove != nullptr);
    remove->setPresented(true, false); remove->triggerClick(); pumpBank();
    CHECK(removedSource == 15);
    CHECK_FALSE(processor.isLfoPresent(15));
    CHECK_FALSE(processor.getModulationInfoForParameter("drive1").isModulated);
    CHECK(panel.getCurrentLfoIndex() == 14);
    CHECK(add->isEnabled());
    for (int slot = 0; slot < fire::lfo_bank::capacity; ++slot) processor.removeLfo(slot);
    panel.refreshLfoDisplay();
    CHECK(panel.getCurrentLfoIndex() == -1);
    CHECK_FALSE(panel.assignButton.isEnabled());
    clickBank(panel, "addLfo");
    CHECK(processor.isLfoPresent(0));
    CHECK(panel.getCurrentLfoIndex() == 0);
    CHECK(panel.assignButton.isEnabled());
}

TEST_CASE("LFO navigation matches the Band rail width row height and selection targets", "[lfo-bank][ui][layout]")
{
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    FireAudioProcessorEditor editor(processor);
    editor.stopTimer();
    auto* lfo = bankFind<LfoPanel>(editor, [](auto&) {return true;});
    auto* band = bankFind<BandPanel>(editor, [](auto&) {return true;});
    REQUIRE(lfo != nullptr); REQUIRE(band != nullptr);
    auto* bandRail = bankFind<fire::ui::EffectRackNavigation>(*band, [](auto&) {return true;});
    REQUIRE(bandRail != nullptr);
    for (int width : {1000, 1250, 1400, 2000})
    {
        editor.setSize(width, width / 2);
        const auto bandView = bandRail->getViewport().getBounds();
        const auto lfoView = lfo->getBankViewport().getBounds();
        CAPTURE(width, bandView.toString(), lfoView.toString());
        CHECK(lfoView.getWidth() == bandView.getWidth());
        CHECK(lfo->getBankRowPitch() == bandRail->getRowPitch());
        for (int slot = 0; slot < 4; ++slot)
        {
            auto* row = bankFind<juce::Button>(*lfo, [&](auto& b) {return b.getComponentID() == "lfoBankSelect" + juce::String(slot + 1);});
            REQUIRE(row != nullptr);
            CHECK(row->getHeight() <= lfo->getBankRowPitch());
            CHECK(static_cast<bool>(row->getProperties()["fireModuleRail"]));
        }
    }
}
