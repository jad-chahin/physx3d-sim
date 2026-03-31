#ifndef PHYSICS3D_APP_SCENESNAPSHOT_H
#define PHYSICS3D_APP_SCENESNAPSHOT_H

#include "render/SceneRenderer.h"

namespace app_loop {

struct RuntimeState;
class SimulationController;

void buildSceneSnapshot(
    const SimulationController& simulation,
    const RuntimeState& runtime,
    double alpha,
    render_scene::SceneSnapshot& snapshot);

} // namespace app_loop

#endif // PHYSICS3D_APP_SCENESNAPSHOT_H
