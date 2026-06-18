#pragma once

#include "SyncEngine.h"
#include "SyncOptions.h"
#include "SyncPlan.h"
#include <filesystem>

namespace ChronoSync {

    void ExecuteSyncPlan(const std::filesystem::path& srcRoot,
                         const std::filesystem::path& destRoot,
                         const std::wstring& destination,
                         const SyncOptions& options,
                         const SyncPlan& plan,
                         const SyncCallbacks& callbacks,
                         SyncStats& stats);

} // namespace ChronoSync
