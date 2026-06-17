#include "PathFilter.h"
#include <algorithm>
#include <cwctype>

namespace ChronoSync {

    static std::wstring Trim(const std::wstring& s) {
        size_t start = 0;
        while (start < s.size() && iswspace(s[start])) {
            ++start;
        }
        size_t end = s.size();
        while (end > start && iswspace(s[end - 1])) {
            --end;
        }
        return s.substr(start, end - start);
    }

    static std::wstring ToLower(const std::wstring& s) {
        std::wstring out = s;
        std::transform(out.begin(), out.end(), out.begin(), [](wchar_t c) {
            return static_cast<wchar_t>(towlower(c));
        });
        return out;
    }

    FilterOptions FilterOptions::Defaults() {
        FilterOptions opts;
        opts.excludePatterns = { L"*.pkl", L"node_modules", L"*.zip" };
        return opts;
    }

    std::vector<std::wstring> PathFilter::ParseSemicolonList(const std::wstring& list) {
        std::vector<std::wstring> patterns;
        size_t start = 0;
        while (start <= list.size()) {
            size_t sep = list.find(L';', start);
            if (sep == std::wstring::npos) {
                sep = list.size();
            }
            std::wstring token = Trim(list.substr(start, sep - start));
            if (!token.empty()) {
                patterns.push_back(token);
            }
            if (sep == list.size()) {
                break;
            }
            start = sep + 1;
        }
        return patterns;
    }

    FilterOptions FilterOptions::FromSemicolonList(const std::wstring& includeList, const std::wstring& excludeList) {
        FilterOptions opts;
        opts.includePatterns = PathFilter::ParseSemicolonList(includeList);
        opts.excludePatterns = PathFilter::ParseSemicolonList(excludeList);
        return opts;
    }

    std::wstring FilterOptions::IncludeToSemicolonList() const {
        std::wstring result;
        for (size_t i = 0; i < includePatterns.size(); ++i) {
            if (i > 0) {
                result += L"; ";
            }
            result += includePatterns[i];
        }
        return result;
    }

    std::wstring FilterOptions::ExcludeToSemicolonList() const {
        std::wstring result;
        for (size_t i = 0; i < excludePatterns.size(); ++i) {
            if (i > 0) {
                result += L"; ";
            }
            result += excludePatterns[i];
        }
        return result;
    }

    bool PathFilter::GlobMatch(const std::wstring& pattern, const std::wstring& text) {
        const std::wstring pat = ToLower(pattern);
        const std::wstring txt = ToLower(text);

        size_t p = 0;
        size_t t = 0;
        size_t starPat = std::wstring::npos;
        size_t starTxt = 0;

        while (t < txt.size()) {
            if (p < pat.size() && (pat[p] == L'*' || pat[p] == txt[t])) {
                if (pat[p] == L'*') {
                    starPat = p++;
                    starTxt = t;
                } else {
                    ++p;
                    ++t;
                }
            } else if (starPat != std::wstring::npos) {
                p = starPat + 1;
                t = ++starTxt;
            } else {
                return false;
            }
        }

        while (p < pat.size() && pat[p] == L'*') {
            ++p;
        }
        return p == pat.size();
    }

    bool PathFilter::MatchesPattern(const std::wstring& pattern, const std::wstring& relativePath, const std::wstring& name, bool isDirectory) {
        const std::wstring pat = Trim(pattern);
        if (pat.empty()) {
            return false;
        }

        const bool hasWildcard = pat.find(L'*') != std::wstring::npos || pat.find(L'?') != std::wstring::npos;
        const bool hasPathSep = pat.find(L'\\') != std::wstring::npos || pat.find(L'/') != std::wstring::npos;

        if (hasPathSep) {
            std::wstring normalizedPath = ToLower(relativePath);
            std::replace(normalizedPath.begin(), normalizedPath.end(), L'/', L'\\');
            std::wstring normalizedPat = ToLower(pat);
            std::replace(normalizedPat.begin(), normalizedPat.end(), L'/', L'\\');
            return GlobMatch(normalizedPat, normalizedPath);
        }

        if (hasWildcard) {
            if (GlobMatch(pat, name)) {
                return true;
            }
            return GlobMatch(pat, relativePath);
        }

        if (ToLower(name) == ToLower(pat)) {
            return true;
        }

        std::wstring normalizedPath = ToLower(relativePath);
        std::replace(normalizedPath.begin(), normalizedPath.end(), L'/', L'\\');
        const std::wstring needle = L"\\" + ToLower(pat) + L"\\";
        if (normalizedPath.find(needle) != std::wstring::npos) {
            return true;
        }
        if (normalizedPath.rfind(ToLower(pat) + L"\\", 0) == 0) {
            return true;
        }
        if (normalizedPath.size() >= pat.size() + 1 &&
            normalizedPath.compare(normalizedPath.size() - pat.size() - 1, pat.size() + 1, L"\\" + ToLower(pat)) == 0) {
            return true;
        }

        (void)isDirectory;
        return false;
    }

    bool PathFilter::IsExcluded(const FilterOptions& options, const std::wstring& relativePath, const std::wstring& name, bool isDirectory) {
        for (const auto& pattern : options.excludePatterns) {
            if (MatchesPattern(pattern, relativePath, name, isDirectory)) {
                return true;
            }
        }

        if (!options.includePatterns.empty()) {
            bool included = false;
            for (const auto& pattern : options.includePatterns) {
                if (MatchesPattern(pattern, relativePath, name, isDirectory)) {
                    included = true;
                    break;
                }
            }
            if (!included) {
                return true;
            }
        }

        return false;
    }

    bool PathFilter::ShouldSkipDirectory(const FilterOptions& options, const std::wstring& dirName) {
        for (const auto& pattern : options.excludePatterns) {
            const std::wstring pat = Trim(pattern);
            if (pat.empty()) {
                continue;
            }
            const bool hasWildcard = pat.find(L'*') != std::wstring::npos || pat.find(L'?') != std::wstring::npos;
            const bool hasPathSep = pat.find(L'\\') != std::wstring::npos || pat.find(L'/') != std::wstring::npos;
            if (!hasWildcard && !hasPathSep && ToLower(dirName) == ToLower(pat)) {
                return true;
            }
            if (MatchesPattern(pat, dirName, dirName, true)) {
                return true;
            }
        }
        return false;
    }

} // namespace ChronoSync
