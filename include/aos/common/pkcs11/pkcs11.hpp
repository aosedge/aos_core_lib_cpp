/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_PKCS11_HPP_
#define AOS_PKCS11_HPP_

#include "aos/common/crypto.hpp"
#include "aos/common/pkcs11/cryptoki/pkcs11.h"
#include "aos/common/tools/log.hpp"
#include "aos/common/tools/memory.hpp"
#include "aos/common/tools/utils.hpp"
#include "aos/common/uuid.hpp"

namespace aos {
namespace pkcs11 {

/**
 * Maximum length of PKCS11 slot description.
 */
constexpr auto cSlotDescriptionLen = AOS_CONFIG_PKCS11_SLOT_DESCRIPTION_LEN;

/**
 * Maximum length of PKCS11 manufacture ID.
 */
constexpr auto cManufacturerIDLen = AOS_CONFIG_PKCS11_MANUFACTURE_ID_LEN;

/**
 * Maximum length of PKCS11 token label.
 */
constexpr auto cLabelLen = AOS_CONFIG_PKCS11_LABEL_LEN;

/**
 * Library description length.
 */
constexpr auto cLibraryDescLen = AOS_CONFIG_PKCS11_LIBRARY_DESC_LEN;

/**
 * HSM device model length.
 */
constexpr auto cModelLen = AOS_CONFIG_PKCS11_MODEL_LEN;

/**
 * Maximum length of user PIN (password).
 */
constexpr auto cPINLength = AOS_CONFIG_PKCS11_PIN_LEN;

/**
 * Maximum number of open sessions per PKCS11 library.
 */
constexpr auto cSessionsPerLib = AOS_CONFIG_PKCS11_SESSIONS_PER_LIB;

/**
 * Maximum number of PKCS11 libraries.
 */
constexpr auto cLibrariesMaxNum = AOS_CONFIG_PKCS11_MAX_NUM_LIBRARIES;

/**
 * Maximum number of attributes for object.
 */
constexpr auto cObjectAttributesCount = AOS_CONFIG_PKCS11_OBJECT_ATTRIBUTES_COUNT;

/**
 * Maximum number of keys per token.
 */
constexpr auto cKeysPerToken = AOS_CONFIG_PKCS11_TOKEN_KEYS_COUNT;

/**
 * Maximum number of slots per PKCS11 library.
 */
constexpr auto cSlotListSize = AOS_CONFIG_PKCS11_SLOT_LIST_SIZE;

/**
 * The types of Cryptoki users.
 */
using UserType = CK_USER_TYPE;

/**
 * Cryptoki state type.
 */
using State = CK_STATE;

/**
 * Cryptoki flags type.
 */
using Flags = CK_FLAGS;

/**
 * A Cryptoki-assigned value that identifies a session.
 */
using SessionHandle = CK_SESSION_HANDLE;

/**
 * A Cryptoki-assigned value that identifies an attribute type.
 */
using AttributeType = CK_ATTRIBUTE_TYPE;

/**
 * Cryptoki object handles.
 */
using ObjectHandle = CK_OBJECT_HANDLE;

/**
 * Cryptoki object class.
 */
using ObjectClass = CK_OBJECT_CLASS;

/**
 * A Cryptoki-assigned value that identifies a slot.
 */
using SlotID = CK_SLOT_ID;

/**
 * Generates random unique PIN.
 *
 * @param[out] pin result pin.
 * @return Error.
 */
Error GenPIN(String& pin);

/**
 * Any version information related to PKCS11 library.
 */
struct Version {
    uint8_t mMajor;
    uint8_t mMinor;

    /**
     * Prints object to log.
     *
     * @param log log to output.
     * @param version object instance.
     * @return Log&.
     */
    friend Log& operator<<(Log& log, const Version& version)
    {
        log << static_cast<int>(version.mMajor) << "." << static_cast<int>(version.mMinor);
        return log;
    }
};

/**
 * Information about Cryptoki library.
 * https://www.cryptsoft.com/pkcs11doc/v200/structCK__INFO.html
 */
struct LibInfo {
    /**
     * Cryptoki interface version number.
     */
    Version mCryptokiVersion;
    /**
     * Cryptoki library version number.
     */
    Version mLibraryVersion;
    /**
     * ID of the Cryptoki library manufacturer.
     */
    StaticString<cManufacturerIDLen> mManufacturerID;
    /**
     * Description of the library.
     */
    StaticString<cLibraryDescLen> mLibraryDescription;

    /**
     * Prints object to log.
     *
     * @param log log to output.
     * @param libInfo object instance.
     * @return Log&.
     */
    friend Log& operator<<(Log& log, const LibInfo& libInfo)
    {
        log << "{cryptokiVersion = " << libInfo.mCryptokiVersion << ", libraryVersion = " << libInfo.mLibraryVersion
            << ", manufacturer = " << libInfo.mManufacturerID << ", description = " << libInfo.mLibraryDescription
            << "}";
        return log;
    }
};

/**
 * Information about PKCS11 slot info.
 * https://www.cryptsoft.com/pkcs11doc/v210/group__SEC__9__2__SLOT__AND__TOKEN__TYPES.html
 */
struct SlotInfo {
    /**
     * ID of the device manufacturer.
     */
    StaticString<cManufacturerIDLen> mManufacturerID;
    /**
     * Description of the slot.
     */
    StaticString<cSlotDescriptionLen> mSlotDescription;
    /**
     * Bit flags that provide capabilities of the slot.
     */
    Flags mFlags;
    /**
     * Version of the slot's hardware.
     */
    Version mHardwareVersion;
    /**
     * Version of the slot's firmware.
     */
    Version mFirmwareVersion;

    /**
     * Prints object to log.
     *
     * @param log log to output.
     * @param slotInfo object instance.
     * @return Log&.
     */
    friend Log& operator<<(Log& log, const SlotInfo& slotInfo)
    {
        log << "{manufacturer = " << slotInfo.mManufacturerID << ", description = " << slotInfo.mSlotDescription
            << ", hwVersion = " << slotInfo.mHardwareVersion << ", fwVersion = " << slotInfo.mFirmwareVersion
            << ", flags = " << slotInfo.mFlags << "}";
        return log;
    }
};

/**
 * Information about session.
 */
using SessionInfo = CK_SESSION_INFO;

/**
 * Information about PKCS11 token.
 */
struct TokenInfo {
    /**
     * Application-defined label, assigned during token initialization.
     */
    StaticString<cLabelLen> mLabel;
    /**
     * ID of the device manufacturer.
     */
    StaticString<cManufacturerIDLen> mManufacturerID;
    /**
     * HSM device model.
     */
    StaticString<cModelLen> mModel;
    /**
     * HSM device serial number.
     */
    StaticString<crypto::cSerialNumStrLen> mSerialNumber;
    /**
     * Bit flags indicating capabilities and status of the device.
     */
    Flags mFlags;
    /**
     * Version number of hardware.
     */
    Version mHardwareVersion;
    /**
     * Version number of firmware.
     */
    Version mFirmwareVersion;
    /**
     * The total amount of memory on the token in bytes in which public objects may be stored.
     */
    size_t mTotalPublicMemory;
    /**
     * The amount of free (unused) memory on the token in bytes for public objects.
     */
    size_t mFreePublicMemory;
    /**
     * The total amount of memory on the token in bytes in which private objects may be stored.
     */
    size_t mTotalPrivateMemory;
    /**
     * The amount of free (unused) memory on the token in bytes for private objects.
     */
    size_t mFreePrivateMemory;

    /**
     * Prints object to log.
     *
     * @param log log to output.
     * @param tokenInfo object instance.
     * @return Log&.
     */
    friend Log& operator<<(Log& log, const TokenInfo& tokenInfo)
    {
        log << "{label = " << tokenInfo.mLabel << ", manufacturer = " << tokenInfo.mManufacturerID
            << ", model = " << tokenInfo.mModel << ", serial = " << tokenInfo.mSerialNumber
            << ", hwVersion = " << tokenInfo.mHardwareVersion << ", fwVersion = " << tokenInfo.mFirmwareVersion
            << ", publicMemory = " << tokenInfo.mTotalPublicMemory - tokenInfo.mFreePublicMemory << "/"
            << tokenInfo.mTotalPublicMemory
            << ", privateMemory = " << tokenInfo.mTotalPrivateMemory - tokenInfo.mFreePrivateMemory << "/"
            << tokenInfo.mTotalPrivateMemory << ", flags = " << tokenInfo.mFlags << "}";
        return log;
    }
};

/**
 * Object attribute.
 */
struct ObjectAttribute {

    /**
     * Object constructor.
     */
    ObjectAttribute(AttributeType type, const Array<uint8_t>& value)
        : mType(type)
        , mValue(value)
    {
    }

    /**
     * Attribute type.
     */
    AttributeType mType;
    /**
     * Attribute value.
     */
    Array<uint8_t> mValue;
};

/**
 * Encapsulates session-related routines of Cryptoki API.
 */
class SessionContext {
public:
    /**
     * Constructs object instance.
     *
     * @param handle session handle.
     * @param funcList function list.
     */
    SessionContext(SessionHandle handle, CK_FUNCTION_LIST_PTR funcList);

    /**
     * Returns session information.
     *
     * @param[out] info result information.
     * @return Error.
     */
    Error GetSessionInfo(SessionInfo& info) const;

    /**
     * Logs a user into a token.
     *
     * @param userType type of Cryptoki user.
     * @param pin user’s PIN.
     * @return Error.
     */
    Error Login(UserType userType, const String& pin);

    /**
     * Logs a user out from a token.
     *
     * @return Error.
     */
    Error Logout();

    /**
     * Initializes the normal user's PIN.
     *
     * @param pin user pin.
     * @return Error.
     */
    Error InitPIN(const String& pin);

    /**
     * Obtains an attribute values of the specified object.
     *
     * @param handle object handle.
     * @param types types of attributes to be returned.
     * @param[out] values result attribute values.
     * @return Error.
     */
    Error GetAttributeValues(ObjectHandle handle, const Array<AttributeType>& types, Array<Array<uint8_t>>& values);

    /**
     * Searches for token and session objects that match a template.
     *
     * @param templ template for search operation.
     * @param[out] objects result object handles.
     * @return Error.
     */
    Error FindObjects(const Array<ObjectAttribute>& templ, Array<ObjectHandle>& objects);

    /**
     * Creates a new object.
     *
     * @param templ object’s template.
     * @return RetWithError<ObjectHandle>.
     */
    RetWithError<ObjectHandle> CreateObject(const Array<ObjectAttribute>& templ);

    /**
     * Destroys object by its handle.
     *
     * @param object handle of the object to be destroyed.
     * @return Error.
     */
    Error DestroyObject(ObjectHandle object);

    /**
     * Signs data.
     *
     * @param mechanism mechanism used to sign.
     * @param privKey the handle of the private key.
     * @param data data to be signed(may require hashing before being signed).
     * @param signature result signature.
     * @return Error.
     */
    Error Sign(CK_MECHANISM_PTR mechanism, ObjectHandle privKey, const Array<uint8_t>& data, Array<uint8_t>& signature);

    /**
     * Decrypts input data.
     *
     * @param mechanism mechanism used to sign.
     * @param privKey the handle of the private key.
     * @param data encrypted data.
     * @param result decrypted data.
     * @return Error.
     */
    Error Decrypt(CK_MECHANISM_PTR mechanism, ObjectHandle privKey, const Array<uint8_t>& data, Array<uint8_t>& result);

    /**
     * Returns session handle.
     *
     * @return session handle.
     */
    SessionHandle GetHandle() const;

    /**
     * Returns function list.
     *
     * @return CK_FUNCTION_LIST_PTR.
     */
    CK_FUNCTION_LIST_PTR GetFunctionList() const;

    /**
     * Destroy object instance.
     */
    ~SessionContext();

private:
    Error SignInit(CK_MECHANISM_PTR mechanism, ObjectHandle privKey);
    Error Sign(const Array<uint8_t>& data, CK_BYTE_PTR signature, CK_ULONG_PTR signSize);

    Error DecryptInit(CK_MECHANISM_PTR mechanism, ObjectHandle privKey);
    Error Decrypt(const Array<uint8_t>& data, CK_BYTE_PTR result, CK_ULONG_PTR resultSize);

    Error FindObjectsInit(const Array<ObjectAttribute>& templ);
    Error FindObjects(Array<ObjectHandle>& objects);
    Error FindObjectsFinal();

    SessionHandle        mHandle;
    CK_FUNCTION_LIST_PTR mFunctionList;
};

#if AOS_CONFIG_PKCS11_USE_STATIC_LIB

/**
 * Static PKCS11 library context.
 */
class StaticLibraryContext {
protected:
    /**
     * Initializes object instance.
     *
     * @return Error.
     */
    RetWithError<CK_FUNCTION_LIST_PTR> Init();
};

/**
 * An alias for the concrete PKCS11 library context.
 */
using PKCS11LibraryContext = StaticLibraryContext;

#else

/**
 * Dynamic PKCS11 library context.
 */
class DynamicLibraryContext {
public:
    /**
     * Sets dynamic library handle.
     *
     * @param handle dynamic library handle.
     */
    void SetHandle(void* handle);

    /**
     * Destroys object instance.
     */
    ~DynamicLibraryContext();

protected:
    /**
     * Initializes object instance.
     *
     * @param functionList PKCS11 function list.
     * @return Error.
     */
    RetWithError<CK_FUNCTION_LIST_PTR> Init();

private:
    void* mHandle = nullptr;
};

/**
 * An alias for the concrete PKCS11 library context.
 */
using PKCS11LibraryContext = DynamicLibraryContext;

#endif

/**
 * PKCS11 Context instance.
 */
class LibraryContext : public PKCS11LibraryContext {
public:
    /**
     * Initializes object instance.
     *
     * @return Error.
     */
    virtual Error Init();

    /**
     * Initializes a token.
     *
     * @param slotID slot identifier.
     * @param pin user pin.
     * @param label token label.
     * @return Error.
     */
    Error InitToken(SlotID slotID, const String& pin, const String& label);

    /**
     * Returns list of slots in the system.
     *
     * @param tokenPresent indicates whether the list obtained includes only those slots with a token present (CK_TRUE),
     * or all slots (CK_FALSE);
     * @param[out] slotList result slot list.
     * @return Error.
     */
    Error GetSlotList(bool tokenPresent, Array<SlotID>& slotList) const;

    /**
     * Returns information about slot.
     *
     * @param slotID slot id.
     * @param[out] slotInfo result slot information.
     * @return Error.
     */
    Error GetSlotInfo(SlotID slotID, SlotInfo& slotInfo) const;

    /**
     * Returns token information for the specified slot id.
     *
     * @param slotID slot id.
     * @param[out] tokenInfo token information for the slot.
     * @return Error.
     */
    Error GetTokenInfo(SlotID slotID, TokenInfo& tokenInfo) const;

    /**
     * Returns general information about Cryptoki library.
     *
     * @param[out] libInfo result information.
     * @return Error.
     */
    Error GetLibInfo(LibInfo& libInfo) const;

    /**
     * Opens a session between an application and a token in a particular slot.
     *
     * @param slotID slot identifier.
     * @param flags  indicates the type of session.
     * @return Error.
     */
    RetWithError<UniquePtr<SessionContext>> OpenSession(SlotID slotID, uint32_t flags);

    /**
     * Destroys object instance.
     */
    ~LibraryContext();

private:
    CK_FUNCTION_LIST_PTR                                      mFunctionList;
    StaticAllocator<sizeof(SessionContext) * cSessionsPerLib> mAllocator;
};

/**
 * A Curve represents a short-form Weierstrass curve.
 */
enum class EllipticCurve {
    /**
     * A Curve which implements NIST P-384 (FIPS 186-3, section D.2.4), also known as secp384r1
     **/
    eP384
};

/**
 * Crypto private key extended with object handles.
 */
class PrivateKey {
public:
    /**
     * Constructs object instance with not initialized key part.
     *
     * @param privHandle private key handle.
     * @param pubHandle public key handle.
     */
    PrivateKey(
        ObjectHandle privHandle = 0, ObjectHandle pubHandle = 0, SharedPtr<crypto::PrivateKeyItf> privKey = nullptr)
        : mPrivHandle(privHandle)
        , mPubHandle(pubHandle)
        , mPrivKey(privKey)
    {
    }

    /**
     * PKCS11 priv key handle.
     */
    ObjectHandle GetPrivHandle() const { return mPrivHandle; }

    /**
     * PKCS11 pub key handle.
     */
    ObjectHandle GetPubHandle() const { return mPubHandle; }

    /**
     * Returns pointer to the crypto private key interface.
     */
    SharedPtr<crypto::PrivateKeyItf> GetPrivKey() { return mPrivKey; }

private:
    ObjectHandle                     mPrivHandle, mPubHandle;
    SharedPtr<crypto::PrivateKeyItf> mPrivKey;
};

/**
 * A helper class for a base Cryptoki API.
 */
class Utils {
public:
    /**
     * Creates an object instance.
     *
     * @param session session context.
     * @param cryptoProvider provider to x509 crypto provider.
     * @param allocator allocator for token/session objects.
     */
    Utils(SessionContext& session, crypto::x509::ProviderItf& cryptoProvider, Allocator& allocator);

    /**
     * Creates an RSA key pair on the token.
     *
     * @param id key pair id.
     * @param label key pair label.
     * @param bitsCount key size.
     * @return RetWithError<PrivateKey>.
     */
    RetWithError<PrivateKey> GenerateRSAKeyPairWithLabel(
        const Array<uint8_t>& id, const String& label, size_t bitsCount);

    /**
     * Creates a ECDSA key pair on the token using curve.
     *
     * @param id token id.
     * @param label label.
     * @param curve elliptic curve.
     * @return RetWithError<PrivateKey>.
     */
    RetWithError<PrivateKey> GenerateECDSAKeyPairWithLabel(
        const Array<uint8_t>& id, const String& label, EllipticCurve curve);

    /**
     * Retrieves a previously created asymmetric key pair.
     *
     * @param id key id.
     * @param label key label.
     * @return RetWithError<PrivateKey>.
     */
    RetWithError<PrivateKey> FindPrivateKey(const Array<uint8_t>& id, const String& label);

    /**
     * Deletes private key for the token.
     *
     * @param key private key to be deleted.
     * @return Error.
     */
    Error DeletePrivateKey(const PrivateKey& key);

    /**
     * Imports a certificate onto the token.
     *
     * @param id certificate id.
     * @param label certificate label.
     * @param cert certificate.
     * @return Error.
     */
    Error ImportCertificate(const Array<uint8_t>& id, const String& label, const crypto::x509::Certificate& cert);

    /**
     * Checks whether certificate with specified attributes has been imported.
     *
     * @param issuer certificate issuer.
     * @param serialNumber certificate serialNumber.
     * @return Error.
     */
    RetWithError<bool> HasCertificate(const Array<uint8_t>& issuer, const Array<uint8_t>& serialNumber);

    /**
     * Finds certificate chain with a given attributes.
     *
     * @param id certificate identifier.
     * @param label certificate label.
     * @return RetWithError<SharedPtr<CertificateChain>>.
     */
    RetWithError<SharedPtr<crypto::x509::CertificateChain>> FindCertificateChain(
        const Array<uint8_t>& id, const String& label);

    /**
     * Deletes a previously imported certificate.
     *
     * @param id certificate id.
     * @param label certificate label.
     * @return Error.
     */
    Error DeleteCertificate(const Array<uint8_t>& id, const String& label);

    /**
     * Converts PKCS11 byte array to string.
     *
     * @param src source byte array.
     * @param[out] dst destination string.
     * @return Error.
     */
    static Error ConvertPKCS11String(const Array<uint8_t>& src, String& dst);

private:
    RetWithError<PrivateKey> ExportPrivateKey(ObjectHandle privKey, ObjectHandle pubKey, CK_KEY_TYPE keyType);

    Error FindCertificates(const Array<uint8_t>& id, const String& label, Array<ObjectHandle>& handles);
    Error FindCertificateChain(const crypto::x509::Certificate& certificate, crypto::x509::CertificateChain& chain);
    RetWithError<SharedPtr<crypto::x509::Certificate>> FindCertificateByKeyID(const Array<uint8_t>& keyID);

    RetWithError<SharedPtr<crypto::x509::Certificate>> GetCertificate(ObjectHandle handle);

    SessionContext&            mSession;
    crypto::x509::ProviderItf& mCryptoProvider;
    Allocator&                 mAllocator;
};

/**
 * Manages PKCS11 libraries.
 */
class PKCS11Manager {
public:
    /**
     * Opens PKCS11 library.
     *
     * @param library path to pkcs11 library.
     * @return SharedPtr<LibraryContext>
     */
    SharedPtr<LibraryContext> OpenLibrary(const String& library);

private:
    using LibraryInfo = Pair<StaticString<cFilePathLen>, SharedPtr<LibraryContext>>;

    StaticAllocator<sizeof(LibraryContext) * cLibrariesMaxNum> mAllocator;
    StaticArray<LibraryInfo, cLibrariesMaxNum>                 mLibraries;
};

} // namespace pkcs11
} // namespace aos

#endif
