#include <cmath>
#include <exception>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "sim/Broadphase.h"
#include "sim/Collision.h"
#include "sim/DefaultWorld.h"
#include "sim/Material.h"
#include "sim/World.h"

void appendUiPauseMenuTests(std::vector<std::pair<std::string, std::function<void()>>>& tests);

namespace {
    using sim::Body;
    using sim::Vec3;

    [[nodiscard]] Body makeDynamicBody(const Vec3& position, const double radius, const double mass)
    {
        Body body{};
        body.position = position;
        body.prevPosition = position;
        body.radius = radius;
        body.invMass = mass > 0.0 ? (1.0 / mass) : 0.0;
        sim::assignMaterial(body, sim::kDefaultMaterialName);
        return body;
    }

    [[nodiscard]] Body makeStaticBody(const Vec3& position, const double radius)
    {
        Body body = makeDynamicBody(position, radius, 0.0);
        body.invMass = 0.0;
        return body;
    }

    void require(const bool condition, const std::string& message)
    {
        if (!condition) {
            throw std::runtime_error(message);
        }
    }

    void testBroadphaseSweptPairDetection()
    {
        std::vector<Body> bodies;
        Body mover = makeDynamicBody(Vec3(-10.0, 0.0, 0.0), 1.0, 1.0);
        mover.velocity = Vec3(30.0, 0.0, 0.0);
        bodies.push_back(mover);
        bodies.push_back(makeStaticBody(Vec3(0.0, 0.0, 0.0), 1.0));
        bodies.push_back(makeStaticBody(Vec3(0.0, 50.0, 0.0), 1.0));

        std::vector<sim::broadphase::Pair> pairs;
        sim::broadphase::sweptPairs(bodies, 0.5, pairs);
        require(pairs.size() == 1 && pairs[0].first == 0 && pairs[0].second == 1,
            "swept broadphase should isolate the imminent impact pair");
    }

    void testCollisionModeSplit()
    {
        Body a = makeDynamicBody(Vec3(0.0, 0.0, 0.0), 1.0, 1.0);
        Body b = makeDynamicBody(Vec3(1.5, 0.0, 0.0), 1.0, 1.0);
        a.velocity = Vec3(1.0, 0.0, 0.0);
        b.velocity = Vec3(-1.0, 0.0, 0.0);

        sim::collision::SolveParams positionOnly{};
        positionOnly.applyVelocityImpulse = false;
        positionOnly.applyFrictionImpulse = false;
        sim::collision::solveCollisionPair(a, b, positionOnly, false);
        require(a.velocity.x == 1.0 && b.velocity.x == -1.0,
            "position-only solve should not change velocities");

        sim::collision::SolveParams fullSolve{};
        const auto stats = sim::collision::solveCollisionPair(a, b, fullSolve, true);
        require(stats.impulseApplied, "full collision solve should apply an impulse");
    }

    void testWorldCcdPreventsTunneling()
    {
        sim::World::Params params{};
        params.enableGravity = false;
        params.enableCollisions = true;
        params.velocityIterations = 4;
        params.positionIterations = 2;

        std::vector<Body> bodies;
        Body mover = makeDynamicBody(Vec3(-5.0, 0.0, 0.0), 0.5, 1.0);
        mover.velocity = Vec3(40.0, 0.0, 0.0);
        bodies.push_back(mover);
        bodies.push_back(makeStaticBody(Vec3(0.0, 0.0, 0.0), 0.5));

        sim::World world(std::move(bodies), params);
        world.step(0.2);

        const auto& steppedBodies = world.bodies();
        require(steppedBodies[0].position.x < 0.6,
            "ccd should stop the fast-moving body before it tunnels through the target");
    }

    void testSleepingAndWarmStart()
    {
        sim::World::Params params{};
        params.enableGravity = true;
        params.enableCollisions = true;
        params.enableSleeping = true;
        params.velocityIterations = 4;
        params.positionIterations = 4;
        params.sleepTime = 0.05;
        params.sleepLinearThreshold = 0.5;
        params.sleepAngularThreshold = 0.5;

        std::vector<Body> bodies;
        bodies.push_back(makeStaticBody(Vec3(0.0, 0.0, 0.0), 1.0));
        bodies.push_back(makeDynamicBody(Vec3(1.9, 0.0, 0.0), 1.0, 1.0));

        sim::World world(std::move(bodies), params);
        world.step(1.0 / 60.0);
        world.step(1.0 / 60.0);
        world.step(1.0 / 60.0);
        world.step(1.0 / 60.0);

        require(world.metrics().warmStartedPairs > 0 || world.metrics().manifoldActivePairs > 0,
            "repeated resting contact should retain persistent manifold state");
        require(world.bodies()[1].sleeping, "resting body should transition to sleeping");
    }

    void testSanitizationRemovesInvalidState()
    {
        sim::World::Params params{};
        params.enableGravity = false;

        Body badBody = makeDynamicBody(Vec3(0.0, 0.0, 0.0), 1.0, 1.0);
        badBody.position.x = std::nan("");
        badBody.velocity.y = std::nan("");
        badBody.radius = -4.0;

        sim::World world(std::vector<Body>{badBody}, params);
        world.step(1.0 / 60.0);

        const Body& repaired = world.bodies().front();
        require(std::isfinite(repaired.position.x) && std::isfinite(repaired.velocity.y),
            "world step should sanitize invalid numeric state");
        require(repaired.radius > 0.0, "world step should sanitize invalid body radius");
    }
}

int main()
{
    std::vector<std::pair<std::string, std::function<void()>>> tests{
        {"broadphase_swept_pair_detection", testBroadphaseSweptPairDetection},
        {"collision_mode_split", testCollisionModeSplit},
        {"world_ccd_prevents_tunneling", testWorldCcdPreventsTunneling},
        {"sleeping_and_warm_start", testSleepingAndWarmStart},
        {"sanitization_removes_invalid_state", testSanitizationRemovesInvalidState},
    };
    appendUiPauseMenuTests(tests);

    int failures = 0;
    for (const auto& [name, test] : tests) {
        try {
            test();
            std::cout << "[PASS] " << name << "\n";
        } catch (const std::exception& ex) {
            ++failures;
            std::cout << "[FAIL] " << name << ": " << ex.what() << "\n";
        }
    }

    if (failures != 0) {
        std::cout << failures << " test(s) failed\n";
        return 1;
    }

    std::cout << tests.size() << " test(s) passed\n";
    return 0;
}

