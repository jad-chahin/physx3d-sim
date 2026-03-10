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
            {GLFW_KEY_TAB, "TAB", true},
            {GLFW_KEY_ENTER, "ENTER", true},
            {GLFW_KEY_APOSTROPHE, "APOSTROPHE", true},
            {GLFW_KEY_COMMA, "COMMA", true},
            {GLFW_KEY_MINUS, "MINUS", true},
            {GLFW_KEY_PERIOD, "PERIOD", true},
            {GLFW_KEY_SLASH, "SLASH", true},
            {GLFW_KEY_SEMICOLON, "SEMICOLON", true},
            {GLFW_KEY_EQUAL, "EQUAL", true},
            {GLFW_KEY_LEFT_BRACKET, "LEFT_BRACKET", true},
            {GLFW_KEY_BACKSLASH, "BACKSLASH", true},
            {GLFW_KEY_RIGHT_BRACKET, "RIGHT_BRACKET", true},
            {GLFW_KEY_GRAVE_ACCENT, "GRAVE_ACCENT", true},
            {GLFW_KEY_LEFT_CONTROL, "LCTRL", true},
            {GLFW_KEY_RIGHT_CONTROL, "RCTRL", true},
            {GLFW_KEY_LEFT_ALT, "LALT", true},
            {GLFW_KEY_RIGHT_ALT, "RALT", true},
            {GLFW_KEY_LEFT_SUPER, "LSUPER", true},
            {GLFW_KEY_RIGHT_SUPER, "RSUPER", true},
            {GLFW_KEY_LEFT_SHIFT, "LSHIFT", true},
            {GLFW_KEY_RIGHT_SHIFT, "RSHIFT", true},
            {GLFW_KEY_LEFT_CONTROL, "LEFT_CONTROL", false},
            {GLFW_KEY_RIGHT_CONTROL, "RIGHT_CONTROL", false},
            {GLFW_KEY_LEFT_ALT, "LEFT_ALT", false},
            {GLFW_KEY_RIGHT_ALT, "RIGHT_ALT", false},
            {GLFW_KEY_LEFT_SUPER, "LEFT_SUPER", false},
            {GLFW_KEY_RIGHT_SUPER, "RIGHT_SUPER", false},
            {GLFW_KEY_LEFT_SHIFT, "LEFT_SHIFT", false},
            {GLFW_KEY_RIGHT_SHIFT, "RIGHT_SHIFT", false},
            {GLFW_KEY_UP, "UP", true},
            {GLFW_KEY_DOWN, "DOWN", true},
            {GLFW_KEY_LEFT, "LEFT", true},
            {GLFW_KEY_RIGHT, "RIGHT", true},
            {GLFW_KEY_INSERT, "INSERT", true},
            {GLFW_KEY_DELETE, "DELETE", true},
            {GLFW_KEY_HOME, "HOME", true},
            {GLFW_KEY_END, "END", true},
            {GLFW_KEY_PAGE_UP, "PAGE_UP", true},
            {GLFW_KEY_PAGE_DOWN, "PAGE_DOWN", true},
            {GLFW_KEY_CAPS_LOCK, "CAPS_LOCK", true},
            {GLFW_KEY_PRINT_SCREEN, "PRINT_SCREEN", true},
            {GLFW_KEY_PAUSE, "PAUSE", true},
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

        bool isDigitKey(const int key) {
            return key >= GLFW_KEY_0 && key <= GLFW_KEY_9;
        }

        bool isLetterKey(const int key) {
            return key >= GLFW_KEY_A && key <= GLFW_KEY_Z;
        }

        bool isFunctionKey(const int key) {
            return key >= GLFW_KEY_F1 && key <= GLFW_KEY_F25;
        }

        bool isKeypadDigit(const int key) {
            return key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_9;
        }

        bool isNamedKeyCode(const int key) {
            for (const auto& keyEntry : kKeyNames) {
                if (keyEntry.code == key && keyEntry.canonical) {
                    return true;
                }
            }
            return false;
        }

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

        int bindingCodeFromMouseButtonInternal(const int button) {
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
            if (needle.size() == 1) {
                const char c = needle[0];
                if (c >= '0' && c <= '9') {
                    outKey = GLFW_KEY_0 + (c - '0');
                    return true;
                }
                if (c >= 'A' && c <= 'Z') {
                    outKey = GLFW_KEY_A + (c - 'A');
                    return true;
                }
            }
            if (needle.size() >= 2 && needle[0] == 'F') {
                const std::string suffix = needle.substr(1);
                bool allDigits = !suffix.empty();
                for (const char c : suffix) {
                    if (c < '0' || c > '9') {
                        allDigits = false;
                        break;
                    }
                }
                if (allDigits) {
                    const int number = std::stoi(suffix);
                    if (number >= 1 && number <= 25) {
                        outKey = GLFW_KEY_F1 + (number - 1);
                        return true;
                    }
                }
            }
            if (needle.rfind("KP_", 0) == 0) {
                const std::string suffix = needle.substr(3);
                if (suffix.size() == 1 && suffix[0] >= '0' && suffix[0] <= '9') {
                    outKey = GLFW_KEY_KP_0 + (suffix[0] - '0');
                    return true;
                }
                if (suffix == "DECIMAL") {
                    outKey = GLFW_KEY_KP_DECIMAL;
                    return true;
                }
                if (suffix == "DIVIDE") {
                    outKey = GLFW_KEY_KP_DIVIDE;
                    return true;
                }
                if (suffix == "MULTIPLY") {
                    outKey = GLFW_KEY_KP_MULTIPLY;
                    return true;
                }
                if (suffix == "SUBTRACT") {
                    outKey = GLFW_KEY_KP_SUBTRACT;
                    return true;
                }
                if (suffix == "ADD") {
                    outKey = GLFW_KEY_KP_ADD;
                    return true;
                }
                if (suffix == "ENTER") {
                    outKey = GLFW_KEY_KP_ENTER;
                    return true;
                }
                if (suffix == "EQUAL") {
                    outKey = GLFW_KEY_KP_EQUAL;
                    return true;
                }
            }
            if (const MouseBindingEntry* mouseEntry = findMouseBindingEntryByName(needle); mouseEntry != nullptr) {
                outKey = bindingCodeFromMouseButtonInternal(mouseEntry->button);
                return true;
            }
            return false;
        }

        bool isBindableKeyCode(const int key) {
            if (mouseButtonFromBindingCodeInternal(key) >= 0) {
                return findMouseBindingEntryByCode(key) != nullptr;
            }
            if (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_BACKSPACE || key == GLFW_KEY_UNKNOWN) {
                return false;
            }
            return isNamedKeyCode(key)
                || isDigitKey(key)
                || isLetterKey(key)
                || isFunctionKey(key)
                || isKeypadDigit(key)
                || key == GLFW_KEY_KP_DECIMAL
                || key == GLFW_KEY_KP_DIVIDE
                || key == GLFW_KEY_KP_MULTIPLY
                || key == GLFW_KEY_KP_SUBTRACT
                || key == GLFW_KEY_KP_ADD
                || key == GLFW_KEY_KP_ENTER
                || key == GLFW_KEY_KP_EQUAL;
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
            for (int key = GLFW_KEY_0; key <= GLFW_KEY_9; ++key) {
                if (!usedKeys.contains(key)) {
                    return key;
                }
            }
            for (int key = GLFW_KEY_A; key <= GLFW_KEY_Z; ++key) {
                if (!usedKeys.contains(key)) {
                    return key;
                }
            }
            for (int key = GLFW_KEY_F1; key <= GLFW_KEY_F25; ++key) {
                if (!usedKeys.contains(key)) {
                    return key;
                }
            }
            for (int key = GLFW_KEY_KP_0; key <= GLFW_KEY_KP_9; ++key) {
                if (!usedKeys.contains(key)) {
                    return key;
                }
            }
            for (const int key : {
                    GLFW_KEY_KP_DECIMAL,
                    GLFW_KEY_KP_DIVIDE,
                    GLFW_KEY_KP_MULTIPLY,
                    GLFW_KEY_KP_SUBTRACT,
                    GLFW_KEY_KP_ADD,
                    GLFW_KEY_KP_ENTER,
                    GLFW_KEY_KP_EQUAL})
            {
                if (!usedKeys.contains(key)) {
                    return key;
                }
            }
            return GLFW_KEY_UNKNOWN;
        }

        bool enforceUniqueBindings(ControlBindings& controls) {
            std::unordered_set<int> usedKeys;
            usedKeys.reserve(kRebindActions.size());
            bool changed = false;

            for (const auto& action : kRebindActions) {
                int& key = controls.*(action.member);
                const int originalKey = key;
                if (!isBindableKeyCode(key) || usedKeys.contains(key)) {
                    constexpr ControlBindings defaults{};
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

                if (key != originalKey) {
                    changed = true;
                }
            }

            return changed;
        }
    } // namespace

    const std::array<RebindAction, 11>& rebindActions() {
        return kRebindActions;
    }

    std::string keyNameForCode(const int key) {
        if (const MouseBindingEntry* mouseEntry = findMouseBindingEntryByCode(key); mouseEntry != nullptr) {
            return mouseEntry->name;
        }
        if (isDigitKey(key)) {
            return std::string(1, static_cast<char>('0' + (key - GLFW_KEY_0)));
        }
        if (isLetterKey(key)) {
            return std::string(1, static_cast<char>('A' + (key - GLFW_KEY_A)));
        }
        if (isFunctionKey(key)) {
            return "F" + std::to_string(key - GLFW_KEY_F1 + 1);
        }
        if (isKeypadDigit(key)) {
            return "KP_" + std::to_string(key - GLFW_KEY_KP_0);
        }
        switch (key) {
            case GLFW_KEY_KP_DECIMAL: return "KP_DECIMAL";
            case GLFW_KEY_KP_DIVIDE: return "KP_DIVIDE";
            case GLFW_KEY_KP_MULTIPLY: return "KP_MULTIPLY";
            case GLFW_KEY_KP_SUBTRACT: return "KP_SUBTRACT";
            case GLFW_KEY_KP_ADD: return "KP_ADD";
            case GLFW_KEY_KP_ENTER: return "KP_ENTER";
            case GLFW_KEY_KP_EQUAL: return "KP_EQUAL";
            default: break;
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
        return mouseButtonFromBindingCodeInternal(bindingCode) >= 0;
    }

    int bindingCodeFromMouseButton(const int button) {
        return bindingCodeFromMouseButtonInternal(button);
    }

    int mouseButtonFromBindingCode(const int bindingCode) {
        return mouseButtonFromBindingCodeInternal(bindingCode);
    }

    bool isBindingPressed(GLFWwindow* window, const int bindingCode) {
        if (window == nullptr || !isBindableKeyCode(bindingCode)) {
            return false;
        }
        if (mouseButtonFromBindingCodeInternal(bindingCode) >= 0) {
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
            if (*keyIt->second != parsedKey) {
                *keyIt->second = parsedKey;
            }
        }

        if (enforceUniqueBindings(controls)) {
            saveControlBindings(controls, path);
        }
    }
} // namespace input
