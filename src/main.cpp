#include "app/AppLoop.h"
#include "sim/DefaultWorld.h"


int main() {
    return app::runApp(sim::makeDefaultWorld());
}
