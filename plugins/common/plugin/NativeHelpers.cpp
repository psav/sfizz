// SPDX-License-Identifier: BSD-2-Clause

// This code is part of the sfizz library and is licensed under a BSD 2-clause
// license. You should have receive a LICENSE.md file along with the code.
// If not, contact the sfizz maintainers at https://github.com/sfztools/sfizz

#include "NativeHelpers.h"
#include <absl/strings/match.h>
#include <absl/strings/strip.h>
#include <absl/strings/string_view.h>
#include <stdexcept>
#include <cstdlib>

#if defined(_WIN32)
#include <windows.h>
#include <shlobj.h>

const fs::path& getUserDocumentsDirectory()
{
    static const fs::path directory = []() -> fs::path {
        std::unique_ptr<WCHAR[]> path(new WCHAR[32768]);
        if (SHGetFolderPathW(nullptr, CSIDL_PERSONAL|CSIDL_FLAG_CREATE, NULL, SHGFP_TYPE_CURRENT, path.get()) != S_OK)
            throw std::runtime_error("Cannot get the document directory.");
        return fs::path(path.get());
    }();
    return directory;
}
#elif defined(__APPLE__)
    // implemented in NativeHelpers.mm
#else
const fs::path& getUserDocumentsDirectory()
{
    static const fs::path directory = []() -> fs::path {
        for (const XdgUserDirsEntry& ent :
                 parseXdgUserDirs(getXdgConfigHome() / "user-dirs.dirs")) {
            if (ent.name == "XDG_DOCUMENTS_DIR")
                return ent.value;
        }
        return getUserHomeDirectory() / "Documents";
    }();
    return directory;
}

const fs::path& getUserHomeDirectory()
{
    static const fs::path directory = []() -> fs::path {
        const char* home = getenv("HOME");
        if (home && home[0] == '/')
            return fs::u8path(home);
        else
            throw std::runtime_error("Cannot get the home directory.");
    }();
    return directory;
}

const fs::path& getXdgConfigHome()
{
    static const fs::path directory = []() -> fs::path {
        const char* config = getenv("XDG_CONFIG_HOME");
        if (config && config[0] == '/')
            return fs::u8path(config);
        else
            return getUserHomeDirectory() / ".config";

    }();
    return directory;
}

std::vector<XdgUserDirsEntry> parseXdgUserDirs(const fs::path& userDirsPath)
{
    // from user-dirs.dirs(5)
    //   This file contains lines of the form `XDG_NAME_DIR=VALUE`
    //   VALUE must be of the form "$HOME/Path" or "/Path".
    //   Lines beginning with a # character are ignored.

    std::vector<XdgUserDirsEntry> ents;
    const fs::path& home = getUserHomeDirectory();

    fs::ifstream in(userDirsPath);
    std::string lineBuf;

    lineBuf.reserve(256);
    while (std::getline(in, lineBuf)) {
        absl::string_view line(lineBuf);

        line = absl::StripLeadingAsciiWhitespace(line);
        if (line.empty() || line.front() == '#')
            continue;

        size_t pos = line.find('=');
        if (pos == line.npos)
            continue;

        XdgUserDirsEntry ent;
        ent.name = std::string(line.substr(0, pos));

        absl::string_view rawValue = line.substr(pos + 1);

        rawValue = absl::StripTrailingAsciiWhitespace(rawValue);

        if (rawValue.size() < 2 || rawValue.front() != '"' || rawValue.back() != '"')
            continue;

        rawValue.remove_prefix(1);
        rawValue.remove_suffix(1);

        if (!rawValue.empty() && rawValue.front() == '/')
            ent.value = fs::u8path(rawValue.begin(), rawValue.end());
        else if (absl::StartsWith(rawValue, "$HOME")) {
            absl::string_view part = rawValue.substr(5);
            ent.value = home / fs::u8path(part.begin(), part.end()).relative_path();
        }
        else
            continue;

        ents.push_back(std::move(ent));
    }

    return ents;
}
#endif
