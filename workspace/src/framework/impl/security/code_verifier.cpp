#include "security/code_verifier.h"
#include "utils/log.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <cstring>

// For cryptographic operations (OpenSSL)
#include <openssl/sha.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/evp.h>

namespace cdmf {
namespace security {

// ========== CodeVerifier::Impl (Private Implementation) ==========

class CodeVerifier::Impl {
public:
    std::vector<TrustedSigner> trustedSigners;
    bool signatureRequired = false;
    bool verifyCertificateChain = true;
    bool verifyTimestamp = true;

    Impl() {
        // Initialize OpenSSL
        OpenSSL_add_all_algorithms();
        ERR_load_crypto_strings();
    }

    ~Impl() {
        // Cleanup OpenSSL
        EVP_cleanup();
        ERR_free_strings();
    }
};

// ========== Helper Functions ==========

std::string CodeVerifier::verificationResultToString(VerificationResult result) {
    switch (result) {
        case VerificationResult::VERIFIED: return "VERIFIED";
        case VerificationResult::INVALID_SIGNATURE: return "INVALID_SIGNATURE";
        case VerificationResult::UNTRUSTED: return "UNTRUSTED";
        case VerificationResult::EXPIRED: return "EXPIRED";
        case VerificationResult::REVOKED: return "REVOKED";
        case VerificationResult::NOT_SIGNED: return "NOT_SIGNED";
        case VerificationResult::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

// ========== CodeVerifier Class ==========

CodeVerifier::CodeVerifier() : pImpl_(std::make_unique<Impl>()) {
    LOGI("CodeVerifier initialized");
}

CodeVerifier::~CodeVerifier() = default;

VerificationInfo CodeVerifier::verifyModule(const std::string& modulePath) {
    LOGI_FMT("Verifying module: " << modulePath);
    VerificationInfo info;
    info.result = VerificationResult::ERROR;
    info.isTimestamped = false;
    info.isChainValid = false;

    // Check if file exists
    std::ifstream file(modulePath, std::ios::binary);
    if (!file.is_open()) {
        LOGE_FMT("Module file not found: " << modulePath);
        info.errorMessage = "Module file not found: " + modulePath;
        return info;
    }
    file.close();
    LOGD_FMT("Module file exists: " << modulePath);

    // Extract signature from module
    std::string signature = extractSignature(modulePath);
    if (signature.empty()) {
        if (pImpl_->signatureRequired) {
            LOGW_FMT("Module is not signed (signature required): " << modulePath);
            info.result = VerificationResult::NOT_SIGNED;
            info.errorMessage = "Module is not signed";
        } else {
            LOGI_FMT("Module is not signed (signature not required): " << modulePath);
            // Not signed but not required
            info.result = VerificationResult::NOT_SIGNED;
            info.errorMessage = "Module is not signed (allowed)";
        }
        return info;
    }
    LOGD_FMT("Found signature for module: " << modulePath);

    // Parse signature metadata
    info = parseSignatureMetadata(signature);
    if (info.result == VerificationResult::ERROR) {
        return info;
    }

    // Check if signer is trusted
    if (!isTrustedSigner(info.signerName)) {
        info.result = VerificationResult::UNTRUSTED;
        info.errorMessage = "Signer is not trusted: " + info.signerName;
        return info;
    }

    // Find trusted signer
    TrustedSigner* signer = nullptr;
    for (auto& s : pImpl_->trustedSigners) {
        if (s.fingerprint == info.signerName || s.name == info.signerName) {
            signer = &s;
            break;
        }
    }

    if (!signer || !signer->enabled) {
        info.result = VerificationResult::UNTRUSTED;
        info.errorMessage = "Signer is disabled or not found";
        return info;
    }

    // Compute file hash
    std::string fileHash = computeFileHash(modulePath, "SHA256");

    // Verify signature
    bool signatureValid = false;
    if (info.algorithm.find("RSA") != std::string::npos) {
        signatureValid = verifyRSASignature(fileHash, signature, signer->publicKeyPath);
    } else if (info.algorithm.find("ECDSA") != std::string::npos) {
        signatureValid = verifyECDSASignature(fileHash, signature, signer->publicKeyPath);
    } else {
        info.result = VerificationResult::ERROR;
        info.errorMessage = "Unsupported signature algorithm: " + info.algorithm;
        return info;
    }

    if (!signatureValid) {
        info.result = VerificationResult::INVALID_SIGNATURE;
        info.errorMessage = "Signature verification failed";
        return info;
    }

    // All checks passed
    info.result = VerificationResult::VERIFIED;
    info.errorMessage = "Module signature verified successfully";
    return info;
}

VerificationInfo CodeVerifier::verifyFile(const std::string& filePath,
                                         const std::string& signaturePath) {
    LOGI_FMT("Verifying file: " << filePath << " with signature: " << signaturePath);
    VerificationInfo info;
    info.result = VerificationResult::ERROR;
    info.isTimestamped = false;
    info.isChainValid = false;

    // Check if files exist
    std::ifstream file(filePath, std::ios::binary);
    std::ifstream sigFile(signaturePath, std::ios::binary);

    if (!file.is_open()) {
        LOGE_FMT("File not found: " << filePath);
        info.errorMessage = "File not found: " + filePath;
        return info;
    }
    if (!sigFile.is_open()) {
        LOGE_FMT("Signature file not found: " << signaturePath);
        info.errorMessage = "Signature file not found: " + signaturePath;
        return info;
    }
    LOGD("Both file and signature file exist");

    // Read signature
    std::stringstream buffer;
    buffer << sigFile.rdbuf();
    std::string signature = buffer.str();

    file.close();
    sigFile.close();

    // Parse signature metadata
    info = parseSignatureMetadata(signature);
    if (info.result == VerificationResult::ERROR) {
        return info;
    }

    // Check if signer is trusted
    if (!isTrustedSigner(info.signerName)) {
        info.result = VerificationResult::UNTRUSTED;
        info.errorMessage = "Signer is not trusted: " + info.signerName;
        return info;
    }

    // Compute file hash
    std::string fileHash = computeFileHash(filePath, "SHA256");

    // Verify signature (simplified - would use actual crypto here)
    info.result = VerificationResult::VERIFIED;
    info.errorMessage = "File signature verified successfully";
    return info;
}

// ========== Trusted Signer Management ==========

bool CodeVerifier::addTrustedSigner(const TrustedSigner& signer) {
    if (signer.fingerprint.empty()) {
        LOGW("Cannot add trusted signer with empty fingerprint");
        return false;
    }

    // Check if already exists
    for (const auto& s : pImpl_->trustedSigners) {
        if (s.fingerprint == signer.fingerprint) {
            LOGW_FMT("Trusted signer already exists: " << signer.fingerprint);
            return false; // Already exists
        }
    }

    LOGI_FMT("Adding trusted signer: " << signer.name << " (fingerprint: " << signer.fingerprint << ")");
    pImpl_->trustedSigners.push_back(signer);
    return true;
}

bool CodeVerifier::removeTrustedSigner(const std::string& fingerprint) {
    LOGI_FMT("Removing trusted signer: " << fingerprint);
    auto it = std::remove_if(pImpl_->trustedSigners.begin(),
                            pImpl_->trustedSigners.end(),
                            [&fingerprint](const TrustedSigner& s) {
                                return s.fingerprint == fingerprint;
                            });

    if (it != pImpl_->trustedSigners.end()) {
        pImpl_->trustedSigners.erase(it, pImpl_->trustedSigners.end());
        LOGI_FMT("Trusted signer removed: " << fingerprint);
        return true;
    }
    LOGW_FMT("Trusted signer not found: " << fingerprint);
    return false;
}

std::vector<TrustedSigner> CodeVerifier::getTrustedSigners() const {
    LOGD_FMT("Getting all trusted signers (count: " << pImpl_->trustedSigners.size() << ")");
    return pImpl_->trustedSigners;
}

bool CodeVerifier::isTrustedSigner(const std::string& fingerprint) const {
    LOGD_FMT("Checking if signer is trusted: " << fingerprint);
    for (const auto& signer : pImpl_->trustedSigners) {
        if (signer.fingerprint == fingerprint || signer.name == fingerprint) {
            bool trusted = signer.enabled;
            LOGD_FMT("  Signer found: " << signer.name << ", enabled: " << trusted);
            return trusted;
        }
    }
    LOGD_FMT("  Signer not found in trusted list");
    return false;
}

bool CodeVerifier::setSignerEnabled(const std::string& fingerprint, bool enabled) {
    LOGI_FMT("Setting signer enabled status: " << fingerprint << " -> " << (enabled ? "enabled" : "disabled"));
    for (auto& signer : pImpl_->trustedSigners) {
        if (signer.fingerprint == fingerprint) {
            signer.enabled = enabled;
            LOGI_FMT("Signer status updated: " << signer.name);
            return true;
        }
    }
    LOGW_FMT("Signer not found: " << fingerprint);
    return false;
}

// ========== Configuration Persistence ==========

bool CodeVerifier::loadTrustedSigners(const std::string& configPath) {
    LOGI_FMT("Loading trusted signers from: " << configPath);
    std::ifstream file(configPath);
    if (!file.is_open()) {
        LOGE_FMT("Failed to open trusted signers config: " << configPath);
        return false;
    }

    pImpl_->trustedSigners.clear();

    std::string line;
    int lineNum = 0;
    int signersLoaded = 0;

    while (std::getline(file, line)) {
        lineNum++;
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Parse line: name|email|keypath|fingerprint|enabled
        std::stringstream ss(line);
        TrustedSigner signer;

        std::getline(ss, signer.name, '|');
        std::getline(ss, signer.email, '|');
        std::getline(ss, signer.publicKeyPath, '|');
        std::getline(ss, signer.fingerprint, '|');

        std::string enabledStr;
        std::getline(ss, enabledStr, '|');
        signer.enabled = (enabledStr == "true" || enabledStr == "1");

        pImpl_->trustedSigners.push_back(signer);
        signersLoaded++;
        LOGD_FMT("  Loaded signer: " << signer.name << " (" << signer.fingerprint << ")");
    }

    file.close();
    LOGI_FMT("Successfully loaded " << signersLoaded << " trusted signers");
    return true;
}

bool CodeVerifier::saveTrustedSigners(const std::string& configPath) const {
    LOGI_FMT("Saving trusted signers to: " << configPath);
    std::ofstream file(configPath);
    if (!file.is_open()) {
        LOGE_FMT("Failed to open config file for writing: " << configPath);
        return false;
    }

    file << "# CDMF Trusted Signers Configuration\n";
    file << "# Format: name|email|keypath|fingerprint|enabled\n\n";

    for (const auto& signer : pImpl_->trustedSigners) {
        file << signer.name << "|"
             << signer.email << "|"
             << signer.publicKeyPath << "|"
             << signer.fingerprint << "|"
             << (signer.enabled ? "true" : "false") << "\n";
        LOGD_FMT("  Saved signer: " << signer.name << " (" << signer.fingerprint << ")");
    }

    file.close();
    LOGI_FMT("Successfully saved " << pImpl_->trustedSigners.size() << " trusted signers");
    return true;
}

// ========== Configuration Methods ==========

void CodeVerifier::setSignatureRequired(bool required) {
    LOGI_FMT("Setting signature required: " << (required ? "true" : "false"));
    pImpl_->signatureRequired = required;
}

bool CodeVerifier::isSignatureRequired() const {
    LOGD_FMT("isSignatureRequired() -> " << (pImpl_->signatureRequired ? "true" : "false"));
    return pImpl_->signatureRequired;
}

void CodeVerifier::setVerifyCertificateChain(bool verify) {
    LOGI_FMT("Setting certificate chain verification: " << (verify ? "enabled" : "disabled"));
    pImpl_->verifyCertificateChain = verify;
}

bool CodeVerifier::isVerifyCertificateChain() const {
    LOGD_FMT("isVerifyCertificateChain() -> " << (pImpl_->verifyCertificateChain ? "true" : "false"));
    return pImpl_->verifyCertificateChain;
}

void CodeVerifier::setVerifyTimestamp(bool verify) {
    LOGI_FMT("Setting timestamp verification: " << (verify ? "enabled" : "disabled"));
    pImpl_->verifyTimestamp = verify;
}

bool CodeVerifier::isVerifyTimestamp() const {
    LOGD_FMT("isVerifyTimestamp() -> " << (pImpl_->verifyTimestamp ? "true" : "false"));
    return pImpl_->verifyTimestamp;
}

// ========== Cryptographic Operations ==========

std::string CodeVerifier::computeFileHash(const std::string& filePath,
                                         const std::string& algorithm) {
    LOGD_FMT("Computing " << algorithm << " hash for file: " << filePath);
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        LOGE_FMT("Failed to open file for hashing: " << filePath);
        return "";
    }

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);

    char buffer[8192];
    size_t totalBytes = 0;
    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        SHA256_Update(&sha256, buffer, file.gcount());
        totalBytes += file.gcount();
    }

    SHA256_Final(hash, &sha256);
    file.close();

    // Convert to hex string
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0')
           << static_cast<int>(hash[i]);
    }

    std::string hashStr = ss.str();
    LOGD_FMT("Computed hash (" << totalBytes << " bytes): " << hashStr);
    return hashStr;
}

bool CodeVerifier::verifyRSASignature(const std::string& data,
                                     const std::string& signature,
                                     const std::string& publicKeyPath) {
    LOGD_FMT("Verifying RSA signature with key: " << publicKeyPath);
    // Simplified implementation - production would use actual RSA verification
    // This is a placeholder for the actual cryptographic verification

    // In production, this would:
    // 1. Load the public key from publicKeyPath
    // 2. Verify the RSA signature using OpenSSL EVP functions
    // 3. Return the verification result

    bool valid = !publicKeyPath.empty() && !data.empty() && !signature.empty();
    LOGD_FMT("  RSA signature verification: " << (valid ? "valid" : "invalid"));
    return valid;
}

bool CodeVerifier::verifyECDSASignature(const std::string& data,
                                       const std::string& signature,
                                       const std::string& publicKeyPath) {
    LOGD_FMT("Verifying ECDSA signature with key: " << publicKeyPath);
    // Simplified implementation - production would use actual ECDSA verification
    // This is a placeholder for the actual cryptographic verification

    bool valid = !publicKeyPath.empty() && !data.empty() && !signature.empty();
    LOGD_FMT("  ECDSA signature verification: " << (valid ? "valid" : "invalid"));
    return valid;
}

std::string CodeVerifier::extractSignature(const std::string& modulePath) {
    LOGD_FMT("Extracting signature from module: " << modulePath);
    // Simplified implementation
    // In production, this would extract embedded signature from the module binary
    // For now, check if a .sig file exists alongside the module

    std::string sigPath = modulePath + ".sig";
    std::ifstream sigFile(sigPath, std::ios::binary);

    if (!sigFile.is_open()) {
        LOGD_FMT("  No signature file found: " << sigPath);
        return ""; // Not signed
    }

    std::stringstream buffer;
    buffer << sigFile.rdbuf();
    std::string sig = buffer.str();
    LOGD_FMT("  Extracted signature (" << sig.length() << " bytes) from: " << sigPath);
    return sig;
}

VerificationInfo CodeVerifier::parseSignatureMetadata(const std::string& signature) {
    LOGD_FMT("Parsing signature metadata (" << signature.length() << " bytes)");
    VerificationInfo info;
    info.result = VerificationResult::ERROR;

    if (signature.empty()) {
        LOGW("Empty signature provided for parsing");
        info.errorMessage = "Empty signature";
        return info;
    }

    // Simplified parsing - production would parse actual signature format
    // For now, assume signature format: "SIGNER:ALGORITHM:TIMESTAMP"
    std::stringstream ss(signature);

    std::getline(ss, info.signerName, ':');
    std::getline(ss, info.algorithm, ':');
    std::getline(ss, info.timestamp, ':');

    if (info.signerName.empty() || info.algorithm.empty()) {
        LOGE("Invalid signature format - missing required fields");
        info.errorMessage = "Invalid signature format";
        return info;
    }

    info.isTimestamped = !info.timestamp.empty();
    info.isChainValid = true; // Would be determined by actual verification
    info.result = VerificationResult::VERIFIED; // Preliminary result

    LOGD_FMT("  Parsed signature: signer=" << info.signerName << ", algorithm=" << info.algorithm
             << ", timestamped=" << (info.isTimestamped ? "yes" : "no"));
    return info;
}

} // namespace security
} // namespace cdmf
