# Rendering Upgrade Spec

This document defines the target visual direction and hard constraints for the next graphics and lighting overhaul.

It is intentionally short and opinionated. The goal is to make future rendering work easier to evaluate, not to describe every possible feature.

## Visual Target

- Overall look: clean, grounded, high-clarity real-time lighting for a physics sandbox.
- Style: physically plausible and technical, not heavily stylized and not filmic for its own sake.
- Default scene mood: bright outdoor daylight with strong shape readability, clear shadowing, and stable depth cues.
- Materials should read through form, roughness, and lighting response more than through noisy textures.
- Motion and interaction should stay easy to read while the simulation is running at high speed.

## What "Better Graphics" Means Here

The upgrade is successful if it materially improves:

- object grounding in the world
- shape readability at a distance
- depth separation between foreground, midground, and background
- shadow quality and contact definition
- consistency of lighting across the scene

The upgrade is not successful if it mainly adds visual effects while making the simulation harder to read.

## Non-Goals

- photorealism at all costs
- many competing light types with no discipline
- overly cinematic post-processing
- shader feature sprawl driven by one-off toggles
- renderer complexity that is disproportionate to the needs of the project

## Platform and Technical Floor

- Rendering backend remains OpenGL 3.3 core for this phase.
- Desktop-only is acceptable for now.
- The current scene remains primarily primitive/procedural geometry unless a later phase intentionally changes that.
- The simulation, scene extraction, and renderer stay separate systems.

## Performance Targets

- Main quality target: steady 60 FPS at 1080p on a mainstream discrete GPU.
- Preferred experience target: 120 FPS at 1080p on stronger desktop hardware.
- GPU-heavy features must degrade cleanly through quality settings instead of forcing one expensive path.
- Rendering work must not compromise simulation responsiveness or UI clarity.

## Rendering Budgets

These are the default working constraints unless later profiling justifies changing them.

- One primary directional light with shadows is the main lighting path.
- Local lights are optional and should be added later only if they clearly help the scene.
- Post-processing should stay restrained: tone mapping is expected, bloom should be subtle, and additional passes need clear value.
- Full-scene effects that require multiple heavy fullscreen passes should not be introduced before the base lighting model is solid.

## Visual Guardrails

- Favor stable lighting over flashy lighting.
- Favor readable shadows over many shadows.
- Favor consistent materials over a large number of material parameters.
- Favor restrained atmospheric effects over dramatic haze.
- UI overlays must stay visually separate from the 3D scene and must remain readable against brighter lighting.

## Code and Architecture Guardrails

- Data flow stays one-way: simulation -> scene snapshot -> renderer input -> render passes.
- New rendering systems should land as explicit modules, not as growth inside one giant renderer file.
- Scene lighting, material logic, and post-processing should be separate responsibilities.
- Avoid shader permutation explosion. Prefer a small number of clear programs/passes.
- Performance instrumentation should be added alongside major rendering features, not after the system becomes slow.

## Quality Tiers

The renderer should be designed so these knobs can exist without architectural churn:

- shadow quality
- ambient or indirect lighting quality
- post-processing on or off
- local light count limits
- optional expensive effects such as ambient occlusion

## Acceptance Standard For The Upgrade

Before the rendering overhaul is considered "worth it," the project should be able to show:

- visibly improved directional lighting
- stable shadows that ground objects
- clearer material response under motion
- better depth perception without cluttering the image
- performance that still supports the simulation as the primary experience

## Implication For The Next Steps

The next implementation phases should optimize for:

1. clean renderer architecture
2. linear/HDR pipeline
3. directional light plus shadows
4. improved materials
5. ambient/environment lighting
6. restrained post-processing

Anything more advanced than that should justify both its visual gain and its maintenance cost.
