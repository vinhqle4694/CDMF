#include "utils/version_manager.h"
#include <algorithm>

namespace cdmf {

std::optional<Version> VersionManager::findBestMatch(
    const std::vector<Version>& available,
    const VersionRange& range) {

    std::vector<Version> matches = findAllMatches(available, range);

    if (matches.empty()) {
        return std::nullopt;
    }

    // Return the highest version
    return *std::max_element(matches.begin(), matches.end());
}

std::vector<Version> VersionManager::findAllMatches(
    const std::vector<Version>& available,
    const VersionRange& range) {

    std::vector<Version> matches;

    for (const auto& version : available) {
        if (range.includes(version)) {
            matches.push_back(version);
        }
    }

    return matches;
}

std::optional<Version> VersionManager::getLatest(const std::vector<Version>& versions) {
    if (versions.empty()) {
        return std::nullopt;
    }

    return *std::max_element(versions.begin(), versions.end());
}

void VersionManager::sort(std::vector<Version>& versions) {
    std::sort(versions.begin(), versions.end());
}

void VersionManager::sortDescending(std::vector<Version>& versions) {
    std::sort(versions.begin(), versions.end(), std::greater<Version>());
}

bool VersionManager::isValidVersionString(const std::string& versionString) {
    try {
        Version::parse(versionString);
        return true;
    } catch (const std::invalid_argument&) {
        return false;
    }
}

bool VersionManager::isValidRangeString(const std::string& rangeString) {
    try {
        VersionRange::parse(rangeString);
        return true;
    } catch (const std::invalid_argument&) {
        return false;
    }
}

} // namespace cdmf
