#include "sim/World.h"
#include <vector>

int main() {
    // Bodies
    sim::Body a{};
    a.invMass = 1.0;

    sim::Body b{};
    b.position = sim::Vec3(10.0, 0.0, 0.0);
    b.velocity = sim::Vec3(0.0, 1.0, 0.0);
    b.invMass = 2.0;
    b.radius = 2.0;

    std::vector<sim::Body> bodies;
    bodies.push_back(a);
    bodies.push_back(b);

    // World initialization
    sim::World world = sim::World(std::move(bodies));


    // Tick loop
    double tr = 60.0; // Tick rate (Ticks per second)
    double s = 10.0; // Seconds
    double dt = 1.0 / tr;

    for (int t = 0; t < tr * s; ++t) {
        world.step(dt);
    }
}
