#include "utils/version.h"
#include <sstream>
#include <regex>
#include <algorithm>

namespace cdmf {

Version::Version()
    : major_(0), minor_(0), patch_(0), qualifier_("") {
}

Version::Version(int major, int minor, int patch, const std::string& qualifier)
    : major_(major), minor_(minor), patch_(patch), qualifier_(qualifier) {
    if (major < 0 || minor < 0 || patch < 0) {
        throw std::invalid_argument("Version numbers cannot be negative");
    }
}

Version Version::parse(const std::string& versionString) {
    if (versionString.empty()) {
        throw std::invalid_argument("Version string cannot be empty");
    }

    // Regular expression to match semantic version format
    // Matches: MAJOR.MINOR.PATCH[-QUALIFIER]
    std::regex versionRegex(R"(^(\d+)\.(\d+)\.(\d+)(?:-([a-zA-Z0-9.-]+))?$)");
    std::smatch matches;

    if (!std::regex_match(versionString, matches, versionRegex)) {
        throw std::invalid_argument("Invalid version format: " + versionString);
    }

    int major = std::stoi(matches[1].str());
    int minor = std::stoi(matches[2].str());
    int patch = std::stoi(matches[3].str());
    std::string qualifier = matches[4].matched ? matches[4].str() : "";

    return Version(major, minor, patch, qualifier);
}

std::string Version::toString() const {
    std::ostringstream oss;
    oss << major_ << "." << minor_ << "." << patch_;
    if (!qualifier_.empty()) {
        oss << "-" << qualifier_;
    }
    return oss.str();
}

bool Version::isCompatibleWith(const Version& other) const {
    // Compatible if same major version (semantic versioning rule)
    return major_ == other.major_;
}

bool Version::operator==(const Version& other) const {
    return compare(other) == 0;
}

bool Version::operator!=(const Version& other) const {
    return compare(other) != 0;
}

bool Version::operator<(const Version& other) const {
    return compare(other) < 0;
}

bool Version::operator>(const Version& other) const {
    return compare(other) > 0;
}

bool Version::operator<=(const Version& other) const {
    return compare(other) <= 0;
}

bool Version::operator>=(const Version& other) const {
    return compare(other) >= 0;
}

int Version::compare(const Version& other) const {
    // Compare major version
    if (major_ != other.major_) {
        return major_ < other.major_ ? -1 : 1;
    }

    // Compare minor version
    if (minor_ != other.minor_) {
        return minor_ < other.minor_ ? -1 : 1;
    }

    // Compare patch version
    if (patch_ != other.patch_) {
        return patch_ < other.patch_ ? -1 : 1;
    }

    // Compare qualifiers
    // Empty qualifier is considered greater than any qualifier
    // (release version > pre-release version)
    if (qualifier_.empty() && !other.qualifier_.empty()) {
        return 1;
    }
    if (!qualifier_.empty() && other.qualifier_.empty()) {
        return -1;
    }

    // Both have qualifiers, compare lexicographically
    if (qualifier_ < other.qualifier_) {
        return -1;
    }
    if (qualifier_ > other.qualifier_) {
        return 1;
    }

    return 0;
}

std::ostream& operator<<(std::ostream& os, const Version& version) {
    os << version.toString();
    return os;
}

} // namespace cdmf
