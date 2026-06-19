#include "SyncGuiWorker.h"
#include "SyncPlanAnalysis.h"

#include <algorithm>
#include <exception>

namespace {

constexpr size_t kVerboseCopyLogLimit = 200;
constexpr size_t kCopyMilestoneInterval = 500;
constexpr int kScanSourceEndPct = 6;
constexpr int kScanDestEndPct = 12;
constexpr int kCompareStartPct = 12;
constexpr int kCompareSpanPct = 88;
constexpr DWORD kStatusUiMinIntervalMs = 250;
constexpr size_t kStatusFileStride = 25;

static size_t s_compareFileIndex = 0;
static size_t s_compareFileTotal = 0;
static size_t s_copyFileIndex = 0;
static size_t s_copyFileTotal = 0;
static int s_scanPass = 0;
static size_t s_sourceScanTotal = 0;
static int s_lastReportedProgressPct = -1;
static size_t s_lastScanStatusCount = 0;
static ULONGLONG s_lastStatusUiTick = 0;
static size_t s_lastStatusFileIndex = 0;
static std::wstring s_currentComparePath;
static std::wstring s_currentCopyPath;
static ULONGLONG s_comparePhaseStartTick = 0;
static ULONGLONG s_copyPhaseStartTick = 0;
static ULONGLONG s_scanPassStartTick = 0;
static ULONGLONG s_lastEtaUiTick = 0;
static std::wstring s_lastEtaDisplay;

static void ResetOperationProgressState() {
    s_compareFileIndex = 0;
    s_compareFileTotal = 0;
    s_copyFileIndex = 0;
    s_copyFileTotal = 0;
    s_scanPass = 0;
    s_sourceScanTotal = 0;
    s_lastReportedProgressPct = -1;
    s_lastScanStatusCount = 0;
    s_lastStatusUiTick = 0;
    s_lastStatusFileIndex = 0;
    s_currentComparePath.clear();
    s_currentCopyPath.clear();
    s_comparePhaseStartTick = 0;
    s_copyPhaseStartTick = 0;
    s_scanPassStartTick = 0;
    s_lastEtaUiTick = 0;
    s_lastEtaDisplay.clear();
}

static void SetEtaDisplay(const std::wstring& duration) {
    const std::wstring display = duration.empty() ? L"ETA \u2014" : L"ETA " + duration;
    const ULONGLONG now = GetTickCount64();
    if (display == s_lastEtaDisplay) {
        return;
    }
    if (!duration.empty() && !s_lastEtaDisplay.empty() && s_lastEtaDisplay != L"ETA \u2014" &&
        now - s_lastEtaUiTick < 2000) {
        return;
    }
    s_lastEtaUiTick = now;
    s_lastEtaDisplay = display;
    g_MsgRegistry.SetEta(display);
}

static std::wstring FormatDurationMs(ULONGLONG ms) {
    if (ms < 1000) {
        return L"<1s";
    }
    ULONGLONG sec = ms / 1000;
    if (sec < 60) {
        return std::to_wstring(sec) + L"s";
    }
    ULONGLONG min = sec / 60;
    sec %= 60;
    if (min < 60) {
        if (sec == 0) {
            return std::to_wstring(min) + L"m";
        }
        return std::to_wstring(min) + L"m " + std::to_wstring(sec) + L"s";
    }
    const ULONGLONG hr = min / 60;
    min %= 60;
    return std::to_wstring(hr) + L"h " + std::to_wstring(min) + L"m";
}

static std::wstring FormatCompareEta(size_t fileIndex, size_t totalFiles, double fileFractionDone) {
    if (fileIndex == 0 || totalFiles == 0 || fileIndex > totalFiles || s_comparePhaseStartTick == 0) {
        return L"";
    }
    if (fileIndex < 2) {
        return L"";
    }

    const ULONGLONG elapsed = GetTickCount64() - s_comparePhaseStartTick;
    if (elapsed < 3000) {
        return L"";
    }

    const double unitsDone = static_cast<double>(fileIndex - 1) + fileFractionDone;
    if (unitsDone <= 0.0) {
        return L"";
    }

    const double msPerUnit = static_cast<double>(elapsed) / unitsDone;
    const double unitsLeft = static_cast<double>(totalFiles) - unitsDone;
    if (unitsLeft <= 0.0) {
        return L"";
    }

    return FormatDurationMs(static_cast<ULONGLONG>(msPerUnit * unitsLeft));
}

static std::wstring FormatCopyEta(size_t fileIndex, size_t totalFiles, double fileFractionDone) {
    if (fileIndex == 0 || totalFiles == 0 || fileIndex > totalFiles || s_copyPhaseStartTick == 0) {
        return L"";
    }
    if (fileIndex < 2) {
        return L"";
    }

    const ULONGLONG elapsed = GetTickCount64() - s_copyPhaseStartTick;
    if (elapsed < 3000) {
        return L"";
    }

    const double unitsDone = static_cast<double>(fileIndex - 1) + fileFractionDone;
    if (unitsDone <= 0.0) {
        return L"";
    }

    const double msPerUnit = static_cast<double>(elapsed) / unitsDone;
    const double unitsLeft = static_cast<double>(totalFiles) - unitsDone;
    if (unitsLeft <= 0.0) {
        return L"";
    }

    return FormatDurationMs(static_cast<ULONGLONG>(msPerUnit * unitsLeft));
}

static std::wstring FormatScanEta(size_t itemsFound) {
    if (itemsFound < 2000 || s_scanPassStartTick == 0) {
        return L"";
    }

    const ULONGLONG elapsed = GetTickCount64() - s_scanPassStartTick;
    if (elapsed < 2000) {
        return L"";
    }

    const double rate = static_cast<double>(itemsFound) / static_cast<double>(elapsed);
    if (rate <= 0.0) {
        return L"";
    }

    size_t estimatedTotal = itemsFound;
    if (s_scanPass > 0 && s_sourceScanTotal > itemsFound) {
        estimatedTotal = s_sourceScanTotal;
    } else {
        estimatedTotal = static_cast<size_t>(static_cast<double>(itemsFound) * 1.15);
        if (estimatedTotal <= itemsFound) {
            estimatedTotal = itemsFound + 5000;
        }
    }

    if (itemsFound >= estimatedTotal) {
        return L"";
    }

    const ULONGLONG remainingMs =
        static_cast<ULONGLONG>(static_cast<double>(estimatedTotal - itemsFound) / rate);
    return FormatDurationMs(remainingMs);
}

static bool ShouldRefreshStatus(size_t fileIndex, bool force) {
    if (force) {
        return true;
    }
    const ULONGLONG now = GetTickCount64();
    if (fileIndex != s_lastStatusFileIndex && fileIndex % kStatusFileStride == 0) {
        return true;
    }
    return now - s_lastStatusUiTick >= kStatusUiMinIntervalMs;
}

static void UpdateCompareStatus(int filePct, bool force) {
    if (s_compareFileTotal == 0) {
        return;
    }
    if (!ShouldRefreshStatus(s_compareFileIndex, force)) {
        return;
    }

    s_lastStatusUiTick = GetTickCount64();
    s_lastStatusFileIndex = s_compareFileIndex;

    std::wstring status = L"SHA256 [" + std::to_wstring(s_compareFileIndex) + L"/" +
                          std::to_wstring(s_compareFileTotal) + L"]: ";
    if (!s_currentComparePath.empty()) {
        status += TruncateForStatus(s_currentComparePath, 90);
    } else {
        status += L"...";
    }
    if (filePct >= 0 && filePct > 0) {
        status += L" \u2014 " + std::to_wstring(filePct) + L"%";
    }

    g_MsgRegistry.SetStatus(status);

    const double fileFraction = filePct >= 0 ? static_cast<double>(filePct) / 100.0 : 0.0;
    SetEtaDisplay(FormatCompareEta(s_compareFileIndex, s_compareFileTotal, fileFraction));
}

static void ReportProgressIfChanged(int pct) {
    if (pct != s_lastReportedProgressPct) {
        s_lastReportedProgressPct = pct;
        g_MsgRegistry.SetProgress(pct);
    }
}

static int OverallPhaseProgressPct(size_t phaseIndex, size_t phaseTotal, unsigned long long phaseBytes,
                                   unsigned long long phaseSize) {
    if (phaseTotal == 0) {
        return 0;
    }
    unsigned long long numer = (phaseIndex > 0 ? phaseIndex - 1 : 0) * 100ULL;
    if (phaseSize > 0) {
        numer += std::min(phaseBytes * 100ULL / phaseSize, 100ULL);
    }
    return static_cast<int>(std::min(numer / phaseTotal, 100ULL));
}

static int MapCompareProgress(int innerPct) {
    return kCompareStartPct + (innerPct * kCompareSpanPct / 100);
}

static int ScanProgressPct(size_t itemsFound) {
    if (s_scanPass == 0) {
        const double denom = static_cast<double>(itemsFound) + 50000.0;
        return static_cast<int>(static_cast<double>(kScanSourceEndPct) * itemsFound / denom);
    }

    const size_t denom = s_sourceScanTotal > 0 ? s_sourceScanTotal : std::max(itemsFound, size_t(1));
    int pct = kScanSourceEndPct +
              static_cast<int>(static_cast<double>(kScanDestEndPct - kScanSourceEndPct) * itemsFound /
                               static_cast<double>(denom));
    return std::min(pct, kScanDestEndPct - 1);
}

static void AttachHashProgress(PrevueSync::SyncCallbacks& callbacks) {
    callbacks.onCompareFileBegin = [](size_t fileIndex, size_t totalFiles, const std::wstring& relPath) {
        s_compareFileIndex = fileIndex;
        s_compareFileTotal = totalFiles;
        s_currentComparePath = relPath;
        if (fileIndex == 1) {
            s_comparePhaseStartTick = GetTickCount64();
        }

        const int innerPct =
            totalFiles > 0 ? static_cast<int>((fileIndex - 1) * 100 / totalFiles) : 0;
        ReportProgressIfChanged(MapCompareProgress(innerPct));

        const bool forceStatus = (fileIndex == 1 || fileIndex == totalFiles || fileIndex % kStatusFileStride == 0);
        UpdateCompareStatus(-1, forceStatus);
    };

    callbacks.onHashProgress = [](const std::wstring& relPath, unsigned long long bytesHashed,
                                  unsigned long long fileSize, bool hashingSource) {
        (void)hashingSource;
        if (!relPath.empty()) {
            s_currentComparePath = relPath;
        }

        int filePct = fileSize > 0 ? static_cast<int>((bytesHashed * 100) / fileSize) : 0;
        if (filePct > 100) {
            filePct = 100;
        }

        int innerPct = 0;
        if (s_compareFileTotal > 0) {
            innerPct = OverallPhaseProgressPct(s_compareFileIndex, s_compareFileTotal, bytesHashed, fileSize);
        } else if (fileSize > 0) {
            innerPct = filePct;
        }

        ReportProgressIfChanged(MapCompareProgress(innerPct));
        UpdateCompareStatus(filePct, false);
    };
}

static void AttachPlanCallbacks(PrevueSync::SyncCallbacks& callbacks, PrevueSync::SyncOptions options,
                                const wchar_t* compareStatus, const wchar_t* defaultCompareStatus) {
    callbacks.onScanStart = [](const std::wstring& rootDir) {
        s_scanPassStartTick = GetTickCount64();
        const wchar_t* label = s_scanPass == 0 ? L"Scanning source" : L"Scanning destination";
        if (s_scanPass == 0) {
            ReportProgressIfChanged(0);
        } else {
            ReportProgressIfChanged(kScanSourceEndPct);
        }
        SetEtaDisplay(L"");
        g_MsgRegistry.SetStatus(std::wstring(label) + L": " + TruncateForStatus(rootDir, 120));
    };
    callbacks.onScanProgress = [](size_t itemsFound, const std::wstring& currentPath) {
        (void)currentPath;
        ReportProgressIfChanged(ScanProgressPct(itemsFound));
        if (itemsFound >= s_lastScanStatusCount + 5000 || itemsFound < s_lastScanStatusCount) {
            s_lastScanStatusCount = itemsFound;
            const wchar_t* label = s_scanPass == 0 ? L"Scanning source" : L"Scanning destination";
            g_MsgRegistry.SetStatus(std::wstring(label) + L": " + std::to_wstring(itemsFound) + L" items...");
            SetEtaDisplay(FormatScanEta(itemsFound));
        }
    };
    callbacks.onScanComplete = [](size_t totalItems) {
        if (s_scanPass == 0) {
            s_sourceScanTotal = totalItems;
            ReportProgressIfChanged(kScanSourceEndPct);
        } else {
            ReportProgressIfChanged(kScanDestEndPct);
        }
        s_scanPass++;
        g_MsgRegistry.PushLog(L"Scan complete. Found " + std::to_wstring(totalItems) + L" items.");
    };
    callbacks.onCompareStart = [options, compareStatus, defaultCompareStatus]() {
        s_compareFileIndex = 0;
        s_compareFileTotal = 0;
        s_currentComparePath.clear();
        s_comparePhaseStartTick = 0;
        ReportProgressIfChanged(kCompareStartPct);
        SetEtaDisplay(L"");
        if (options.compareMode == PrevueSync::CompareMode::Sha256) {
            g_MsgRegistry.SetStatus(compareStatus);
        } else {
            g_MsgRegistry.SetStatus(defaultCompareStatus);
        }
    };
    callbacks.onCompareComplete = [](size_t dirsToCreate, size_t filesToCopy, size_t itemsToDelete) {
        ReportProgressIfChanged(100);
        s_compareFileIndex = 0;
        s_compareFileTotal = 0;
        std::wstring plan = L"Plan: Create " + std::to_wstring(dirsToCreate) + L" dirs, Copy " +
                            std::to_wstring(filesToCopy) + L" files, Remove/Delete " +
                            std::to_wstring(itemsToDelete) + L" items.";
        g_MsgRegistry.PushLog(plan);
    };
    callbacks.onLog = [](const std::wstring& message, bool isError) {
        std::wstring prefix = isError ? L"[ERROR] " : L"[INFO] ";
        g_MsgRegistry.PushLog(prefix + message);
    };
}

static void AttachCopyCallbacks(PrevueSync::SyncCallbacks& callbacks) {
    callbacks.onCopyStart = [](const std::wstring& relPath, unsigned long long fileSizeBytes,
                               size_t fileIndex, size_t totalFiles) {
        (void)fileSizeBytes;
        s_copyFileIndex = fileIndex;
        s_copyFileTotal = totalFiles;
        s_currentCopyPath = relPath;
        s_lastReportedProgressPct = -1;
        if (fileIndex == 1) {
            s_copyPhaseStartTick = GetTickCount64();
        }
        int base = totalFiles > 0 ? static_cast<int>((fileIndex - 1) * 100 / totalFiles) : 0;
        ReportProgressIfChanged(base);

        std::wstring status = L"[" + std::to_wstring(fileIndex) + L"/" + std::to_wstring(totalFiles) +
                              L"] Syncing: " + TruncateForStatus(relPath, 55);
        g_MsgRegistry.SetStatus(status);
        SetEtaDisplay(FormatCopyEta(fileIndex, totalFiles, 0.0));
        if (totalFiles <= kVerboseCopyLogLimit) {
            g_MsgRegistry.PushLog(L"Syncing: " + relPath + L" (" + std::to_wstring(fileSizeBytes / 1024) + L" KB)");
        } else if (fileIndex == 1 || fileIndex % kCopyMilestoneInterval == 0 || fileIndex == totalFiles) {
            g_MsgRegistry.PushLog(L"Syncing [" + std::to_wstring(fileIndex) + L"/" + std::to_wstring(totalFiles) +
                                  L"]: " + TruncateForStatus(relPath));
        }
    };

    callbacks.onCopyProgress = [](unsigned long long bytesCopied, unsigned long long fileSizeBytes) {
        if (fileSizeBytes > 0 && s_copyFileTotal > 0) {
            const int overall = OverallPhaseProgressPct(s_copyFileIndex, s_copyFileTotal, bytesCopied, fileSizeBytes);
            ReportProgressIfChanged(overall);

            if (ShouldRefreshStatus(s_copyFileIndex, false)) {
                s_lastStatusUiTick = GetTickCount64();
                s_lastStatusFileIndex = s_copyFileIndex;
                const int filePct = static_cast<int>(bytesCopied * 100 / fileSizeBytes);
                std::wstring status = L"[" + std::to_wstring(s_copyFileIndex) + L"/" +
                                      std::to_wstring(s_copyFileTotal) + L"] Syncing: " +
                                      TruncateForStatus(s_currentCopyPath, 55);
                if (filePct > 0) {
                    status += L" \u2014 " + std::to_wstring(filePct) + L"%";
                }
                g_MsgRegistry.SetStatus(status);
                SetEtaDisplay(FormatCopyEta(s_copyFileIndex, s_copyFileTotal, static_cast<double>(filePct) / 100.0));
            }
        }
    };

    callbacks.onCopyComplete = [](const std::wstring& relPath, bool success, const std::wstring& errorMessage) {
        if (!success) {
            g_MsgRegistry.PushLog(L"  \u2718 Failed: " + relPath + L" - " + errorMessage);
        }
        if (s_copyFileTotal > 0) {
            ReportProgressIfChanged(static_cast<int>(s_copyFileIndex * 100 / s_copyFileTotal));
        }
    };

    callbacks.onDeleteItem = [](const std::wstring& relPath, bool isDirectory) {
        std::wstring typeStr = isDirectory ? L"directory" : L"file";
        g_MsgRegistry.PushLog(L"[PRUNE] Archiving " + typeStr + L": " + TruncateForStatus(relPath, 200));
    };

    callbacks.onDeleteFailed = [](const std::wstring& relPath, const std::wstring& errorMessage) {
        g_MsgRegistry.PushLog(L"[PRUNE] Archive failed: " + TruncateForStatus(relPath, 200) +
                              L" (" + errorMessage + L")");
    };
}

} // namespace

void SetControlsState(BOOL enabled) {
    EnableWindow(g_hWndSrcBrowse, enabled);
    EnableWindow(g_hWndDestBrowse, enabled);
    EnableWindow(g_hWndPruneCheck, enabled);
    EnableWindow(GetDlgItem(g_hWndMain, ID_PRUNE_LABEL), enabled);
    EnableWindow(g_hWndExcludeEdit, enabled);
    EnableWindow(g_hWndIncludeEdit, enabled);
    EnableWindow(g_hWndSaveProfileBtn, enabled);
    EnableWindow(g_hWndLoadProfileBtn, enabled);
    EnableWindow(g_hWndSha256Check, enabled);
    EnableWindow(g_hWndVerifyCheck, enabled);
    EnableWindow(g_hWndVersionedBackupCheck, enabled);
    EnableWindow(g_hWndDeltaCopyCheck, enabled);
    EnableWindow(g_hWndScheduleBtn, enabled);
    EnableWindow(g_hWndHistoryBtn, enabled);
    EnableWindow(g_hWndAddQueueBtn, enabled);
    EnableWindow(g_hWndRunQueueBtn, enabled);
    EnableWindow(g_hWndClearQueueBtn, enabled);
    EnableWindow(g_hWndSaveQueueBtn, enabled);
    EnableWindow(g_hWndLoadQueueBtn, enabled);
    EnableWindow(g_hWndPreviewBtn, enabled);
    EnableWindow(g_hWndAnalyzeBtn, enabled);
    EnableWindow(g_hWndSyncBtn, enabled);
    if (!enabled) {
        EnableWindow(g_hWndUndoBtn, FALSE);
    }
}

void PreviewThreadProc(std::wstring src, std::wstring dest, PrevueSync::SyncOptions options) {
    ResetOperationProgressState();
    PrevueSync::SyncCallbacks callbacks;
    AttachHashProgress(callbacks);
    AttachPlanCallbacks(callbacks, options,
                        L"SHA256 compare: hashing files (updates every 8 MB per file)...",
                        L"Comparing structures for preview...");

    try {
        auto report = PrevueSync::BuildSyncPlanReport(src, dest, options, callbacks);

        auto* launchBundle = new PreviewLaunchData();
        launchBundle->pList = new std::vector<PrevueSync::PreviewItem>(std::move(report.previewItems));
        launchBundle->analysis = std::move(report.analysis);
        launchBundle->hasAnalysis = true;
        launchBundle->sourceRoot = src;
        launchBundle->destRoot = dest;
        PostMessageW(g_hWndMain, WM_SYNC_PREVIEW_COMPLETE, 0, (LPARAM)launchBundle);
    } catch (const std::exception& ex) {
        std::string err = ex.what() ? ex.what() : "unknown error";
        g_MsgRegistry.PushLog(L"[ERROR] Preview failed: " + std::wstring(err.begin(), err.end()));
        PostMessageW(g_hWndMain, WM_SYNC_PREVIEW_COMPLETE, 1, 0);
    } catch (...) {
        g_MsgRegistry.PushLog(L"[ERROR] Preview failed with an unexpected error.");
        PostMessageW(g_hWndMain, WM_SYNC_PREVIEW_COMPLETE, 1, 0);
    }
}

void AnalyzeThreadProc(std::wstring src, std::wstring dest, PrevueSync::SyncOptions options) {
    ResetOperationProgressState();
    PrevueSync::SyncCallbacks callbacks;
    AttachHashProgress(callbacks);
    AttachPlanCallbacks(callbacks, options,
                        L"SHA256 compare: hashing files (updates every 8 MB per file)...",
                        L"Analyzing planned sync impact...");

    try {
        auto report = PrevueSync::BuildSyncPlanReport(src, dest, options, callbacks);
        auto* data = new AnalyzeCompleteData();
        data->analysis = std::move(report.analysis);
        data->hasAnalysis = true;
        data->report = PrevueSync::FormatSyncPlanReport(data->analysis, src, dest);
        PostMessageW(g_hWndMain, WM_SYNC_ANALYZE_COMPLETE, 0, (LPARAM)data);
    } catch (const std::exception& ex) {
        std::string err = ex.what() ? ex.what() : "unknown error";
        g_MsgRegistry.PushLog(L"[ERROR] Analyze failed: " + std::wstring(err.begin(), err.end()));
        PostMessageW(g_hWndMain, WM_SYNC_ANALYZE_COMPLETE, 1, 0);
    } catch (...) {
        g_MsgRegistry.PushLog(L"[ERROR] Analyze failed with an unexpected error.");
        PostMessageW(g_hWndMain, WM_SYNC_ANALYZE_COMPLETE, 1, 0);
    }
}

void SyncThreadProc(std::wstring src, std::wstring dest, PrevueSync::SyncOptions options) {
    ResetOperationProgressState();
    PrevueSync::SyncCallbacks callbacks;
    AttachHashProgress(callbacks);
    AttachPlanCallbacks(callbacks, options,
                        L"SHA256 compare: hashing files (updates every 8 MB per file)...",
                        L"Comparing directory structures...");
    AttachCopyCallbacks(callbacks);

    callbacks.onScanDir = [](const std::wstring& subDir) {
        (void)subDir;
    };

    auto* pStats = new PrevueSync::SyncStats();
    try {
        *pStats = PrevueSync::SyncEngine::Sync(src, dest, options, callbacks);
    } catch (const std::exception& ex) {
        std::string err = ex.what() ? ex.what() : "unknown error";
        g_MsgRegistry.PushLog(L"[ERROR] Sync failed: " + std::wstring(err.begin(), err.end()));
    } catch (...) {
        g_MsgRegistry.PushLog(L"[ERROR] Sync failed with an unexpected error.");
    }

    PostMessageW(g_hWndMain, WM_SYNC_COMPLETE, 1, (LPARAM)pStats);
}

void QueueThreadProc(std::vector<PrevueSync::SyncJob> jobs) {
    ResetOperationProgressState();
    PrevueSync::SyncCallbacks callbacks;
    AttachHashProgress(callbacks);
    AttachPlanCallbacks(callbacks, PrevueSync::SyncOptions{},
                        L"SHA256 compare: hashing files (updates every 8 MB per file)...",
                        L"Comparing directory structures...");
    AttachCopyCallbacks(callbacks);

    size_t completedJobs = 0;
    for (size_t i = 0; i < jobs.size(); ++i) {
        g_MsgRegistry.PushLog(L"=== Queue job " + std::to_wstring(i + 1) + L"/" + std::to_wstring(jobs.size()) +
                              L": " + jobs[i].name + L" ===");
        PrevueSync::SyncStats stats = PrevueSync::SyncEngine::Sync(jobs[i].source, jobs[i].destination, jobs[i].options, callbacks);
        if (stats.filesCopied > 0 || stats.itemsDeleted > 0 || stats.dirsCreated > 0) {
            completedJobs++;
        }
    }

    size_t* pCompleted = new size_t(completedJobs);
    PostMessageW(g_hWndMain, WM_SYNC_QUEUE_COMPLETE, static_cast<WPARAM>(jobs.size()), (LPARAM)pCompleted);
}

void UndoThreadProc(std::wstring dest) {
    PrevueSync::SyncCallbacks callbacks;
    callbacks.onLog = [](const std::wstring& message, bool isError) {
        std::wstring prefix = isError ? L"[ERROR] " : L"[INFO] ";
        g_MsgRegistry.PushLog(prefix + message);
    };
    callbacks.onScanStart = [](const std::wstring& rootDir) {
        g_MsgRegistry.SetStatus(L"Scanning trash: " + TruncateForStatus(rootDir, 200));
    };

    PrevueSync::SyncEngine::UndoPruning(dest, callbacks);
    PostMessageW(g_hWndMain, WM_SYNC_UNDO_COMPLETE, 0, 0);
}
