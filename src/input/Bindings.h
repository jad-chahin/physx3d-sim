#ifndef PHYSICS3D_BINDINGS_H
#define PHYSICS3D_BINDINGS_H

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>
#include <array>
#include <string>


namespace input {
    constexpr int kMouseBindingBase = -1000;
    constexpr int kMouseBindingMaxButtons = 8;

    struct ControlBindings {
        int freeze = GLFW_KEY_SPACE;
        int speedDown = GLFW_KEY_MINUS;
        int speedUp = GLFW_KEY_EQUAL;
        int speedReset = GLFW_KEY_1;
        int moveForward = GLFW_KEY_W;
        int moveBack = GLFW_KEY_S;
        int moveLeft = GLFW_KEY_A;
        int moveRight = GLFW_KEY_D;
        int moveUp = GLFW_KEY_E;
        int moveDown = GLFW_KEY_Q;
        int cameraBoost = GLFW_KEY_LEFT_SHIFT;
    };

    struct RebindAction {
        const char* label;
        const char* configKey;
        int ControlBindings::* member;
    };

    const std::array<RebindAction, 11>& rebindActions();

    bool isBindableKey(int key);
    std::string keyNameForCode(int key);
    bool isMouseBindingCode(int bindingCode);
    int bindingCodeFromMouseButton(int button);
    int mouseButtonFromBindingCode(int bindingCode);
    bool isBindingPressed(GLFWwindow* window, int bindingCode);

    void saveControlBindings(const ControlBindings& controls, const std::string& path);
    void loadControlBindings(ControlBindings& controls, const std::string& path);
}

#endif //PHYSICS3D_BINDINGS_H
