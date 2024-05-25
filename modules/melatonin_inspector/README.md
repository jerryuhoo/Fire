![](https://github.com/sudara/melatonin_inspector/actions/workflows/ci.yml/badge.svg)

## Melatonin Component Inspector

A JUCE module that gives you the ability to inspect and visually edit (non-destructively) components in your UI.

It's inspired by [Figma](https://figma.com) (where I prefer to design UI), web browser web inspectors and Jim Credland's [Component Debugger](https://github.com/jcredland/juce-toys/blob/master/jcf_debug/source/component_debugger.cpp) [juce-toys](https://github.com/jcredland/juce-toys). 

A big hearty thanks to [Dmytro Kiro](https://github.com/dikadk) and [Roland Rabien (aka FigBug)](https://github.com/figbug) for contributing some great features!

<p align="center">
<img src="https://user-images.githubusercontent.com/472/213710642-389d4313-5b5a-4656-859e-ea014c859021.jpg" width="500"/>
</p>


<p align="center">
✨<br/>
✨✨<br/>
✨✨✨<br/>
<b><i>...the features...</i></b><br/>
✨✨✨<br/>
✨✨<br/>
✨<br/>
</p>

## Browse and select components visually

Point n' click to inspect a component, see its size and distance to parent.

<p align="center">
<img src="https://user-images.githubusercontent.com/472/213708404-90948d45-6440-455d-bac0-662acf5a1d70.gif" width="600"/>
</p>


## Explore the component hierarchy

Immediately gain clarity over parent/child relationships and see what components are currently visible.

<p align="center">
<img src="https://user-images.githubusercontent.com/472/213709528-b0f4d3dc-61bb-4e72-aae5-7c414ee5e798.gif" width="400"/>
</p>

Filter components by name. Names are derived from stock components, label/button text, or demangled class names.


<p align="center">
<img src="https://user-images.githubusercontent.com/472/213702600-2e479677-8a6e-459b-ba38-1db93be689e3.gif" width="400"/>
</p>


## Preview Component

See what exactly is drawing on a per-component basis, even when the component is hidden. A fixed transparency grid helps you understand which components and images have transparency.

<p align="center">
<img src="https://github.com/sudara/melatonin_inspector/assets/472/429e4ce6-cc6c-4ca0-ba7c-201272234c6b" width="600"/>
</p>

## Edit component position and spacing

There's like...4 different ways to do this, visually and numerically...

<p align="center">
<img src="https://user-images.githubusercontent.com/472/213701508-eaf1dfc6-4465-4581-915e-7d788d47e08c.gif" width="600"/>
</p>

We also display component *padding* if you follow the convention of storing them as the component properties  `paddingLeft`, `paddingTop`, `paddingRight`, `paddingBottom`. See my `PaddedComponent` [base class as an example](https://gist.github.com/sudara/eed6b8bb3b960b4c2156a0883913ea15).

<p align="center">
<img src="https://user-images.githubusercontent.com/472/213940575-0f9fa289-6b00-4899-b07e-9c9ae9e500c9.gif" width="600"/>
</p>

## Inspect and modify component flags and properties

See the most important component properties at a glance, including look and feels, fonts for labels, etc. Where applicable, flags are editable! 

<p align="center">
<img src="https://user-images.githubusercontent.com/472/213707219-dc8619c1-8800-4267-b3f3-0c9911817a63.jpg" width="600"/>
</p>


Any custom properties you've added the component will also show up here and be editable. 

![AudioPluginHost - 2023-08-14 01](https://github.com/sudara/melatonin_inspector/assets/472/3c69c652-5468-409b-9e3c-134868f4db9c)

## Nudge components around 

Verify new values, get things pixel perfect.

<p align="center">
<img src="https://user-images.githubusercontent.com/472/213701074-a48414e8-a86c-4881-a911-392710078ce7.gif" width="600"/>
</p>

## View spacing relative to siblings/neighbors

Hold "alt" while component is selected. A Figma inspired feature.

<p align="center">
<img src="https://user-images.githubusercontent.com/472/213705531-d9887c7b-15a7-4b78-bab4-6868765d45a5.gif" width="450"/>
</p>

## Display and modify JUCE widget colors

No, it's not a christmas miracle, but we do [magically](https://github.com/sudara/melatonin_inspector/blob/main/update_juce_colours.rb) display JUCE's friendly `enum ColourIds` names from the stock widgets. 

See what that Slider's `trackColourId` is set to, and hey, go ahead and try out a new theme in real time.

(Just to be clear: The color changes are temporary, no code is edited!)


![AudioPluginHost - 2023-08-14 38](https://github.com/sudara/melatonin_inspector/assets/472/09e0fdd7-1e31-4956-8808-781920c64a66)

## Color picker

Accurately pinpoint colors. Click to pick and store one. Toggle between RGBA and HEX values.

<p align="center">
<img src="https://github.com/sudara/melatonin_inspector/assets/472/9d9d1fe8-b34a-4e1f-a71c-26f0a0ef5169" width="450"/>
</p>


## FPS meter

Overlay an FPS meter on your Editor to get an intuitive understanding of your painting performance. Please see the [FAQ](https://github.com/sudara/melatonin_inspector#my-fps-seems-low-is-it-accurate) for details on usage.

<p align="center">
<img src="https://github.com/sudara/melatonin_inspector/assets/472/06ee7e30-a536-4dc3-a54e-13aba3a2b0c0" width="600"/>
</p>


## Display component performance in real time

A life saving feature. 

See time spent exclusively in a component's `paint` method as well as conveniently provide you with a sum with all children.

Keep track of the max. Double click to `repaint` and get fresh timings. See [setup paint timing](#7-optional-setup-component-timing).

![AudioPluginHost - 2023-08-16 57](https://github.com/sudara/melatonin_inspector/assets/472/7b08ea30-ebd1-4900-bb67-02bb8393211b)


## Installing with CMake

### CMake option #1: `FetchContent`

Place this chunk o love somewhere before your call to `juce_add_plugin` or `juce_add_gui_app`:

```cmake
Include (FetchContent)
FetchContent_Declare (melatonin_inspector
  GIT_REPOSITORY https://github.com/sudara/melatonin_inspector.git
  GIT_TAG origin/main
  SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/melatonin_inspector)
FetchContent_MakeAvailable (melatonin_inspector)
```

### CMake option #2 git submodules

If you are a git submodule aficionado, life is great! Add this submodule to your project:
```sh
git submodule add -b main https://github.com/sudara/melatonin_inspector.git modules/melatonin_inspector
```
and then simply call `add_subdirectory` in your CMakeLists.txt. Remember, modules go *before* your main call to `juce_add_plugin` or `juce_add_gui_app` ([this makes life easier in IDEs](https://github.com/juce-framework/JUCE/blob/master/docs/CMake%20API.md#juce_enable_module_source_groups)):

```cmake
add_subdirectory (modules/melatonin_inspector)
```

To update melatonin_inspector down the road (gasp! maintained dependencies!?):
```git
git submodule update --remote --merge modules/melatonin_inspector
```

### CMake Step 2: Tell JUCE about the module

Wait wait, not so fast! You couldn't get away that easily. 

*After* your `juce_add_plugin` call you will need to link your plugin to the module's target, for example:

```
target_link_libraries("YourProject" PRIVATE melatonin_inspector)
```

Note: you don't have to call `juce_add_module`. That's handled by our CMake.

If you use Projucer, add the module manually.
 
## Installing with Projucer

If you're rolling old school, or just prefer Projucer life, you'll be happy to note that though JUCE doesn't make it easy we've bent over backwards to make sure our icons, etc are included in the module.

### Download the module 

You can still use git to add it as a submodule if you'd like stay up to date with any changes:

```
git submodule add -b main https://github.com/sudara/melatonin_inspector.git modules/melatonin_inspector
```

Or just download it and stick it somewhere.

### Add it to the projucer

Just "Add a module from a specified folder" and you're done!

<p align="center">
<img src="https://github.com/sudara/melatonin_inspector/assets/472/010d9bf3-f8dc-4fc1-9039-69ba42ff856c" width="500"/>
</p>

## 2. Add an include to your Plugin Editor

Include the module header:

```cpp
#include "melatonin_inspector/melatonin_inspector.h"
```

## 3. Add the inspector as a private member to your Editor

The easiest way to get started is to pass a reference to the root component of your UI (typically the Editor itself like in this example, but you could also inspect anything that derives from `juce::Component`).

```cpp
melatonin::Inspector inspector { *this };
```

If you prefer the inspector open in the disabled state by default, you can pass false as the second argument.

```cpp
melatonin::Inspector inspector { *this, false };
```

## 4. Set it visible

When the inspector as an editor member, you can use `cmd/ctrl i` to toggle whether the inspector is enabled.

`setVisible` on the member will also pop the window open.

What I do is have a GUI toggle that pops open the window and enables inspection:

```cpp
// open the inspector window
inspector.setVisible(true); 

// enable the inspector
inspector.toggle(true);
```

## 5. Optional: Make it smarter 

Setting up as above means that the inspector will always be constructed with your editor. Clicking close on the inspector's `DocumentWindow` will just hide it while disabling inspection. 

If you wrap the inspector with `#if DEBUG` this might be fine for you.

However, if you'd plan to ship a product that includes the inspector, or otherwise want to lazily construct it to be more efficient, use a `unique_ptr` instead and set the `onClose` callback to reset the pointer.

```c++
// PluginEditor.h
std::unique_ptr<melatonin::Inspector> inspector;

// in some button on-click logic
// replace `this` with a reference to your editor if necessary
if (!inspector)
{
    inspector = std::make_unique<melatonin::Inspector> (*this);
    inspector->onClose = [this]() { inspector.reset(); };
}

inspector->setVisible (true);
```
Thanks to @FigBug for this feature.

## 6. Optional: Setup component timing

Just `#include modules/melatonin_inspector/melatonin/helpers/timing.h` and then call the RAII helper ***at the top*** of a component's paint method:

```c++
void paint (juce::Graphics& g) override
{
    melatonin::ComponentTimer timer { this };

    // do all your expensive painting...
 ```

This simply times the method and stores it in the component's own properties. It will store up to 3 values named `timing1`, `timing2`, `timing3`.

Want automatic timings for every JUCE component, including stock widgets? [Upvote this FR](https://forum.juce.com/t/fr-callback-or-other-mechanism-for-exposing-component-debugging-timing/54481/1).

Want timings for your custom components ***right now***? Do what I do and derive all your components from a `juce::Component` subclass which wraps the `paint` call and adds the helper before `paint` is called. 

Check out [the forum post for detail](https://forum.juce.com/t/fr-callback-or-other-mechanism-for-exposing-component-debugging-timing/54481/11?u=sudara). Or, if you run a JUCE fork, you might prefer [Roland's solution](https://forum.juce.com/t/fr-callback-or-other-mechanism-for-exposing-component-debugging-timing/54481/6?u=sudara).

## FAQ

### Can I use this in a GUI app/standalone?

Yup! See the tests folder for an example.

### Can I save my component resizes or edits?

Nope! 

For that, one would need a component system relying on data for placement and size vs. code. See [Daniel's Foley GUI Magic](https://github.com/ffAudio/foleys_gui_magic).

### How is the component hierarchy created?

It traverses components from the root, building a `TreeView`. 

In the special case of `TabbedComponent`, each tab is added as a child. 

### My FPS seems low, is it accurate?

It's a smoothed running average.

If you see low FPS rates, check the following:

* Are you running *just* your plugin? Make sure other plugin UIs are closed.
* I optimize the inspector for Debug usage, but it can be tough for complex UIs to hit 60fps in Debug, especially on macOS (see note below). See what happens in Release.
* You might have legitimately expensive paint calls (esp. in Debug). You can verify this out via [Perfetto](https://github.com/sudara/melatonin_perfetto).

On recent macOS, a `repaint()` on even small sections of a window (ie, what the FPS meter does) will cause the OS to paint the entire plugin window. You can use `Flash Screen Updates` in Quartz Debug to verify this.  Because of this macOS behavior, the FPS meter will actually trigger full repaints of your UI, so anything expensive (especially in Debug) will slow down what the FPS meter reports.

If you are using the JUCE flag `JUCE_COREGRAPHICS_RENDER_WITH_MULTIPLE_PAINT_CALLS`, JUCE will internally manage the rectangles that need to be repainted, with the aim of being more precise/hygenic with what actually gets painted. This might be a good choice if your plugin already frequently repainting parts of the UI. But please don't switch over to that flag just to appease the FPS meter! It needs to be a choice you make depending on your internal testing (without the FPS meter in play).


Feel free to ask for other ideas in the [forum thread](https://forum.juce.com/t/melatonin-inspector-a-web-inspector-ish-module-for-juce-components/45672).

### I have a problem / feature request

Please do submit an Issue or PR on the repository! Or visit the [official thread on the JUCE forum](https://forum.juce.com/t/melatonin-inspector-a-web-inspector-ish-module-for-juce-components/45672).


## Contributing

Contributions are always welcome! 

The inspector wouldn't be half as awesome without the help of the community.

If you'd like to contribute, look out for the issues tagged with "[Good First Issue](https://github.com/sudara/melatonin_inspector/issues?q=is:issue+is:open+label:%22good+first+issue%22)"

Note that CI tests for compilation and treats errors on both macOS and Windows as errors. 

### Assets

All assets are PNG exported at 2x. 

Please see the CMakelists.txt file for details on how to add icons in a Projucer friendly way. There's a script for it!
