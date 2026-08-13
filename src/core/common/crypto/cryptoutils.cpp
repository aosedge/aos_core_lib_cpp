/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/fs.hpp>
#include <core/common/tools/logger.hpp>
#include <core/common/tools/thread.hpp>
#include <core/common/tools/variant.hpp>

#include "cryptoutils.hpp"

namespace aos::crypto {

namespace {

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

Mutex                                sReadBufferMutex;
StaticArray<uint8_t, cFileChunkSize> sReadBuffer;

} // namespace

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error CalculateFileHash(const String& path, const Hash& algorithm, HasherItf& hashProvider, Array<uint8_t>& hash)
{
    auto [hasher, err] = hashProvider.CreateHash(algorithm);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    fs::File file;

    err = file.Open(path, fs::File::Mode::Read);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    LockGuard lock {sReadBufferMutex};

    while (true) {
        err = file.ReadBlock(sReadBuffer);
        if (!err.IsNone() && !err.Is(ErrorEnum::eEOF)) {
            return AOS_ERROR_WRAP(err);
        }

        if (sReadBuffer.IsEmpty()) {
            break;
        }

        err = hasher->Update(sReadBuffer);
        if (!err.IsNone()) {
            if (!err.Is(ErrorEnum::eEOF)) {
                return AOS_ERROR_WRAP(err);
            }

            break;
        }
    }

    err = hasher->Finalize(hash);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * CA certificate validation
 **********************************************************************************************************************/

namespace {

// Intersection of curves supported by mbedTLS and the AOS OpenSSL engine.
constexpr uint8_t cPrime192v1OID[] = {0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x01}; // 1.2.840.10045.3.1.1
constexpr uint8_t cSecp224r1OID[]  = {0x2B, 0x81, 0x04, 0x00, 0x21}; // 1.3.132.0.33
constexpr uint8_t cPrime256v1OID[] = {0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07}; // 1.2.840.10045.3.1.7
constexpr uint8_t cSecp384r1OID[]  = {0x2B, 0x81, 0x04, 0x00, 0x22}; // 1.3.132.0.34
constexpr uint8_t cSecp521r1OID[]  = {0x2B, 0x81, 0x04, 0x00, 0x23}; // 1.3.132.0.35
constexpr uint8_t cSecp192k1OID[]  = {0x2B, 0x81, 0x04, 0x00, 0x1F}; // 1.3.132.0.31
constexpr uint8_t cSecp224k1OID[]  = {0x2B, 0x81, 0x04, 0x00, 0x20}; // 1.3.132.0.32
constexpr uint8_t cSecp256k1OID[]  = {0x2B, 0x81, 0x04, 0x00, 0x0A}; // 1.3.132.0.10
constexpr uint8_t cX25519OID[]     = {0x2B, 0x65, 0x6E}; // 1.3.101.110
constexpr uint8_t cX448OID[]       = {0x2B, 0x65, 0x6F}; // 1.3.101.111
constexpr uint8_t cBrainpoolP256r1OID[]
    = {0x2B, 0x24, 0x03, 0x03, 0x02, 0x08, 0x01, 0x01, 0x07}; // 1.3.36.3.3.2.8.1.1.7
constexpr uint8_t cBrainpoolP384r1OID[]
    = {0x2B, 0x24, 0x03, 0x03, 0x02, 0x08, 0x01, 0x01, 0x0B}; // 1.3.36.3.3.2.8.1.1.11
constexpr uint8_t cBrainpoolP512r1OID[]
    = {0x2B, 0x24, 0x03, 0x03, 0x02, 0x08, 0x01, 0x01, 0x0D}; // 1.3.36.3.3.2.8.1.1.13

struct ECCurveOID {
    const uint8_t* mOID;
    size_t         mOIDLen;
    const char*    mName;
};

constexpr ECCurveOID cSupportedECCurves[] = {
    {cPrime192v1OID, sizeof(cPrime192v1OID), "prime192v1"},
    {cSecp224r1OID, sizeof(cSecp224r1OID), "secp224r1"},
    {cPrime256v1OID, sizeof(cPrime256v1OID), "prime256v1"},
    {cSecp384r1OID, sizeof(cSecp384r1OID), "secp384r1"},
    {cSecp521r1OID, sizeof(cSecp521r1OID), "secp521r1"},
    {cSecp192k1OID, sizeof(cSecp192k1OID), "secp192k1"},
    {cSecp224k1OID, sizeof(cSecp224k1OID), "secp224k1"},
    {cSecp256k1OID, sizeof(cSecp256k1OID), "secp256k1"},
    {cX25519OID, sizeof(cX25519OID), "X25519"},
    {cX448OID, sizeof(cX448OID), "X448"},
    {cBrainpoolP256r1OID, sizeof(cBrainpoolP256r1OID), "brainpoolP256r1"},
    {cBrainpoolP384r1OID, sizeof(cBrainpoolP384r1OID), "brainpoolP384r1"},
    {cBrainpoolP512r1OID, sizeof(cBrainpoolP512r1OID), "brainpoolP512r1"},
};

const uint8_t* ECParamsOIDData(const Array<uint8_t>& oid, size_t& len)
{
    if (oid.Size() >= 2 && oid[0] == 0x06 && static_cast<size_t>(oid[1]) == oid.Size() - 2) {
        len = oid.Size() - 2;

        return oid.Get() + 2;
    }

    len = oid.Size();

    return oid.Get();
}

bool ECParamsOIDEquals(const Array<uint8_t>& oid, const uint8_t* expected, size_t expectedLen)
{
    size_t         len  = 0;
    const uint8_t* data = ECParamsOIDData(oid, len);

    return len == expectedLen && memcmp(data, expected, expectedLen) == 0;
}

const char* GetECCurveName(const Array<uint8_t>& ecParamsOID)
{
    for (const auto& curve : cSupportedECCurves) {
        if (ECParamsOIDEquals(ecParamsOID, curve.mOID, curve.mOIDLen)) {
            return curve.mName;
        }
    }

    return "unknown";
}

bool IsSupportedECCurve(const Array<uint8_t>& ecParamsOID)
{
    for (const auto& curve : cSupportedECCurves) {
        if (ECParamsOIDEquals(ecParamsOID, curve.mOID, curve.mOIDLen)) {
            return true;
        }
    }

    return false;
}

Error CheckCAPublicKey(const x509::Certificate& cert)
{
    const auto& pubKey = GetBase<PublicKeyItf>(cert.mPublicKey);

    switch (pubKey.GetKeyType().GetValue()) {
    case KeyTypeEnum::eRSA: {
        const auto& rsa = static_cast<const RSAPublicKey&>(pubKey);
        if (rsa.GetN().Size() * 8 < 2048) {
            LOG_WRN() << "CA certificate RSA public key length is below 2048 bits: bits=" << rsa.GetN().Size() * 8;
        }

        return ErrorEnum::eNone;
    }

    case KeyTypeEnum::eECDSA: {
        const auto& ecdsa = static_cast<const ECDSAPublicKey&>(pubKey);
        if (!IsSupportedECCurve(ecdsa.GetECParamsOID())) {
            LOG_ERR() << "CA certificate public key curve mismatch: unsupported curve: "
                      << GetECCurveName(ecdsa.GetECParamsOID());

            return AOS_ERROR_WRAP(ErrorEnum::eNotSupported);
        }

        return ErrorEnum::eNone;
    }

    default:
        LOG_ERR() << "CA certificate public key algorithm mismatch: expected RSA or ECDSA, actual "
                  << pubKey.GetKeyType();

        return AOS_ERROR_WRAP(ErrorEnum::eNotSupported);
    }
}

} // namespace

Error ValidateCACert(const x509::Certificate& cert)
{
    if (cert.mVersion != x509::cX509Version3) {
        LOG_ERR() << "CA certificate is not X.509 v3: version=" << cert.mVersion;

        return AOS_ERROR_WRAP(ErrorEnum::eFailed);
    }

    if (!cert.mIsCA) {
        LOG_ERR() << "CA certificate basic constraints mismatch: expected CA:TRUE, actual CA:FALSE";

        return AOS_ERROR_WRAP(ErrorEnum::eFailed);
    }

    if (cert.mKeyUsage.HasValue()) {
        if ((cert.mKeyUsage.GetValue() & x509::keyusage::cKeyCertSign) == 0) {
            LOG_ERR() << "CA certificate key usage mismatch: missing keyCertSign";

            return AOS_ERROR_WRAP(ErrorEnum::eFailed);
        }

        if ((cert.mKeyUsage.GetValue() & x509::keyusage::cCRLSign) == 0) {
            LOG_WRN() << "CA certificate key usage mismatch: missing cRLSign";
        }
    } else {
        LOG_WRN() << "CA certificate key usage extension is missing";
    }

    if (cert.mSubjectKeyId.IsEmpty()) {
        LOG_ERR() << "CA certificate subjectKeyIdentifier is missing";

        return AOS_ERROR_WRAP(ErrorEnum::eFailed);
    }

    return CheckCAPublicKey(cert);
}

} // namespace aos::crypto
