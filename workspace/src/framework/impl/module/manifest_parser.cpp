#include "module/manifest_parser.h"
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace cdmf {

ModuleManifest ManifestParser::parseFile(const std::string& manifestPath) {
    std::ifstream file(manifestPath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open manifest file: " + manifestPath);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return parseString(buffer.str());
}

ModuleManifest ManifestParser::parseString(const std::string& jsonString) {
    try {
        nlohmann::json json = nlohmann::json::parse(jsonString);
        return parse(json);
    } catch (const nlohmann::json::parse_error& e) {
        throw std::runtime_error(std::string("JSON parse error: ") + e.what());
    }
}

ModuleManifest ManifestParser::parse(const nlohmann::json& json) {
    ModuleManifest manifest;
    manifest.rawJson = json;

    // Parse required module section
    if (!json.contains("module")) {
        throw std::runtime_error("Manifest missing required 'module' section");
    }

    parseModuleSection(json["module"], manifest);

    // Parse optional sections
    if (json.contains("dependencies")) {
        parseDependencies(json["dependencies"], manifest);
    }

    if (json.contains("exported-packages")) {
        parseExportedPackages(json["exported-packages"], manifest);
    }

    if (json.contains("imported-packages")) {
        parseImportedPackages(json["imported-packages"], manifest);
    }

    if (json.contains("services")) {
        parseServices(json["services"], manifest);
    }

    if (json.contains("cli-methods")) {
        parseCLIMethods(json["cli-methods"], manifest);
    }

    if (json.contains("security")) {
        parseSecurity(json["security"], manifest);
    }

    // Validate
    validate(manifest);

    return manifest;
}

bool ManifestParser::validate(const ModuleManifest& manifest) {
    if (manifest.symbolicName.empty()) {
        throw std::runtime_error("Module symbolic-name is required");
    }

    if (manifest.version.toString().empty()) {
        throw std::runtime_error("Module version is required");
    }

    return true;
}

void ManifestParser::parseModuleSection(const nlohmann::json& json, ModuleManifest& manifest) {
    manifest.symbolicName = getString(json, "symbolic-name");
    if (manifest.symbolicName.empty()) {
        throw std::runtime_error("Module symbolic-name is required");
    }

    std::string versionStr = getString(json, "version");
    if (versionStr.empty()) {
        throw std::runtime_error("Module version is required");
    }
    manifest.version = Version::parse(versionStr);

    manifest.name = getString(json, "name", manifest.symbolicName);
    manifest.library = getString(json, "library");  // Optional: library filename
    manifest.description = getString(json, "description");
    manifest.vendor = getString(json, "vendor");
    manifest.category = getString(json, "category", "general");
    manifest.activator = getString(json, "activator");
    manifest.autoStart = getBool(json, "auto-start", false);
}

void ManifestParser::parseDependencies(const nlohmann::json& json, ModuleManifest& manifest) {
    if (!json.is_array()) {
        throw std::runtime_error("'dependencies' must be an array");
    }

    for (const auto& dep : json) {
        ModuleDependency dependency;

        dependency.symbolicName = getString(dep, "symbolic-name");
        if (dependency.symbolicName.empty()) {
            throw std::runtime_error("Dependency symbolic-name is required");
        }

        std::string versionRangeStr = getString(dep, "version-range", "[0.0.0,)");
        dependency.versionRange = VersionRange::parse(versionRangeStr);

        dependency.optional = getBool(dep, "optional", false);

        manifest.dependencies.push_back(dependency);
    }
}

void ManifestParser::parseExportedPackages(const nlohmann::json& json, ModuleManifest& manifest) {
    if (!json.is_array()) {
        throw std::runtime_error("'exported-packages' must be an array");
    }

    for (const auto& pkg : json) {
        ModuleManifest::ExportedPackage exportedPkg;

        exportedPkg.package = getString(pkg, "package");
        if (exportedPkg.package.empty()) {
            throw std::runtime_error("Exported package name is required");
        }

        std::string versionStr = getString(pkg, "version", "0.0.0");
        exportedPkg.version = Version::parse(versionStr);

        manifest.exportedPackages.push_back(exportedPkg);
    }
}

void ManifestParser::parseImportedPackages(const nlohmann::json& json, ModuleManifest& manifest) {
    if (!json.is_array()) {
        throw std::runtime_error("'imported-packages' must be an array");
    }

    for (const auto& pkg : json) {
        ModuleManifest::ImportedPackage importedPkg;

        importedPkg.package = getString(pkg, "package");
        if (importedPkg.package.empty()) {
            throw std::runtime_error("Imported package name is required");
        }

        std::string versionRangeStr = getString(pkg, "version-range", "[0.0.0,)");
        importedPkg.versionRange = VersionRange::parse(versionRangeStr);

        manifest.importedPackages.push_back(importedPkg);
    }
}

void ManifestParser::parseServices(const nlohmann::json& json, ModuleManifest& manifest) {
    if (!json.is_object()) {
        throw std::runtime_error("'services' must be an object");
    }

    // Parse provides
    if (json.contains("provides") && json["provides"].is_array()) {
        for (const auto& svc : json["provides"]) {
            ModuleManifest::ProvidedService providedSvc;

            providedSvc.interface = getString(svc, "interface");
            if (providedSvc.interface.empty()) {
                throw std::runtime_error("Provided service interface is required");
            }

            if (svc.contains("properties") && svc["properties"].is_object()) {
                for (auto it = svc["properties"].begin(); it != svc["properties"].end(); ++it) {
                    providedSvc.properties[it.key()] = it.value().dump();
                }
            }

            manifest.providedServices.push_back(providedSvc);
        }
    }

    // Parse requires
    if (json.contains("requires") && json["requires"].is_array()) {
        for (const auto& svc : json["requires"]) {
            ModuleManifest::RequiredService requiredSvc;

            requiredSvc.interface = getString(svc, "interface");
            if (requiredSvc.interface.empty()) {
                throw std::runtime_error("Required service interface is required");
            }

            requiredSvc.cardinality = getString(svc, "cardinality", "1..1");

            manifest.requiredServices.push_back(requiredSvc);
        }
    }
}

void ManifestParser::parseSecurity(const nlohmann::json& json, ModuleManifest& manifest) {
    if (!json.is_object()) {
        throw std::runtime_error("'security' must be an object");
    }

    // Parse permissions
    if (json.contains("permissions") && json["permissions"].is_array()) {
        for (const auto& perm : json["permissions"]) {
            if (perm.is_string()) {
                manifest.permissions.push_back(perm.get<std::string>());
            }
        }
    }

    // Parse sandbox
    if (json.contains("sandbox") && json["sandbox"].is_object()) {
        manifest.sandboxEnabled = getBool(json["sandbox"], "enabled", false);
    }
}

void ManifestParser::parseCLIMethods(const nlohmann::json& json, ModuleManifest& manifest) {
    if (!json.is_array()) {
        return;
    }

    for (const auto& methodJson : json) {
        if (!methodJson.is_object()) {
            continue;
        }

        ModuleManifest::CLIMethod method;

        // Required fields
        method.interface = getString(methodJson, "interface");
        method.method = getString(methodJson, "method");
        method.signature = getString(methodJson, "signature");
        method.description = getString(methodJson, "description");

        if (method.interface.empty() || method.method.empty()) {
            // Skip invalid method declarations
            continue;
        }

        // Parse arguments (optional)
        if (methodJson.contains("arguments") && methodJson["arguments"].is_array()) {
            for (const auto& argJson : methodJson["arguments"]) {
                if (!argJson.is_object()) {
                    continue;
                }

                ModuleManifest::CLIMethodArgument arg;
                arg.name = getString(argJson, "name");
                arg.type = getString(argJson, "type");
                arg.required = getBool(argJson, "required", false);
                arg.description = getString(argJson, "description");

                method.arguments.push_back(arg);
            }
        }

        manifest.cliMethods.push_back(method);
    }
}

std::string ManifestParser::getString(const nlohmann::json& json, const std::string& key,
                                      const std::string& defaultValue) {
    if (json.contains(key) && json[key].is_string()) {
        return json[key].get<std::string>();
    }
    return defaultValue;
}

bool ManifestParser::getBool(const nlohmann::json& json, const std::string& key,
                             bool defaultValue) {
    if (json.contains(key) && json[key].is_boolean()) {
        return json[key].get<bool>();
    }
    return defaultValue;
}

} // namespace cdmf
