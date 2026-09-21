# Trackball Gesture Development Notes

## Purpose

This document records the working implementation of the PMW3610 right-trackball gesture feature and the lessons from the implementation/debugging process.

The goal is to make future gesture work incremental and reproducible instead of re-inventing the input-processing path.

## Hardware and target behavior

- Pointing device: right-side PMW3610 trackball.
- Gesture activation: hold the I+O combo.
- While I+O is held:
  - physical left movement -> mouse back
  - physical right movement -> mouse forward
  - physical up/down -> no gesture
  - cursor movement is suppressed
- Without I+O, the trackball keeps its normal cursor behavior.
- One threshold crossing produces one gesture action.
- Keeping I+O held allows another roll to produce another action.

## What finally worked

The implementation uses the existing `kot149/zmk-mouse-gesture` module rather than a custom ZMK input processor.

The relevant pieces are:

1. `config/west.yml`
   - add the `kot149` remote if needed
   - add `zmk-mouse-gesture` at `v1`
   - add the `ssbb` remote and `zmk-listeners` at `v1`

2. `config/keymap.keymap`
   - include `<mouse-gesture.dtsi>`
   - configure `&zip_mouse_gesture`
   - use `stroke-size = <200>`
   - enable eager mode
   - enable `suppress-movement`
   - set `gesture-cooldown-ms = <0>`
   - map `GESTURE_LEFT` to `&mkp MB4`
   - map `GESTURE_RIGHT` to `&mkp MB5`
   - hold I+O on layer 7
   - use a layer listener to call `&mouse_gesture_on` / `&mouse_gesture_off`

3. `snippets/input-trackball-pmw3610/input-trackball-pmw3610.overlay`
   - keep the normal PMW3610 listener processing unchanged
   - add a layer-specific gesture processor on layer 7
   - apply `INPUT_TRANSFORM_XY_SWAP` immediately before `&zip_mouse_gesture`

The last point is important for this hardware. The PMW3610's physical orientation and the normal pointing-device transform do not line up with the gesture processor's logical axes. Applying the swap only for the gesture layer makes the physical left/right movement become logical left/right for gesture recognition without changing normal cursor behavior.

ZMK documents `INPUT_TRANSFORM_XY_SWAP` as swapping X and Y event types, and layer-specific input-processor overrides are supported by the input-processor system.

## Why the custom processor was removed

An earlier attempt implemented a new local mouse-gesture input processor in this repository. That was unnecessary and created additional C source, devicetree binding, and build-system changes.

Once the upstream `zmk-mouse-gesture` module was confirmed to work on the actual hardware, the local processor became dead code and was removed.

Do not reintroduce a custom processor unless the upstream module is proven insufficient for a concrete requirement.

## Important debugging lessons

### 1. Prove activation before changing axis mapping

A successful test of I+O + trackball movement triggering back/forward proved that:

- the combo was working
- layer 7 was active
- the layer listener was activating the gesture processor
- `&zip_mouse_gesture` was receiving movement
- the gesture binding was firing

At that point the remaining problem was axis orientation, not activation.

### 2. Separate gesture recognition from physical orientation

If the gesture fires on the wrong physical axis, first inspect the input transforms.

Do not immediately change:

- gesture activation
- combo definitions
- thresholds
- custom processor code

For this keyboard, the decisive fix was the gesture-layer `INPUT_TRANSFORM_XY_SWAP`.

### 3. Keep cursor suppression in the gesture processor

`suppress-movement` is part of the upstream mouse-gesture module. It consumes the X/Y movement while gesture recognition is active, which prevents the gesture roll from also moving the cursor.

### 4. Use the upstream module's semantics

The module already provides:

- activation behaviors
- gesture pattern matching
- eager execution
- movement suppression
- cooldown handling
- layer-specific use

Future work should extend these mechanisms instead of duplicating them locally.

## Adding future gestures

The preferred workflow is:

1. Decide the physical gesture and the desired behavior.
2. Confirm which logical gesture direction the current layer produces.
3. Add or adjust a pattern under `&zip_mouse_gesture`.
4. Bind the pattern directly to an existing ZMK behavior when possible.
5. Build.
6. Flash.
7. Test with the activation key held.
8. Only then change transforms or thresholds if the physical direction is wrong.

For example, a future multi-stroke gesture can use patterns such as:

```dts
some_action {
    pattern = <GESTURE_DOWN GESTURE_RIGHT>;
    bindings = <...>;
};
```

The module supports four-direction patterns and can fire them eagerly when configured.

## Safe tuning points

These are the first parameters to tune when behavior needs adjustment:

- `stroke-size`: how much movement is needed for one stroke.
- `enable-eager-mode`: whether a recognized pattern fires immediately.
- `suppress-movement`: whether gesture movement is prevented from reaching the cursor.
- `gesture-cooldown-ms`: cooldown after firing a gesture.
- `event-code-x` / `event-code-y`: only if the device uses non-standard event codes.
- layer-specific `zip_xy_transform`: for physical sensor orientation.

Do not change several of these at once. Change one thing, build, flash, and test.

## Current architecture

```
I+O held
  -> combo activates layer 7
  -> layer listener activates mouse gesture recognition
  -> layer 7 input processor:
       XY_SWAP
       -> mouse gesture recognition
  -> gesture pattern
  -> MB4 / MB5
  -> host interprets back / forward

I+O not held
  -> normal PMW3610 input path
  -> normal cursor behavior
```

## Reference implementations

The upstream implementation that was used as the basis is `kot149/zmk-mouse-gesture` v1.

The most useful upstream example is its README and its documented layer-specific gesture support.

ZMK's official input-processor documentation is also relevant, especially the transformer and layer-specific input-processor sections.

## Rule for future changes

Prefer this order:

**existing upstream implementation -> minimal configuration -> layer-specific transform -> only then custom code**

If an existing real implementation already matches the requirement, copy its architecture rather than inventing a new one.
