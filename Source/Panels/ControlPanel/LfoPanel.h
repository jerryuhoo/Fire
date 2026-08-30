/*
  ==============================================================================

    LfoPanel.h
    Created: 2 Aug 2025 9:24:01pm
    Author:  Yifeng Yu

  ==============================================================================
*/

#pragma once

#include "../../DSP/LfoData.h"
#include "../../GUI/PrimarySlider.h"
#include "../../GUI/FireTheme.h"
#include "../../Utility/Parameters.h" // Include for LfoEditMode and LfoPresetShape
#include "ModulationMatrixPanel.h"
#include "juce_audio_processors/juce_audio_processors.h"
#include "juce_gui_basics/juce_gui_basics.h"
#include <cstdint>
#include <optional>

class FireAudioProcessor;
class LfoEditorAccessibilityHandler;
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
struct LfoEditorTestAccess;
struct LfoPanelDialogTestAccess;
struct LfoPanelBrushTestAccess;
#endif

inline std::optional<LfoData> lfoClipboard;

//
//  The LfoEditor is now a pure "View" component.
//  It holds a pointer to the data it should display and modify.
//  It is told which editing mode to be in by the LfoPanel.
//
class LfoEditor : public juce::Component,
                  private juce::Timer
{
public:
    struct DataContext
    {
        int lfoIndex = -1;
        std::uint64_t revision = 0;
    };

    LfoEditor();
    ~LfoEditor() override;

    // Sets the data model for the editor to point to. This is the safe way to switch LFOs.
    void setDataToDisplay(const LfoData& dataToDisplay);
    void setDataToDisplay(const LfoData& dataToDisplay,
                          DataContext dataContext);
    DataContext getDataContext() const noexcept { return activeDataContext; }
    bool updateDataContextRevision(DataContext expectedContext,
                                   std::uint64_t newRevision) noexcept;
    void setDataContextValidator(
        std::function<bool(const DataContext&)> validator);

    // Called by LfoPanel to set the current interaction mode.
    void setCurrentBrush(LfoPresetShape newBrush);
    void setEditMode(LfoEditMode newMode);

    //==============================================================================
    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;
    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    bool keyPressed(const juce::KeyPress& key) override;
    void visibilityChanged() override;
    void enablementChanged() override;
    void focusGained(FocusChangeType) override;
    void focusLost(FocusChangeType) override;
    std::unique_ptr<juce::AccessibilityHandler>
    createAccessibilityHandler() override;

    void setGridDivisions(int horizontal, int vertical);
    void setPlayheadPosition(float position);
    void setPhaseOffsetLinePosition(float position);
    void setSmoothness(float smoothness);
    /** Invalidates this editor's async menu result without using JUCE's
        process-wide dismissAllActiveMenus(). The visual popup may remain until
        JUCE dismisses it, but its eventual result is guaranteed to be inert.
    */
    void invalidateContextMenuSession();
    void dismissTransientInteraction();

    std::function<void(const LfoData&)> onDataChanged;

    enum CommandIDs
    {
        selectAll = 1,
        clear,
        copy,
        paste,
        invertX,
        invertY
    };

private:
    friend class LfoEditorAccessibilityHandler;
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
    friend struct LfoEditorTestAccess;
    friend struct LfoPanelBrushTestAccess;
#endif

    enum class PointerGesture
    {
        none,
        rejected,
        popupMenu,
        primary
    };

    // This pointer holds the currently active LFO data. It does not own the data.
    LfoData activeLfoData;
    bool dataIsActive = false;
    DataContext activeDataContext;
    std::function<bool(const DataContext&)> dataContextValidator;

    // Internal helper methods that now operate on the activeLfoData pointer.
    void addPoint(juce::Point<float> newPoint);
    void removePoint(int index);
    void updateAndSortPoints();
    void rebuildCurvatures();
    bool isValidPointIndex(int index) const noexcept;
    bool isValidCurveIndex(int index) const noexcept;
    bool hasValidSelectedPointIndices() const noexcept;
    bool hasValidPointDragState() const noexcept;
    bool validateCurveInteractionOrCancel() noexcept;
    bool validatePointDragInteractionOrCancel() noexcept;
    void cancelPointAndCurveInteraction() noexcept;
    void cancelAllInteraction() noexcept;
    void timerCallback() override;
    void updateAnimationTargets() noexcept;
    void startAnimationIfNeeded() noexcept;
    float getPointVisualRadius() const noexcept;
    static bool isCompletePrimaryDown(
        const juce::MouseEvent& event) noexcept;
    static bool isStandalonePopupDown(
        const juce::MouseEvent& event) noexcept;
    bool isPointerSource(const juce::MouseEvent& event) const noexcept;
    bool recoverMissingPointerUp(const juce::MouseEvent& event);
    void beginPointerGesture(PointerGesture gesture,
                             const juce::MouseEvent& event) noexcept;
    void clearPointerGesture() noexcept;
    void clearPrimaryDoubleClickAuthorization() noexcept;
    bool hasPrimaryDoubleClickAuthorization(
        const juce::MouseEvent& event) const noexcept;
    struct ContextMenuCommandContext
    {
        DataContext dataContext;
        LfoData sourceData;
        std::optional<LfoData> clipboardData;
        bool dataWasActive = false;
        bool copyWasEnabled = false;
        bool pasteWasEnabled = false;
        bool invertWasEnabled = false;
    };
    std::function<void(int)> createContextMenuResultHandler();
    void showContextMenu(const juce::MouseEvent& event);
    void handleContextMenuResult(int result,
                                 const ContextMenuCommandContext& context);

    // Helper methods for brush mode
    int getOrCreatePointAtX(float targetX);
    bool applyBrushShape(const juce::Point<int>& clickPosition);
    void publishActiveData();
    int findSegmentIndexAt(const juce::Point<int>& position) const;

    juce::Point<float> toNormalized(juce::Point<int> localPoint);
    juce::Point<float> fromNormalized(juce::Point<float> normalizedPoint);

    void deleteSelectedPoints();
    bool canAcceptPointKeyboardInput() const noexcept;
    bool selectAdjacentPoint(bool moveBackwards);
    bool nudgeSelectedPoints(juce::Point<float> requestedDelta);
    juce::String getAccessiblePointStatus() const;
    void notifyAccessiblePointStateChanged(
        juce::AccessibilityEvent event);

    void rebuildGridCache(float physicalScale);
    void rebuildWavePath();
    uint64_t getWavePathSignature() const noexcept;
    juce::Image gridCache;
    juce::Path cachedWavePath;
    uint64_t cachedWavePathSignature = 0;
    float cachedGridScale = 0.0f;
    int cachedGridWidth = 0;
    int cachedGridHeight = 0;
    int cachedHorizontalDivisions = 0;
    int cachedVerticalDivisions = 0;

    // --- State variables for interaction ---
    LfoEditMode currentMode = LfoEditMode::PointEdit;
    LfoPresetShape currentBrush = LfoPresetShape::SawUp;

    // State for PointEdit mode
    int draggingPointIndex = -1;
    int editingCurveIndex = -1; // This is the new name for draggedCurvatureIndex
    int hoveredPointIndex = -1;
    int animatedPointIndex = -1;
    fire::ui::DampedValue pointHoverAnimation;
    fire::ui::DampedValue focusAnimation;
    float initialCurvature = 0.0f;
    int initialDragY = 0;

    int hGridDivs = 4;
    int vGridDivs = 4;
    float playheadPos = -1.0f;
    float phaseOffsetPosition = -1.0f;

    const int maxPoints = 64;

    std::vector<int> selectedPointIndices;
    juce::Rectangle<int> selectionRectangle;

    enum class DraggingState
    {
        None,
        Point,
        Selection,
        Marquee
    };
    DraggingState draggingState = DraggingState::None;

    // Used for dragging single or multiple points
    juce::Point<float> dragAnchor; // The normalized position of the main point being dragged
    std::vector<juce::Point<float>> initialDragPositions; // Store original positions of all selected points

    bool isBrushing = false;
    juce::Point<int> lastBrushCell { -1, -1 };
    PointerGesture activePointerGesture = PointerGesture::none;
    juce::MouseInputSource::InputSourceType pointerSourceType =
        juce::MouseInputSource::mouse;
    int pointerSourceIndex = -1;
    bool primaryDoubleClickAuthorized = false;
    juce::MouseInputSource::InputSourceType doubleClickSourceType =
        juce::MouseInputSource::mouse;
    int doubleClickSourceIndex = -1;
    juce::int64 doubleClickAuthorizationStartMs = 0;
    juce::int64 doubleClickAuthorizationDeadlineMs = 0;
    std::uint64_t contextMenuGeneration = 0;
    bool contextMenuSessionActive = false;
    bool accessibilityHandlerHasBeenCreated = false;
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
    std::function<void()> contextMenuLaunchHook;
    int accessibilityStructureNotificationCount = 0;
#endif

    void selectAllPoints();
    void clearAllPoints();
    void copyShape();
    bool canPasteShape() const noexcept;
    bool pasteShape();
    void invertShape(bool invertX, bool invertY);
};

//==============================================================================
/** A ComboBox whose asynchronous result belongs to exactly one LFO editing
    session. JUCE's stock ComboBox callback only retains the component, so a
    result from a popup opened before a mode/data/LFO switch can otherwise
    select a brush in the replacement session.
*/
class LfoBrushSelector final : public juce::ComboBox
{
public:
    using SelectionCallback = std::function<void(LfoPresetShape)>;

    LfoBrushSelector() = default;

    void setSelectionCallback(SelectionCallback callback);
    void setInteractionAvailable(bool shouldBeAvailable) noexcept;
    void invalidateInteractionContext() noexcept;
    void dismissTransientInteraction() noexcept;
    void showPopup() override;

private:
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
    friend struct LfoPanelBrushTestAccess;
#endif

    bool keyPressed(const juce::KeyPress& key) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event,
                        const juce::MouseWheelDetails& wheel) override;
    void visibilityChanged() override;
    void enablementChanged() override;

    std::function<void(int)> createPopupResultHandler();
    std::function<void(int)> createPopupResultHandler(
        std::uint64_t contextGeneration);
    void capturePopupRequest() noexcept;
    bool isContextCurrent(std::uint64_t generation) const noexcept;
    bool commitSelection(int itemId, std::uint64_t generation);
    bool isCompletePrimaryDown(const juce::MouseEvent& event) const noexcept;
    bool isPointerSource(const juce::MouseEvent& event) const noexcept;
    void recoverMissingPointerUp(const juce::MouseEvent& event);
    void releasePointerInteractionWithoutSelection(
        const juce::MouseEvent& event);
    void clearPointerInteraction() noexcept;
    void cancelCurrentInteraction() noexcept;
    void closePopupWindow() noexcept;

    SelectionCallback selectionCallback;
    std::uint64_t interactionContextGeneration = 0;
    std::uint64_t popupRequestGeneration = 0;
    std::uint64_t popupSessionRevision = 0;
    std::uint64_t pointerInteractionGeneration = 0;
    juce::MouseInputSource::InputSourceType pointerSourceType =
        juce::MouseInputSource::mouse;
    int pointerSourceIndex = -1;
    bool interactionAvailable = false;
    bool popupRequestArmed = false;
    bool popupSessionActive = false;
    bool pointerInteractionActive = false;
    bool cancelPendingPointerRelease = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LfoBrushSelector)
};

//
//  The LfoPanel is the main "Controller" component.
//  It owns all the LFO data and all the UI controls.
//
class LfoPanel : public juce::Component,
                 public juce::Button::Listener,
                 public juce::Slider::Listener,
                 public juce::AudioProcessorValueTreeState::Listener,
                 public juce::AsyncUpdater
{
public:
    LfoPanel(FireAudioProcessor& p);
    ~LfoPanel() override;

    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;

    /** Called by the editor's shared UI clock. */
    void animationTick(float deltaSeconds = 1.0f / 60.0f);
    void showAssignArmed(int lfoIndex);
    void showAssignCompleted(int lfoIndex);
    void showAssignUnchanged(int lfoIndex);
    void showAssignCapacityReached();
    void showAssignCancelled();
    void clearAssignFeedback();

    void setScale(float newScale);
    void setOnDataChangedCallback(std::function<void()> callback);

    std::function<void()> onDataChanged;
    std::function<void(int lfoIndex)> onAssignButtonClicked;
    std::function<void(int lfoIndex)> onCurrentLfoChanged;
    PrimaryTextButton assignButton;

    void refreshLfoDisplay();
    void handleAsyncUpdate() override;
    void dismissTransientInteraction();
    void dismissModulationMatrixDialog();
    void visibilityChanged() override;
    void enablementChanged() override;

private:
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
    friend struct LfoPanelDialogTestAccess;
    friend struct LfoPanelBrushTestAccess;
#endif

    void buttonClicked(juce::Button* button) override;
    void sliderValueChanged(juce::Slider* slider) override;
    void sliderDragStarted(juce::Slider* slider) override;
    void sliderDragEnded(juce::Slider* slider) override;
    void setEditMode(LfoEditMode newMode);
    void styleButton(juce::Button& button, bool isToggle);
    void styleLfoSelectButton(juce::TextButton& button, juce::Colour colour);
    void configureModulationMatrixDialog(
        juce::DialogWindow::LaunchOptions& launchOptions);
    static void configureModulationMatrixDialogResizeLimits(
        juce::DialogWindow& dialog);
    void showModulationMatrixDialog();
    void setLfo(int newIndex);
    void displayLfoData(int index);

    FireAudioProcessor& processor;

    float scale = 1.0f;

    // --- Data Model ---
    // The LfoPanel owns the data for all 4 LFOs.
    int currentLfoIndex = 0;

    // --- UI Components ---
    LfoEditor lfoEditor;

    std::array<std::unique_ptr<PrimaryTextButton>, 4> lfoSelectButtons;

    // --- UI Components for mode selection ---
    PrimaryTextButton editModeButton { "Edit Mode" };
    PrimaryTextButton brushModeButton { "Brush Mode" };
    LfoBrushSelector brushSelector;

    PrimaryTextButton matrixButton { "Matrix" };
    PrimaryTextButton syncButton;

    PrimarySlider rateSlider;
    juce::Label rateLabel;

    PrimarySlider gridXSlider;
    juce::Label gridXLabel;
    PrimarySlider gridYSlider;
    juce::Label gridYLabel;

    PrimarySlider lfoSmoothSlider;
    juce::Label lfoSmoothLabel;

    PrimarySlider lfoPhaseSlider;
    juce::Label lfoPhaseLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rateSliderAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncButtonAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoSmoothAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoPhaseAttachment;
    bool isUpdatingRateSlider = false;
    bool isDraggingRateSlider = false;
    bool rateSliderRefreshWasDeferred = false;
    bool isDraggingPhaseSlider = false;
    std::atomic<bool> pendingRateSliderUpdate { false };
    std::atomic<unsigned int> pendingSmoothnessUpdates { 0 };
    std::array<juce::String, 4> syncParameterIDs;
    std::array<juce::String, 4> smoothParameterIDs;

    juce::Component::SafePointer<juce::DialogWindow> modulationMatrixDialog;
    std::uint64_t modulationMatrixDialogSessionGeneration = 0;
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
    std::function<juce::DialogWindow*()>
        modulationMatrixDialogFactoryForTesting;
#endif

    void updateRateSlider();

    void parameterChanged(const juce::String& parameterID, float newValue) override;

    juce::Rectangle<int> leftColumnArea;
    juce::Rectangle<int> centerColumnArea;
    juce::Rectangle<int> rightColumnArea;
    const std::array<juce::Colour, 4> lfoColours {
        fire::ui::colours::modulation,
        fire::ui::colours::signalCool,
        fire::ui::colours::positive,
        fire::ui::colours::gold
    };
    fire::ui::DampedValue lfoSelectionPosition;
    enum class AssignFeedback
    {
        idle,
        armed,
        completed,
        unchanged,
        capacityReached,
        cancelled
    };
    AssignFeedback assignFeedback = AssignFeedback::idle;
    float assignFeedbackSecondsRemaining = 0.0f;
    juce::Rectangle<int> separatorLine;
    juce::Rectangle<int> topRowArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LfoPanel)
};
