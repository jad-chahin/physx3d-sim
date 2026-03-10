//
// Created by Codex on 2026-03-07.
//

#include "input/Bindings.h"

#include <cctype>
#include <fstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>


namespace input {
    namespace {
        constexpr std::array<RebindAction, 11> kRebindActions{{
            {"FREEZE", "FREEZE", &ControlBindings::freeze},
            {"SPEED DOWN", "SPEED_DOWN", &ControlBindings::speedDown},
            {"SPEED UP", "SPEED_UP", &ControlBindings::speedUp},
            {"SPEED RESET", "SPEED_RESET", &ControlBindings::speedReset},
            {"MOVE FORWARD", "MOVE_FORWARD", &ControlBindings::moveForward},
            {"MOVE BACK", "MOVE_BACK", &ControlBindings::moveBack},
            {"MOVE LEFT", "MOVE_LEFT", &ControlBindings::moveLeft},
            {"MOVE RIGHT", "MOVE_RIGHT", &ControlBindings::moveRight},
            {"MOVE UP", "MOVE_UP", &ControlBindings::moveUp},
            {"MOVE DOWN", "MOVE_DOWN", &ControlBindings::moveDown},
            {"BOOST", "CAMERA_BOOST", &ControlBindings::cameraBoost},
        }};

        struct KeyNameEntry {
            int code;
            const char* name;
            bool canonical;
        };

        struct MouseBindingEntry {
            int button;
            const char* name;
            bool canonical;
        };

        constexpr std::array<KeyNameEntry, 44> kKeyNames{{
            {GLFW_KEY_SPACE, "SPACE", true},
            {GLFW_KEY_MINUS, "MINUS", true},
            {GLFW_KEY_EQUAL, "EQUAL", true},
            {GLFW_KEY_LEFT_SHIFT, "LSHIFT", true},
            {GLFW_KEY_RIGHT_SHIFT, "RSHIFT", true},
            {GLFW_KEY_LEFT_SHIFT, "LEFT_SHIFT", false},
            {GLFW_KEY_RIGHT_SHIFT, "RIGHT_SHIFT", false},
            {GLFW_KEY_0, "0", true},
            {GLFW_KEY_1, "1", true},
            {GLFW_KEY_2, "2", true},
            {GLFW_KEY_3, "3", true},
            {GLFW_KEY_4, "4", true},
            {GLFW_KEY_5, "5", true},
            {GLFW_KEY_6, "6", true},
            {GLFW_KEY_7, "7", true},
            {GLFW_KEY_8, "8", true},
            {GLFW_KEY_9, "9", true},
            {GLFW_KEY_A, "A", true},
            {GLFW_KEY_B, "B", true},
            {GLFW_KEY_C, "C", true},
            {GLFW_KEY_D, "D", true},
            {GLFW_KEY_E, "E", true},
            {GLFW_KEY_F, "F", true},
            {GLFW_KEY_G, "G", true},
            {GLFW_KEY_H, "H", true},
            {GLFW_KEY_I, "I", true},
            {GLFW_KEY_J, "J", true},
            {GLFW_KEY_K, "K", true},
            {GLFW_KEY_L, "L", true},
            {GLFW_KEY_M, "M", true},
            {GLFW_KEY_N, "N", true},
            {GLFW_KEY_O, "O", true},
            {GLFW_KEY_P, "P", true},
            {GLFW_KEY_Q, "Q", true},
            {GLFW_KEY_R, "R", true},
            {GLFW_KEY_S, "S", true},
            {GLFW_KEY_T, "T", true},
            {GLFW_KEY_U, "U", true},
            {GLFW_KEY_V, "V", true},
            {GLFW_KEY_W, "W", true},
            {GLFW_KEY_X, "X", true},
            {GLFW_KEY_Y, "Y", true},
            {GLFW_KEY_Z, "Z", true},
            {GLFW_KEY_UNKNOWN, "UNKNOWN", false},
        }};

        constexpr std::array<MouseBindingEntry, 11> kMouseBindingNames{{
            {GLFW_MOUSE_BUTTON_LEFT, "MOUSE1", true},
            {GLFW_MOUSE_BUTTON_RIGHT, "MOUSE2", true},
            {GLFW_MOUSE_BUTTON_MIDDLE, "MOUSE3", true},
            {3, "MOUSE4", true},
            {4, "MOUSE5", true},
            {5, "MOUSE6", true},
            {6, "MOUSE7", true},
            {7, "MOUSE8", true},
            {GLFW_MOUSE_BUTTON_LEFT, "MOUSE_LEFT", false},
            {GLFW_MOUSE_BUTTON_RIGHT, "MOUSE_RIGHT", false},
            {GLFW_MOUSE_BUTTON_MIDDLE, "MOUSE_MIDDLE", false},
        }};

        int g_lastPressedKey = GLFW_KEY_UNKNOWN;

        std::string trimCopy(const std::string& s) {
            const auto first = s.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) {
                return "";
            }
            const auto last = s.find_last_not_of(" \t\r\n");
            return s.substr(first, last - first + 1);
        }

        std::string toUpperCopy(std::string s) {
            for (char& c : s) {
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            }
            return s;
        }

        const KeyNameEntry* findKeyEntryByCode(const int key) {
            for (const auto& keyEntry : kKeyNames) {
                if (keyEntry.code != key) {
                    continue;
                }
                if (!keyEntry.canonical) {
                    continue;
                }
                return &keyEntry;
            }
            return nullptr;
        }

        const KeyNameEntry* findKeyEntryByName(const std::string_view keyName) {
            for (const auto& keyEntry : kKeyNames) {
                if (keyName == keyEntry.name) {
                    return &keyEntry;
                }
            }
            return nullptr;
        }

        int mouseBindingCodeFromButton(const int button) {
            if (button < 0 || button >= kMouseBindingMaxButtons) {
                return GLFW_KEY_UNKNOWN;
            }
            return kMouseBindingBase - button;
        }

        int mouseButtonFromBindingCodeInternal(const int bindingCode) {
            if (bindingCode > kMouseBindingBase) {
                return -1;
            }
            const int button = kMouseBindingBase - bindingCode;
            if (button < 0 || button >= kMouseBindingMaxButtons) {
                return -1;
            }
            return button;
        }

        bool isMouseBindingCodeInternal(const int bindingCode) {
            return mouseButtonFromBindingCodeInternal(bindingCode) >= 0;
        }

        const MouseBindingEntry* findMouseBindingEntryByCode(const int bindingCode) {
            const int button = mouseButtonFromBindingCodeInternal(bindingCode);
            if (button < 0) {
                return nullptr;
            }
            for (const auto& mouseEntry : kMouseBindingNames) {
                if (!mouseEntry.canonical) {
                    continue;
                }
                if (mouseEntry.button == button) {
                    return &mouseEntry;
                }
            }
            return nullptr;
        }

        const MouseBindingEntry* findMouseBindingEntryByName(const std::string_view bindingName) {
            for (const auto& mouseEntry : kMouseBindingNames) {
                if (bindingName == mouseEntry.name) {
                    return &mouseEntry;
                }
            }
            return nullptr;
        }

        bool tryCodeForKeyName(const std::string& keyName, int& outKey) {
            const std::string needle = toUpperCopy(keyName);
            if (const KeyNameEntry* keyEntry = findKeyEntryByName(needle); keyEntry != nullptr) {
                outKey = keyEntry->code;
                return true;
            }
            if (const MouseBindingEntry* mouseEntry = findMouseBindingEntryByName(needle); mouseEntry != nullptr) {
                outKey = mouseBindingCodeFromButton(mouseEntry->button);
                return true;
            }
            return false;
        }

        bool isBindableKeyCode(const int key) {
            if (isMouseBindingCodeInternal(key)) {
                return findMouseBindingEntryByCode(key) != nullptr;
            }
            if (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_BACKSPACE || key == GLFW_KEY_UNKNOWN) {
                return false;
            }
            return findKeyEntryByCode(key) != nullptr;
        }

        int firstAvailableBindableKey(const std::unordered_set<int>& usedKeys) {
            for (const auto& keyEntry : kKeyNames) {
                if (!keyEntry.canonical) {
                    continue;
                }
                if (usedKeys.contains(keyEntry.code)) {
                    continue;
                }
                if (isBindableKeyCode(keyEntry.code)) {
                    return keyEntry.code;
                }
            }
            return GLFW_KEY_UNKNOWN;
        }

        void enforceUniqueBindings(ControlBindings& controls) {
            std::unordered_set<int> usedKeys;
            usedKeys.reserve(kRebindActions.size());

            for (const auto& action : kRebindActions) {
                int& key = controls.*(action.member);
                if (!isBindableKeyCode(key) || usedKeys.contains(key)) {
                    ControlBindings defaults{};
                    key = defaults.*(action.member);
                }

                if (!isBindableKeyCode(key) || usedKeys.contains(key)) {
                    if (const int fallback = firstAvailableBindableKey(usedKeys); fallback != GLFW_KEY_UNKNOWN) {
                        key = fallback;
                    }
                }

                if (isBindableKeyCode(key)) {
                    usedKeys.insert(key);
                }
            }
        }
    } // namespace

    const std::array<RebindAction, 11>& rebindActions() {
        return kRebindActions;
    }

    void keyCaptureCallback(GLFWwindow*, const int key, const int scancode, const int action, const int mods) {
        (void)scancode;
        (void)mods;
        if (action == GLFW_PRESS && key != GLFW_KEY_UNKNOWN) {
            g_lastPressedKey = key;
        }
    }

    int consumeLastPressedKey() {
        const int key = g_lastPressedKey;
        g_lastPressedKey = GLFW_KEY_UNKNOWN;
        return key;
    }

    std::string keyNameForCode(const int key) {
        if (const MouseBindingEntry* mouseEntry = findMouseBindingEntryByCode(key); mouseEntry != nullptr) {
            return mouseEntry->name;
        }
        if (const KeyNameEntry* keyEntry = findKeyEntryByCode(key); keyEntry != nullptr) {
            return keyEntry->name;
        }
        return "UNKNOWN";
    }

    bool isBindableKey(const int key) {
        return isBindableKeyCode(key);
    }

    bool isMouseBindingCode(const int bindingCode) {
        return isMouseBindingCodeInternal(bindingCode);
    }

    int bindingCodeFromMouseButton(const int button) {
        return mouseBindingCodeFromButton(button);
    }

    int mouseButtonFromBindingCode(const int bindingCode) {
        return mouseButtonFromBindingCodeInternal(bindingCode);
    }

    bool isBindingPressed(GLFWwindow* window, const int bindingCode) {
        if (window == nullptr || !isBindableKeyCode(bindingCode)) {
            return false;
        }
        if (isMouseBindingCodeInternal(bindingCode)) {
            const int button = mouseButtonFromBindingCodeInternal(bindingCode);
            return button >= 0 && glfwGetMouseButton(window, button) == GLFW_PRESS;
        }
        return glfwGetKey(window, bindingCode) == GLFW_PRESS;
    }

    void saveControlBindings(const ControlBindings& controls, const std::string& path) {
        std::ofstream out(path, std::ios::trunc);
        if (!out) {
            return;
        }

        for (const auto& action : kRebindActions) {
            const int key = controls.*(action.member);
            out << action.configKey << "=" << keyNameForCode(key) << "\n";
        }
    }

    void loadControlBindings(ControlBindings& controls, const std::string& path) {
        std::ifstream in(path);
        if (!in) {
            saveControlBindings(controls, path);
            return;
        }

        std::unordered_map<std::string, int*> keyTargets;
        keyTargets.reserve(kRebindActions.size());
        for (const auto& action : kRebindActions) {
            keyTargets.emplace(action.configKey, &(controls.*(action.member)));
        }

        std::string line;
        while (std::getline(in, line)) {
            const auto eq = line.find('=');
            if (eq == std::string::npos) {
                continue;
            }

            const std::string rawKey = trimCopy(line.substr(0, eq));
            const std::string rawValue = trimCopy(line.substr(eq + 1));
            if (rawKey.empty() || rawValue.empty()) {
                continue;
            }

            const std::string cfgKey = toUpperCopy(rawKey);
            auto keyIt = keyTargets.find(cfgKey);
            if (keyIt == keyTargets.end()) {
                continue;
            }

            int parsedKey = GLFW_KEY_UNKNOWN;
            if (!tryCodeForKeyName(rawValue, parsedKey)) {
                continue;
            }
            if (!isBindableKey(parsedKey)) {
                continue;
            }
            *keyIt->second = parsedKey;
        }

        enforceUniqueBindings(controls);
        saveControlBindings(controls, path);
    }
} // namespace input
