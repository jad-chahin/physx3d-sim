#include <iostream>

struct Vec3 {
    double x{}, y{}, z{};

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator*(double s) const { return {x * s, y * s, z * s}; }
};

struct Body {
    Vec3 position; // [m]
    Vec3 velocity; // [m/s]
    double invMass{}; // [1/kg]  (0 means "infinite mass" / static body)
    double radius{1.0}; // [m]
};

int main() {
    Body b;
    b.position = {0.0, 10.0, 0.0};
    b.velocity = {0.0, 0.0, 0.0};
    b.invMass = 1.0;
    b.radius = 1.0;

    const Vec3 gravity{0.0, -9.81, 0.0}; // [m/s^2] acceleration due to gravity
    constexpr int tickRate = 60; // [ticks/s]
    constexpr double dt = 1.0 / tickRate; // [s/tick]
    constexpr int simTime = 10; // [s]
    constexpr int ticks = tickRate * simTime; // [ticks]

    const double restitution = 0.5;
    const double groundY = 0.0;
    double minY = groundY + b.radius;

    for (int tick = 0; tick < ticks; ++tick) {
        const double t = tick * dt; // [s]

        std::cout << "t=" << t << "s  y=" << b.position.y << " m\n";

        b.velocity = b.velocity + gravity * dt;
        b.position = b.position + b.velocity * dt;

        if (b.position.y < minY) { // Hit ground -> bounce
            b.position.y = minY;
            if (b.velocity.y < 0.0) {
                b.velocity.y = -b.velocity.y * restitution;
            }
        }
    }
    return 0;
}
