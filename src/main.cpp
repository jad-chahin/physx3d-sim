#include "app/AppLoop.h"
#include "sim/DefaultScene.h"


int main() {
    return app::runApp(sim::makeDefaultWorld());
}
