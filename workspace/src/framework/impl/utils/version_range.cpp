#include "utils/version_range.h"
#include <regex>
#include <sstream>

namespace cdmf {

VersionRange::VersionRange()
    : minimum_(0, 0, 0)
    , maximum_(0, 0, 0)
    , includeMinimum_(true)
    , includeMaximum_(true)
    , hasMinimum_(false)
    , hasMaximum_(false) {
}

VersionRange::VersionRange(const Version& minimum, const Version& maximum,
                           bool includeMinimum, bool includeMaximum)
    : minimum_(minimum)
    , maximum_(maximum)
    , includeMinimum_(includeMinimum)
    , includeMaximum_(includeMaximum)
    , hasMinimum_(true)
    , hasMaximum_(true) {
}

VersionRange VersionRange::parse(const std::string& rangeString) {
    if (rangeString.empty()) {
        return VersionRange(); // Unbounded range
    }

    // Trim whitespace
    std::string trimmed = rangeString;
    trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
    trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);

    // Check if it's a simple version (no brackets)
    if (trimmed[0] != '[' && trimmed[0] != '(') {
        // Interpret as [version,)
        try {
            Version ver = Version::parse(trimmed);
            VersionRange range;
            range.minimum_ = ver;
            range.includeMinimum_ = true;
            range.hasMinimum_ = true;
            range.hasMaximum_ = false;
            return range;
        } catch (const std::invalid_argument&) {
            throw std::invalid_argument("Invalid version range format: " + rangeString);
        }
    }

    // Parse interval notation: [min,max] or (min,max) or variations
    // Regex: ([\[\(])\s*([^,\]\)]*)\s*,\s*([^,\]\)]*)\s*([\]\)])
    std::regex rangeRegex(R"(([\[\(])\s*([^,\]\)]*)\s*,\s*([^,\]\)]*)\s*([\]\)]))");
    std::smatch matches;

    if (!std::regex_match(trimmed, matches, rangeRegex)) {
        throw std::invalid_argument("Invalid version range format: " + rangeString);
    }

    char startBracket = matches[1].str()[0];
    std::string minStr = matches[2].str();
    std::string maxStr = matches[3].str();
    char endBracket = matches[4].str()[0];

    // Trim version strings
    auto trim = [](std::string& s) {
        s.erase(0, s.find_first_not_of(" \t\n\r"));
        s.erase(s.find_last_not_of(" \t\n\r") + 1);
    };
    trim(minStr);
    trim(maxStr);

    VersionRange range;

    // Parse minimum version
    if (minStr.empty()) {
        range.hasMinimum_ = false;
    } else {
        range.minimum_ = Version::parse(minStr);
        range.hasMinimum_ = true;
        range.includeMinimum_ = (startBracket == '[');
    }

    // Parse maximum version
    if (maxStr.empty()) {
        range.hasMaximum_ = false;
    } else {
        range.maximum_ = Version::parse(maxStr);
        range.hasMaximum_ = true;
        range.includeMaximum_ = (endBracket == ']');
    }

    // Validate range
    if (range.hasMinimum_ && range.hasMaximum_) {
        if (range.minimum_ > range.maximum_) {
            throw std::invalid_argument("Invalid range: minimum > maximum");
        }
        if (range.minimum_ == range.maximum_ &&
            (!range.includeMinimum_ || !range.includeMaximum_)) {
            throw std::invalid_argument("Invalid range: empty range");
        }
    }

    return range;
}

bool VersionRange::includes(const Version& version) const {
    // Check lower bound
    if (hasMinimum_) {
        if (includeMinimum_) {
            if (version < minimum_) {
                return false;
            }
        } else {
            if (version <= minimum_) {
                return false;
            }
        }
    }

    // Check upper bound
    if (hasMaximum_) {
        if (includeMaximum_) {
            if (version > maximum_) {
                return false;
            }
        } else {
            if (version >= maximum_) {
                return false;
            }
        }
    }

    return true;
}

std::string VersionRange::toString() const {
    if (!hasMinimum_ && !hasMaximum_) {
        return "[0.0.0,)"; // Unbounded
    }

    std::ostringstream oss;

    // Start bracket
    if (hasMinimum_) {
        oss << (includeMinimum_ ? '[' : '(');
        oss << minimum_.toString();
    } else {
        oss << '(';
    }

    oss << ',';

    // End bracket
    if (hasMaximum_) {
        oss << maximum_.toString();
        oss << (includeMaximum_ ? ']' : ')');
    } else {
        oss << ')';
    }

    return oss.str();
}

bool VersionRange::operator==(const VersionRange& other) const {
    if (hasMinimum_ != other.hasMinimum_ || hasMaximum_ != other.hasMaximum_) {
        return false;
    }

    if (hasMinimum_) {
        if (minimum_ != other.minimum_ || includeMinimum_ != other.includeMinimum_) {
            return false;
        }
    }

    if (hasMaximum_) {
        if (maximum_ != other.maximum_ || includeMaximum_ != other.includeMaximum_) {
            return false;
        }
    }

    return true;
}

bool VersionRange::operator!=(const VersionRange& other) const {
    return !(*this == other);
}

} // namespace cdmf
