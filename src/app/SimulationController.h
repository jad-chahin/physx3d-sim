#ifndef PHYSICS3D_APP_SIMULATIONCONTROLLER_H
#define PHYSICS3D_APP_SIMULATIONCONTROLLER_H

#include <vector>

#include "app/AppRuntime.h"
#include "input/Bindings.h"
#include "sim/World.h"
#include "ui/PauseMenu.h"

struct GLFWwindow;

namespace app_loop {

class SimulationController {
public:
    explicit SimulationController(sim::World world);

    std::vector<sim::Body>& mutableBodies();
    [[nodiscard]] const std::vector<sim::Body>& bodies() const;
    [[nodiscard]] bool hasBodies() const;

    void applySettings(const ui::SimulationSettings& simSettings);
    void handleHotkeys(
        GLFWwindow* window,
        const input::ControlBindings& controls,
        const ui::SimulationSettings& simSettings,
        bool pauseMenuOpen,
        RuntimeState& runtime);
    void step(RuntimeState& runtime, bool pauseMenuOpen, double frameTime);
    void reset(RuntimeState& runtime, const ui::SimulationSettings& simSettings);

private:
    sim::World world_;

    void syncPreviousState_();
};

} // namespace app_loop

#endif // PHYSICS3D_APP_SIMULATIONCONTROLLER_H
