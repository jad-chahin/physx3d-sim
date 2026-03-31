#ifndef PHYSICS3D_APP_WINDOWING_H
#define PHYSICS3D_APP_WINDOWING_H

#include "app/AppRuntime.h"

struct GLFWwindow;

namespace app_loop {

class GlfwSession {
public:
    GlfwSession() = default;
    GlfwSession(const GlfwSession&) = delete;
    GlfwSession& operator=(const GlfwSession&) = delete;
    ~GlfwSession();

    bool init();

private:
    bool active_ = false;
};

class WindowHandle {
public:
    WindowHandle() = default;
    WindowHandle(const WindowHandle&) = delete;
    WindowHandle& operator=(const WindowHandle&) = delete;
    ~WindowHandle();

    void reset(GLFWwindow* window);
    GLFWwindow* get() const;
    operator GLFWwindow*() const;

private:
    GLFWwindow* window_ = nullptr;
};

double consumeScrollDeltaY(AppLoopState& state);
int consumeLastPressedKey(AppLoopState& state);

} // namespace app_loop

#endif // PHYSICS3D_APP_WINDOWING_H
