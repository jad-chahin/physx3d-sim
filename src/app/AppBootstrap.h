#ifndef PHYSICS3D_APP_BOOTSTRAP_H
#define PHYSICS3D_APP_BOOTSTRAP_H

#include "app/AppRuntime.h"

struct GLFWwindow;

namespace app_loop {

void configureWindow(GLFWwindow* window, AppLoopState& appState);
bool initGlad();
void configureOpenGl();
void printGlInfo();

} // namespace app_loop

#endif // PHYSICS3D_APP_BOOTSTRAP_H
