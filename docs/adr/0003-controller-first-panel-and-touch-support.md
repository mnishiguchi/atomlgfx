<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# ADR 0003: Controller-first panel and touch support

## Status

Accepted

## Context

`atomlgfx` currently supports a small panel-driver set and a touch path that was originally shaped around XPT2046-style SPI touch configuration.

That worked for the current supported boards and examples, but it creates a structural limitation for adding new hardware such as M5Stack Core2, which uses:

- LCD controller: `ILI9342C`
- touch controller: `FT6336U`

The main design question is not only whether to add support for those two controllers, but how to model future hardware support in a way that keeps the codebase maintainable and the public API understandable.

A narrow board-specific approach such as adding an early `:m5stack_core2` special case would solve one immediate bring-up path, but it would blur the distinction between:

- generic controller support
- board-level presets and examples

The touch side is the more important architecture decision.

Supporting `FT6336U` requires a more general model where touch is selected by controller type rather than inferred from an XPT2046-shaped SPI configuration surface.

Implementation work also clarified an additional boundary:

- generic controller support is not the same as full board bring-up

Some boards that share the same display and touch controllers still differ in how reset, backlight, interrupt, or power are routed. Those board-specific control paths may involve PMUs or I/O expanders and should not be forced into the generic controller layer too early.

A second practical clarification also emerged during implementation:

- the initial FT6336U rollout may arrive first through open-time controller configuration
- full build-default parity for non-XPT2046 touch controllers may follow later

This staged rollout is acceptable if the architectural direction is correct and the public model remains controller-first.

## Decision

`atomlgfx` will support new display and touch hardware through a controller-first configuration model.

1. Panel support remains controller-based.
   - Add `ILI9342C` as a supported `panel_driver`.
   - Continue to treat panel selection as a generic controller choice, not as a board-only choice.

2. Touch support becomes controller-based.
   - Introduce an explicit `touch_driver` configuration.
   - The initial generic touch-driver surface will support at least:
     - `:xpt2046`
     - `:ft6336u`

3. Driver-specific configuration is separated by controller family.
   - XPT2046-specific SPI configuration remains available for `:xpt2046`.
   - FT6336U-specific configuration is added in a controller-appropriate form.
   - We do not force new touch controllers into an XPT2046-shaped configuration model.

4. Generic controller support has a narrower meaning than full board bring-up.
   - Controller support means `atomlgfx` can configure and attach the controller through its normal generic configuration path.
   - Controller support does not by itself guarantee that all board-specific reset, backlight, interrupt, or power-management paths are handled.
   - Board-specific PMU or I/O-expander behavior belongs in a higher board layer when required.

5. The initial rollout may be staged.
   - FT6336U support may land first as an open-time controller configuration path.
   - Full build-default parity for additional touch-controller families may follow later.
   - This staged rollout is acceptable as long as the long-term model remains controller-first.

6. Touch capability and attachment semantics must become controller-aware.
   - Touch support must not be inferred only from XPT2046-specific signals such as a chip-select pin.
   - Effective touch attachment and capability advertisement must reflect the selected controller family and the conditions required for that family.

7. Board support is layered on top of controller support.
   - M5Stack Core2 support will be provided through controller support first.
   - A Core2 preset, helper, or example may be added later as a convenience layer.
   - Board presets must not replace the generic controller abstractions.

8. Public rendering APIs should remain stable where possible.
   - This decision primarily affects configuration, capability gating, and native attach paths.
   - Existing drawing and touch-read APIs should remain unchanged unless a clear incompatibility requires otherwise.

## Rationale

This decision keeps the architecture aligned with the existing direction of the project:

- generic public APIs
- native-layer ownership of hardware-specific behavior
- clear separation between configuration, device logic, and examples

A controller-first model is a better long-term fit than a board-first shortcut.

It avoids several problems:

- growing the codebase through one-off board special cases
- baking SPI-specific assumptions into all future touch support
- making the supported surface harder to explain and test
- conflating controller support with full board support

This direction also keeps future work reusable.

Once `ILI9342C` and `FT6336U` are modeled as first-class controllers, the same infrastructure can support other boards that use the same or similar hardware without needing a fresh architecture decision each time.

At the same time, keeping controller support distinct from board bring-up gives the project room to handle boards that need extra reset, power, backlight, or interrupt orchestration without polluting the generic controller layer with board-specific glue too early.

The staged rollout decision is also intentional.

It is better to land the correct controller model first than to preserve a misleading XPT2046-shaped abstraction only because the build-default surface has not yet been generalized.

## Consequences

### Positive

- Makes `ILI9342C` support reusable beyond one board
- Makes `FT6336U` support reusable beyond one board
- Removes the current XPT2046-shaped limitation from touch configuration
- Keeps board presets as a convenience layer rather than the primary abstraction
- Makes future controller additions easier to reason about
- Clarifies that controller support and board bring-up are related but distinct milestones
- Allows incremental rollout without committing to the wrong abstraction

### Negative

- Requires refactoring of the current touch configuration path
- Increases configuration and validation complexity compared with a single-controller touch model
- Adds more native attach and capability branches to test
- May require migration care to preserve existing XPT2046 behavior cleanly
- Some boards that share the same controllers may still require additional board-specific initialization layers
- The initial rollout may temporarily leave build-default touch configuration less symmetric than the open-time configuration surface

## Rejected alternatives

### Alternative 1: add only a board-specific `:m5stack_core2` path

Rejected.

This would solve one immediate target but would not address the deeper architectural problem that touch support is currently too tightly shaped around one controller family.

### Alternative 2: add `ILI9342C` now and postpone the touch-model refactor indefinitely

Rejected.

The LCD addition is useful, but the larger long-term decision is how touch support is modeled. Delaying that indefinitely would make future controller support harder to add cleanly.

### Alternative 3: keep touch support implicitly XPT2046-shaped and bolt FT6336U onto the same configuration surface

Rejected.

This would preserve short-term familiarity at the cost of a misleading and brittle abstraction.

### Alternative 4: treat controller support as equivalent to full board support

Rejected.

Some boards sharing the same controllers still differ in reset, backlight, interrupt, or power plumbing. Treating controller support as full board support would overstate what the generic layer actually provides.

### Alternative 5: model all board-control resources immediately in generic open configuration

Rejected for now.

A fully generic surface for PMUs, I/O expanders, board-controlled reset lines, and board-controlled backlights may become useful later, but introducing that abstraction now would add complexity before the controller-first layer is fully established.

## Follow-up implications

- Add `ILI9342C` to the supported panel-driver configuration and native device attach path.
- Introduce explicit `touch_driver` selection.
- Refactor touch configuration validation to separate controller-specific fields.
- Add native `FT6336U` support.
- Update touch capability and effective-attachment logic so it is controller-aware rather than CS-only.
- Update the LovyanGFX build surface to include the minimum sources required by the selected controller families.
- Add an M5Stack Core2 preset, helper, or example after generic controller support is stable.
- Preserve existing XPT2046 support during the refactor.
