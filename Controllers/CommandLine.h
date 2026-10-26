#pragma once

#include <optional>

#include <QStringList>

// Headless diagnostic modes (--hid-list, --probe, --dump, --stream-test,
// --matrix-test). Each prints one JSON document to stdout.
class CommandLine {
    public:
        // Exit code when an argument matched. nullopt means start the GUI.
        static std::optional<int> run(const QStringList& args);
};
