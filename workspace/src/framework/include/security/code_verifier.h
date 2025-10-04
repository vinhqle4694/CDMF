#ifndef CDMF_CODE_VERIFIER_H
#define CDMF_CODE_VERIFIER_H

#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace cdmf {
namespace security {

/**
 * @enum VerificationResult
 * @brief Result of code verification
 */
enum class VerificationResult {
    VERIFIED,           ///< Code signature is valid and trusted
    INVALID_SIGNATURE,  ///< Signature is malformed or invalid
    UNTRUSTED,          ///< Signature is valid but signer is not trusted
    EXPIRED,            ///< Signature has expired
    REVOKED,            ///< Signing certificate has been revoked
    NOT_SIGNED,         ///< Code is not signed
    ERROR               ///< Verification error occurred
};

/**
 * @enum SignatureAlgorithm
 * @brief Supported signature algorithms
 */
enum class SignatureAlgorithm {
    RSA_SHA256,         ///< RSA with SHA-256
    RSA_SHA512,         ///< RSA with SHA-512
    ECDSA_SHA256,       ///< ECDSA with SHA-256
    ECDSA_SHA512        ///< ECDSA with SHA-512
};

/**
 * @struct VerificationInfo
 * @brief Detailed information about code verification
 */
struct VerificationInfo {
    VerificationResult result;
    std::string signerName;
    std::string signerEmail;
    std::string algorithm;
    std::string timestamp;
    std::string errorMessage;
    bool isTimestamped;
    bool isChainValid;
};

/**
 * @struct TrustedSigner
 * @brief Information about a trusted code signer
 */
struct TrustedSigner {
    std::string name;
    std::string email;
    std::string publicKeyPath;
    std::string fingerprint;
    bool enabled;
};

/**
 * @class CodeVerifier
 * @brief Verifies digital signatures of module code
 *
 * The CodeVerifier ensures that modules are signed by trusted parties
 * and have not been tampered with. It supports:
 * - Digital signature verification using RSA and ECDSA
 * - Certificate chain validation
 * - Trusted signer management
 * - Signature timestamp verification
 */
class CodeVerifier {
public:
    /**
     * @brief Constructor
     */
    CodeVerifier();

    /**
     * @brief Destructor
     */
    ~CodeVerifier();

    /**
     * @brief Verify a module's code signature
     * @param modulePath Path to the module binary
     * @return VerificationInfo with detailed verification result
     */
    VerificationInfo verifyModule(const std::string& modulePath);

    /**
     * @brief Verify a file's signature
     * @param filePath Path to the file
     * @param signaturePath Path to the signature file
     * @return VerificationInfo with detailed verification result
     */
    VerificationInfo verifyFile(const std::string& filePath,
                                const std::string& signaturePath);

    /**
     * @brief Add a trusted signer
     * @param signer The trusted signer information
     * @return true if added successfully
     */
    bool addTrustedSigner(const TrustedSigner& signer);

    /**
     * @brief Remove a trusted signer
     * @param fingerprint The signer's fingerprint
     * @return true if removed successfully
     */
    bool removeTrustedSigner(const std::string& fingerprint);

    /**
     * @brief Get all trusted signers
     * @return Vector of trusted signers
     */
    std::vector<TrustedSigner> getTrustedSigners() const;

    /**
     * @brief Check if a signer is trusted
     * @param fingerprint The signer's fingerprint
     * @return true if trusted
     */
    bool isTrustedSigner(const std::string& fingerprint) const;

    /**
     * @brief Enable or disable a trusted signer
     * @param fingerprint The signer's fingerprint
     * @param enabled true to enable, false to disable
     * @return true if updated successfully
     */
    bool setSignerEnabled(const std::string& fingerprint, bool enabled);

    /**
     * @brief Load trusted signers from configuration
     * @param configPath Path to the configuration file
     * @return true if loaded successfully
     */
    bool loadTrustedSigners(const std::string& configPath);

    /**
     * @brief Save trusted signers to configuration
     * @param configPath Path to the configuration file
     * @return true if saved successfully
     */
    bool saveTrustedSigners(const std::string& configPath) const;

    /**
     * @brief Set whether to require all modules to be signed
     * @param required true to require signatures
     */
    void setSignatureRequired(bool required);

    /**
     * @brief Check if signatures are required
     * @return true if signatures are required
     */
    bool isSignatureRequired() const;

    /**
     * @brief Set whether to verify certificate chains
     * @param verify true to verify chains
     */
    void setVerifyCertificateChain(bool verify);

    /**
     * @brief Check if certificate chain verification is enabled
     * @return true if enabled
     */
    bool isVerifyCertificateChain() const;

    /**
     * @brief Set whether to verify signature timestamps
     * @param verify true to verify timestamps
     */
    void setVerifyTimestamp(bool verify);

    /**
     * @brief Check if timestamp verification is enabled
     * @return true if enabled
     */
    bool isVerifyTimestamp() const;

    /**
     * @brief Compute file hash
     * @param filePath Path to the file
     * @param algorithm Hash algorithm (SHA256, SHA512)
     * @return Hex-encoded hash string
     */
    std::string computeFileHash(const std::string& filePath,
                                const std::string& algorithm = "SHA256");

    /**
     * @brief Convert VerificationResult to string
     * @param result The verification result
     * @return String representation
     */
    static std::string verificationResultToString(VerificationResult result);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;

    /**
     * @brief Verify RSA signature
     * @param data The data that was signed
     * @param signature The signature to verify
     * @param publicKeyPath Path to public key
     * @return true if signature is valid
     */
    bool verifyRSASignature(const std::string& data,
                           const std::string& signature,
                           const std::string& publicKeyPath);

    /**
     * @brief Verify ECDSA signature
     * @param data The data that was signed
     * @param signature The signature to verify
     * @param publicKeyPath Path to public key
     * @return true if signature is valid
     */
    bool verifyECDSASignature(const std::string& data,
                             const std::string& signature,
                             const std::string& publicKeyPath);

    /**
     * @brief Extract signature from module
     * @param modulePath Path to the module
     * @return Signature data or empty if not signed
     */
    std::string extractSignature(const std::string& modulePath);

    /**
     * @brief Parse signature metadata
     * @param signature The signature data
     * @return VerificationInfo with metadata
     */
    VerificationInfo parseSignatureMetadata(const std::string& signature);
};

} // namespace security
} // namespace cdmf

#endif // CDMF_CODE_VERIFIER_H
