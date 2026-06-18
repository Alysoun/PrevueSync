#pragma once

#include "GuiCommon.h"
#include "SyncOptions.h"
#include <string>
#include <vector>

void SetControlsState(BOOL enabled);

void PreviewThreadProc(std::wstring src, std::wstring dest, ChronoSync::SyncOptions options);
void AnalyzeThreadProc(std::wstring src, std::wstring dest, ChronoSync::SyncOptions options);
void SyncThreadProc(std::wstring src, std::wstring dest, ChronoSync::SyncOptions options);
void QueueThreadProc(std::vector<ChronoSync::SyncJob> jobs);
void UndoThreadProc(std::wstring dest);
