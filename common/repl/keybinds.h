#pragma once

#include <string>
#include <vector>

class KeyBind {
public:
    enum class Modifier { CTRL, SHIFT, META };

    Modifier modifier;
    std::string key;
    std::string description;
    std::string command;
    KeyBind() {}

    KeyBind(Modifier mod, const std::string& k, const std::string& desc, const std::string& cmd)
        : modifier(mod), key(k), description(desc), command(cmd) {
    }

    std::string toString() const {
        switch (modifier) {
        case Modifier::CTRL: return "Ctrl-" + key;
        case Modifier::SHIFT: return "Shift-" + key;
        case Modifier::META: return "Meta-" + key;
        default: return key;
        }
    }

    bool operator==(const KeyBind& other) const {
        return modifier == other.modifier && key == other.key;
    }
};