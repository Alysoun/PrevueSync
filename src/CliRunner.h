#pragma once

namespace ChronoSync {

    class CliRunner {
    public:
        // Returns true if CLI mode handled the process (set exitCode).
        static bool TryRun(int& exitCode);
    };

} // namespace ChronoSync
