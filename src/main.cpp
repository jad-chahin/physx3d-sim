#include <iostream>

#include "sim/World.h"
#include <vector>

int main() {
    // Bodies
    sim::Body a{};
    a.invMass = 1.0;

    sim::Body b{};
    b.position = sim::Vec3(10.0, 0.0, 0.0);
    b.velocity = sim::Vec3(-1.0, 0.0, 0.0);
    b.invMass = 0.5;
    b.radius = 2.0;

    std::vector<sim::Body> bodies;
    bodies.push_back(a);
    bodies.push_back(b);

    // World initialization
    auto world = sim::World(std::move(bodies));


    // Tick loop
    constexpr double tr = 60.0; // Tick rate (Ticks per second)
    constexpr double s = 10.0; // Seconds
    constexpr double dt = 1.0 / tr;

    for (int t = 0; t < tr * s; ++t) {
        world.step(dt);
    }
    const sim::Body& A = world.bodies()[0];
    const sim::Body& B = world.bodies()[1];

    std::cout << "a: " << "(" << A.position.x << ", " << A.position.y << ", " << A.position.z << ")" << std::endl;
    std::cout << "b: " << "(" << B.position.x << ", " << B.position.y << ", " << B.position.z << ")" << std::endl;
}
