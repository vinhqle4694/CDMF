#ifndef CDMF_MANIFEST_PARSER_H
#define CDMF_MANIFEST_PARSER_H

#include "utils/version.h"
#include "utils/version_range.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <map>

namespace cdmf {

/**
 * @brief Module dependency specification
 */
struct ModuleDependency {
    std::string symbolicName;     ///< Required module symbolic name
    VersionRange versionRange;    ///< Compatible version range
    bool optional;                ///< Optional dependency (won't fail if missing)

    ModuleDependency()
        : symbolicName()
        , versionRange()
        , optional(false) {}

    ModuleDependency(const std::string& name, const VersionRange& range, bool opt = false)
        : symbolicName(name)
        , versionRange(range)
        , optional(opt) {}
};

/**
 * @brief Module manifest data
 *
 * Parsed representation of a module's manifest.json file.
 */
struct ModuleManifest {
    // Basic metadata
    std::string symbolicName;
    Version version;
    std::string name;
    std::string library;
    std::string description;
    std::string vendor;
    std::string category;
    std::string activator;
    bool autoStart;

    // Dependencies
    std::vector<ModuleDependency> dependencies;

    // Exported packages
    struct ExportedPackage {
        std::string package;
        Version version;
    };
    std::vector<ExportedPackage> exportedPackages;

    // Imported packages
    struct ImportedPackage {
        std::string package;
        VersionRange versionRange;
    };
    std::vector<ImportedPackage> importedPackages;

    // Services
    struct ProvidedService {
        std::string interface;
        std::map<std::string, std::string> properties;
    };
    std::vector<ProvidedService> providedServices;

    struct RequiredService {
        std::string interface;
        std::string cardinality;  // "0..1", "1..1", "0..n", "1..n"
    };
    std::vector<RequiredService> requiredServices;

    // CLI Methods
    struct CLIMethodArgument {
        std::string name;
        std::string type;
        bool required;
        std::string description;
    };

    struct CLIMethod {
        std::string interface;          ///< Service interface (e.g., "cdmf::IConfigurationAdmin")
        std::string method;              ///< Method name (e.g., "createConfiguration")
        std::string signature;           ///< Method signature (e.g., "(pid:string) -> void")
        std::string description;         ///< Method description
        std::vector<CLIMethodArgument> arguments;  ///< Method arguments
    };
    std::vector<CLIMethod> cliMethods;

    // Security
    std::vector<std::string> permissions;
    bool sandboxEnabled;

    // Raw JSON
    nlohmann::json rawJson;

    ModuleManifest()
        : symbolicName()
        , version()
        , name()
        , library()
        , description()
        , vendor()
        , category()
        , activator()
        , autoStart(false)
        , dependencies()
        , exportedPackages()
        , importedPackages()
        , providedServices()
        , requiredServices()
        , cliMethods()
        , permissions()
        , sandboxEnabled(false)
        , rawJson() {}
};

/**
 * @brief Module manifest parser
 *
 * Parses and validates module manifest JSON files.
 *
 * Manifest schema:
 * @code
 * {
 *   "module": {
 *     "symbolic-name": "com.example.mymodule",
 *     "version": "1.2.3",
 *     "name": "My Module",
 *     "description": "Example module",
 *     "vendor": "Example Corp",
 *     "category": "utility",
 *     "activator": "MyModuleActivator",
 *     "auto-start": true
 *   },
 *   "dependencies": [...],
 *   "exported-packages": [...],
 *   "imported-packages": [...],
 *   "services": {...},
 *   "security": {...}
 * }
 * @endcode
 */
class ManifestParser {
public:
    /**
     * @brief Parse manifest from JSON file
     *
     * @param manifestPath Path to manifest.json file
     * @return Parsed manifest data
     * @throws std::runtime_error if file cannot be read
     * @throws std::runtime_error if JSON is invalid
     * @throws std::runtime_error if required fields are missing
     */
    static ModuleManifest parseFile(const std::string& manifestPath);

    /**
     * @brief Parse manifest from JSON string
     *
     * @param jsonString JSON manifest content
     * @return Parsed manifest data
     * @throws std::runtime_error if JSON is invalid
     * @throws std::runtime_error if required fields are missing
     */
    static ModuleManifest parseString(const std::string& jsonString);

    /**
     * @brief Parse manifest from JSON object
     *
     * @param json Parsed JSON object
     * @return Parsed manifest data
     * @throws std::runtime_error if required fields are missing
     */
    static ModuleManifest parse(const nlohmann::json& json);

    /**
     * @brief Validate manifest completeness
     *
     * @param manifest Manifest to validate
     * @return true if valid
     * @throws std::runtime_error if validation fails (with details)
     */
    static bool validate(const ModuleManifest& manifest);

private:
    static void parseModuleSection(const nlohmann::json& json, ModuleManifest& manifest);
    static void parseDependencies(const nlohmann::json& json, ModuleManifest& manifest);
    static void parseExportedPackages(const nlohmann::json& json, ModuleManifest& manifest);
    static void parseImportedPackages(const nlohmann::json& json, ModuleManifest& manifest);
    static void parseServices(const nlohmann::json& json, ModuleManifest& manifest);
    static void parseCLIMethods(const nlohmann::json& json, ModuleManifest& manifest);
    static void parseSecurity(const nlohmann::json& json, ModuleManifest& manifest);

    static std::string getString(const nlohmann::json& json, const std::string& key,
                                 const std::string& defaultValue = "");
    static bool getBool(const nlohmann::json& json, const std::string& key,
                       bool defaultValue = false);
};

} // namespace cdmf

#endif // CDMF_MANIFEST_PARSER_H
