/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <time.h>

#if !AOS_CONFIG_PKCS11_USE_STATIC_LIB
#include <dlfcn.h>
#endif

#include "aos/common/pkcs11/pkcs11.hpp"
#include "aos/common/pkcs11/privatekey.hpp"

#include "log.hpp"

namespace aos {
namespace pkcs11 {

/***********************************************************************************************************************
 * Helpers
 **********************************************************************************************************************/

Error ConvertFromPKCS11String(const Array<uint8_t>& src, String& dst)
{
    dst.Clear();

    if (src.IsEmpty()) {
        return ErrorEnum::eNone;
    }

    int size = src.Size();

    if (!dst.Resize(size).IsNone()) {
        return ErrorEnum::eNoMemory;
    }

    memcpy(dst.Get(), src.Get(), size);

    // Trim string
    for (int i = size - 1; i >= 0; --i) {
        if (dst[i] == ' ')
            --size;
        else
            break;
    }

    dst.Resize(size);

    return ErrorEnum::eNone;
}

template <size_t cSize>
Error ConvertFromPKCS11String(const CK_UTF8CHAR (&src)[cSize], String& dst)
{
    return ConvertFromPKCS11String(Array<uint8_t>(src, cSize), dst);
}

template <size_t cSize>
Error ConvertToPKCS11String(const String& src, CK_UTF8CHAR (&dst)[cSize])
{
    if (cSize <= src.Size()) {
        return ErrorEnum::eNoMemory;
    }

    memset(dst, ' ', cSize);
    memcpy(dst, src.CStr(), src.Size());

    return ErrorEnum::eNone;
}

void ConvertFromPKCS11Version(const CK_VERSION& src, Version& dst)
{
    dst.mMajor = src.major;
    dst.mMinor = src.minor;
}

CK_UTF8CHAR_PTR ConvertToPKCS11UTF8CHARPTR(const char* src)
{
    return reinterpret_cast<CK_UTF8CHAR_PTR>(const_cast<char*>(src));
}

Error ConvertFromPKCS11SlotInfo(const CK_SLOT_INFO& src, SlotInfo& dst)
{
    Error err = ConvertFromPKCS11String(src.manufacturerID, dst.mManufacturerID);
    if (!err.IsNone()) {
        return err;
    }

    err = ConvertFromPKCS11String(src.slotDescription, dst.mSlotDescription);
    if (!err.IsNone()) {
        return err;
    }

    ConvertFromPKCS11Version(src.hardwareVersion, dst.mHardwareVersion);
    ConvertFromPKCS11Version(src.firmwareVersion, dst.mFirmwareVersion);

    dst.mFlags = src.flags;

    return ErrorEnum::eNone;
}

Error ConvertFromPKCS11TokenInfo(const CK_TOKEN_INFO& src, TokenInfo& dst)
{
    Error err = ConvertFromPKCS11String(src.label, dst.mLabel);
    if (!err.IsNone()) {
        return err;
    }

    err = ConvertFromPKCS11String(src.manufacturerID, dst.mManufacturerID);
    if (!err.IsNone()) {
        return err;
    }

    err = ConvertFromPKCS11String(src.model, dst.mModel);
    if (!err.IsNone()) {
        return err;
    }

    err = ConvertFromPKCS11String(src.serialNumber, dst.mSerialNumber);
    if (!err.IsNone()) {
        return err;
    }

    dst.mFlags = src.flags;
    ConvertFromPKCS11Version(src.hardwareVersion, dst.mHardwareVersion);
    ConvertFromPKCS11Version(src.firmwareVersion, dst.mFirmwareVersion);
    dst.mTotalPublicMemory  = src.ulTotalPublicMemory;
    dst.mFreePublicMemory   = src.ulFreePublicMemory;
    dst.mTotalPrivateMemory = src.ulTotalPrivateMemory;
    dst.mFreePrivateMemory  = src.ulFreePrivateMemory;

    return ErrorEnum::eNone;
}

Error ConvertFromPKCS11LibInfo(const CK_INFO& src, LibInfo& dst)
{
    ConvertFromPKCS11Version(src.cryptokiVersion, dst.mCryptokiVersion);
    ConvertFromPKCS11Version(src.libraryVersion, dst.mLibraryVersion);

    Error err = ConvertFromPKCS11String(src.manufacturerID, dst.mManufacturerID);
    if (!err.IsNone()) {
        return err;
    }

    err = ConvertFromPKCS11String(src.libraryDescription, dst.mLibraryDescription);
    if (!err.IsNone()) {
        return err;
    }

    return ErrorEnum::eNone;
}

Error ConvertToPKCS11Attributes(const Array<ObjectAttribute>& src, Array<CK_ATTRIBUTE>& dst)
{
    if (dst.MaxSize() < src.Size()) {
        return ErrorEnum::eNoMemory;
    }

    dst.Clear();

    for (const auto& attr : src) {
        CK_ATTRIBUTE tmp;

        tmp.type       = attr.mType;
        tmp.pValue     = const_cast<uint8_t*>(attr.mValue.Get());
        tmp.ulValueLen = attr.mValue.Size();

        dst.PushBack(tmp);
    }

    return ErrorEnum::eNone;
}

Error BuildAttributes(const Array<AttributeType>& types, Array<Array<uint8_t>>& values, Array<CK_ATTRIBUTE>& dst)
{
    // number of types and values should be the same in order to be combined into attributes array.
    if (types.Size() != values.Size()) {
        return ErrorEnum::eInvalidArgument;
    }

    if (dst.MaxSize() < types.Size()) {
        return ErrorEnum::eNoMemory;
    }

    dst.Clear();

    for (size_t i = 0; i < types.Size(); ++i) {
        CK_ATTRIBUTE tmp;

        tmp.type       = types[i];
        tmp.pValue     = static_cast<void*>(values[i].Get());
        tmp.ulValueLen = values[i].MaxSize();

        dst.PushBack(tmp);
    }

    return ErrorEnum::eNone;
}

Error GetAttributesValues(Array<CK_ATTRIBUTE>& src, Array<Array<uint8_t>>& values)
{
    if (values.MaxSize() < src.Size()) {
        return ErrorEnum::eNoMemory;
    }

    values.Clear();

    for (const auto& attr : src) {
        if (attr.ulValueLen == CK_UNAVAILABLE_INFORMATION) {
            return ErrorEnum::eInvalidArgument;
        }

        auto tmp = Array<uint8_t>(static_cast<uint8_t*>(attr.pValue), attr.ulValueLen);
        values.PushBack(tmp);
    }

    return ErrorEnum::eNone;
}

Array<uint8_t> ConvertToAttributeValue(const String& val)
{
    return {const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(val.Get())), val.Size()};
}

template <typename T>
Array<uint8_t> ConvertToAttributeValue(T& val)
{
    return {reinterpret_cast<uint8_t*>(&val), sizeof(val)};
}

/***********************************************************************************************************************
 * GenPIN
 **********************************************************************************************************************/

Error GenPIN(String& pin)
{
    if (pin.MaxSize() == 0) {
        return ErrorEnum::eNone;
    }

    pin.Clear();

    srand(::time(nullptr)); // use current time as seed for random generator

    StaticString<sizeof(unsigned) * 2> chunk;

    while (pin.Size() < pin.MaxSize()) {
        unsigned value     = rand();
        auto     byteArray = Array<uint8_t>(reinterpret_cast<uint8_t*>(&value), sizeof(value));

        auto err = chunk.ByteArrayToHex(byteArray);
        if (!err.IsNone()) {
            return err;
        }

        auto chunkSize = Min(pin.MaxSize() - pin.Size(), chunk.Size());

        pin.Insert(pin.end(), chunk.begin(), chunk.begin() + chunkSize);
    }

    return ErrorEnum::eNone;
}

#if AOS_CONFIG_PKCS11_USE_STATIC_LIB

/***********************************************************************************************************************
 * StaticLibraryContext
 **********************************************************************************************************************/

RetWithError<CK_FUNCTION_LIST_PTR> StaticLibraryContext::Init()
{
    CK_FUNCTION_LIST_PTR functionList = nullptr;

    CK_RV rv = C_GetFunctionList(&functionList);
    if (rv != CKR_OK) {
        LOG_ERR() << "Get function list failed: err = " << rv;

        return {nullptr, static_cast<int>(rv)};
    }

    return {functionList, ErrorEnum::eNone};
}

#else

/***********************************************************************************************************************
 * DynamicLibraryContext
 **********************************************************************************************************************/

void DynamicLibraryContext::SetHandle(void* handle)
{
    mHandle = handle;
}

DynamicLibraryContext::~DynamicLibraryContext()
{
    if (dlclose(mHandle) != 0) {
        LOG_ERR() << "PKCS11 library close failed: error = " << dlerror();
    }
}

RetWithError<CK_FUNCTION_LIST_PTR> DynamicLibraryContext::Init()
{
    CK_C_GetFunctionList getFuncList = reinterpret_cast<CK_C_GetFunctionList>(dlsym(mHandle, "C_GetFunctionList"));

    if (getFuncList == nullptr) {
        LOG_ERR() << "Can't find get function list function: dlsym err = " << dlerror();

        return {nullptr, ErrorEnum::eFailed};
    }

    CK_FUNCTION_LIST_PTR functionList = nullptr;

    CK_RV rv = getFuncList(&functionList);
    if (rv != CKR_OK) {
        LOG_ERR() << "Get function list failed: err = " << rv;

        return {nullptr, static_cast<int>(rv)};
    }

    return {functionList, ErrorEnum::eNone};
}

#endif

/***********************************************************************************************************************
 * LibraryContext
 **********************************************************************************************************************/

Error LibraryContext::Init()
{
    LockGuard lock(mMutex);

    Error err = ErrorEnum::eNone;

    Tie(mFunctionList, err) = PKCS11LibraryContext::Init();
    if (!err.IsNone()) {
        return err;
    }

    if (mFunctionList == nullptr) {
        LOG_ERR() << "Function list is not set";

        return ErrorEnum::eWrongState;
    }

    auto rv = mFunctionList->C_Initialize(nullptr);
    if (rv != CKR_OK) {
        LOG_ERR() << "Initialize library failed: err = " << rv;

        return static_cast<int>(rv);
    }

    return ErrorEnum::eNone;
}

Error LibraryContext::InitToken(SlotID slotID, const String& pin, const String& label)
{
    if (!mFunctionList || !mFunctionList->C_InitToken) {
        return ErrorEnum::eWrongState;
    }

    CK_UTF8CHAR pkcsLabel[cLabelLen];

    auto err = ConvertToPKCS11String(label, pkcsLabel);
    if (!err.IsNone()) {
        return err;
    }

    CK_RV rv = mFunctionList->C_InitToken(slotID, ConvertToPKCS11UTF8CHARPTR(pin.CStr()), pin.Size(), pkcsLabel);
    if (rv != CKR_OK) {
        return static_cast<int>(rv);
    }

    return ErrorEnum::eNone;
}

Error LibraryContext::GetSlotList(bool tokenPresent, Array<SlotID>& slotList) const
{
    if (!mFunctionList || !mFunctionList->C_GetSlotList) {
        return ErrorEnum::eWrongState;
    }

    CK_ULONG count = 0;

    CK_RV rv = mFunctionList->C_GetSlotList(static_cast<CK_BBOOL>(tokenPresent), nullptr, &count);
    if (rv != CKR_OK) {
        return static_cast<int>(rv);
    }

    auto err = slotList.Resize(count);
    if (!err.IsNone()) {
        return err;
    }

    rv = mFunctionList->C_GetSlotList(static_cast<CK_BBOOL>(tokenPresent), slotList.Get(), &count);
    if (rv != CKR_OK) {
        return static_cast<int>(rv);
    }

    return ErrorEnum::eNone;
}

Error LibraryContext::GetSlotInfo(SlotID slotID, SlotInfo& slotInfo) const
{
    if (!mFunctionList || !mFunctionList->C_GetSlotInfo) {
        return ErrorEnum::eWrongState;
    }

    CK_SLOT_INFO pkcsInfo;

    CK_RV rv = mFunctionList->C_GetSlotInfo(slotID, &pkcsInfo);
    if (rv != CKR_OK) {
        return static_cast<int>(rv);
    }

    return ConvertFromPKCS11SlotInfo(pkcsInfo, slotInfo);
}

Error LibraryContext::GetTokenInfo(SlotID slotID, TokenInfo& tokenInfo) const
{
    if (!mFunctionList || !mFunctionList->C_GetTokenInfo) {
        return ErrorEnum::eWrongState;
    }

    CK_TOKEN_INFO pkcsInfo;

    CK_RV rv = mFunctionList->C_GetTokenInfo(slotID, &pkcsInfo);
    if (rv != CKR_OK) {
        return static_cast<int>(rv);
    }

    return ConvertFromPKCS11TokenInfo(pkcsInfo, tokenInfo);
}

Error LibraryContext::GetLibInfo(LibInfo& libInfo) const
{
    if (!mFunctionList || !mFunctionList->C_GetInfo) {
        return ErrorEnum::eWrongState;
    }

    CK_INFO pkcsInfo;

    CK_RV rv = mFunctionList->C_GetInfo(&pkcsInfo);
    if (rv != CKR_OK) {
        return static_cast<int>(rv);
    }

    return ConvertFromPKCS11LibInfo(pkcsInfo, libInfo);
}

RetWithError<SharedPtr<SessionContext>> LibraryContext::OpenSession(SlotID slotID, Flags flags)
{
    LockGuard lock(mMutex);

    SessionParams params = {slotID, flags};

    for (auto& val : mSessions) {
        if (val.mFirst == params) {
            return {val.mSecond, ErrorEnum::eNone};
        }
    }

    Error                     err = ErrorEnum::eNone;
    SharedPtr<SessionContext> session;

    Tie(session, err) = PKCS11OpenSession(slotID, flags);
    if (!err.IsNone()) {
        return {session, err};
    }

    if (mSessions.Size() != mSessions.MaxSize()) {
        mSessions.PushBack({params, session});

        return {session, ErrorEnum::eNone};
    }

    mSessions[mLRUInd] = Pair<SessionParams, SharedPtr<SessionContext>>(params, session);

    mLRUInd++;
    if (mLRUInd == mSessions.Size()) {
        mLRUInd = 0;
    }

    return {session, ErrorEnum::eNone};
}

void LibraryContext::ClearSessions()
{
    LockGuard lock(mMutex);

    mSessions.Clear();
}

Error LibraryContext::CloseAllSessions(SlotID slotID)
{
    if (!mFunctionList || !mFunctionList->C_CloseAllSessions) {
        return ErrorEnum::eWrongState;
    }

    CK_RV rv = mFunctionList->C_CloseAllSessions(slotID);
    if (rv != CKR_OK) {
        return static_cast<int>(rv);
    }

    return ErrorEnum::eNone;
}

LibraryContext::~LibraryContext()
{
    if (!mFunctionList || !mFunctionList->C_Finalize) {
        LOG_ERR() << "Finalize library failed: library is not initialized";
    }

    ClearSessions();

    CK_RV rv = mFunctionList->C_Finalize(nullptr);
    if (rv != CKR_OK) {
        LOG_ERR() << "Finalize library failed: err = " << rv;
    }
}

RetWithError<SharedPtr<SessionContext>> LibraryContext::PKCS11OpenSession(SlotID slotID, Flags flags)
{
    if (!mFunctionList || !mFunctionList->C_OpenSession) {
        return {nullptr, ErrorEnum::eWrongState};
    }

    SessionHandle handle;

    CK_RV rv = mFunctionList->C_OpenSession(slotID, flags, nullptr, nullptr, &handle);
    if (rv != CKR_OK) {
        return {nullptr, static_cast<int>(rv)};
    }

    auto session = MakeShared<SessionContext>(&mAllocator, handle, mFunctionList);

    return {session, ErrorEnum::eNone};
}

/***********************************************************************************************************************
 * SessionContext
 **********************************************************************************************************************/

SessionContext::SessionContext(SessionHandle handle, CK_FUNCTION_LIST_PTR funcList)
    : mHandle(handle)
    , mFunctionList(funcList)
{
}

Error SessionContext::GetSessionInfo(SessionInfo& info) const
{
    if (!mFunctionList || !mFunctionList->C_GetSessionInfo) {
        return ErrorEnum::eWrongState;
    }

    CK_RV rv = mFunctionList->C_GetSessionInfo(mHandle, &info);
    if (rv != CKR_OK) {
        return static_cast<int>(rv);
    }

    return ErrorEnum::eNone;
}

Error SessionContext::Login(UserType userType, const String& pin)
{
    if (!mFunctionList || !mFunctionList->C_Login) {
        return ErrorEnum::eWrongState;
    }

    CK_RV rv = mFunctionList->C_Login(mHandle, userType, ConvertToPKCS11UTF8CHARPTR(pin.CStr()), pin.Size());
    if (rv == CKR_USER_ALREADY_LOGGED_IN) {
        return ErrorEnum::eAlreadyLoggedIn;
    }

    if (rv != CKR_OK) {
        return static_cast<int>(rv);
    }

    return ErrorEnum::eNone;
}

Error SessionContext::Logout()
{
    if (!mFunctionList || !mFunctionList->C_Logout) {
        return ErrorEnum::eWrongState;
    }

    CK_RV rv = mFunctionList->C_Logout(mHandle);
    if (rv != CKR_OK) {
        return static_cast<int>(rv);
    }

    return ErrorEnum::eNone;
}

Error SessionContext::InitPIN(const String& pin)
{
    if (!mFunctionList || !mFunctionList->C_InitPIN) {
        return ErrorEnum::eWrongState;
    }

    CK_RV rv = mFunctionList->C_InitPIN(mHandle, ConvertToPKCS11UTF8CHARPTR(pin.CStr()), pin.Size());
    if (rv != CKR_OK) {
        return static_cast<int>(rv);
    }

    return ErrorEnum::eNone;
}

Error SessionContext::GetAttributeValues(
    ObjectHandle object, const Array<AttributeType>& types, Array<Array<uint8_t>>& values) const
{
    if (!mFunctionList || !mFunctionList->C_GetAttributeValue) {
        return ErrorEnum::eWrongState;
    }

    StaticArray<CK_ATTRIBUTE, cObjectAttributesCount> pkcsAttributes;

    Error err = BuildAttributes(types, values, pkcsAttributes);
    if (!err.IsNone()) {
        return err;
    }

    CK_RV rv = mFunctionList->C_GetAttributeValue(mHandle, object, pkcsAttributes.Get(), pkcsAttributes.Size());
    if (rv != CKR_OK) {
        return static_cast<int>(rv);
    }

    return GetAttributesValues(pkcsAttributes, values);
}

Error SessionContext::FindObjects(const Array<ObjectAttribute>& templ, Array<ObjectHandle>& objects) const
{
    Error initErr = FindObjectsInit(templ);
    if (!initErr.IsNone()) {
        return initErr;
    }

    Error findErr    = FindObjects(objects);
    Error finalError = FindObjectsFinal();

    if (!findErr.IsNone()) {
        return findErr;
    }

    if (!finalError.IsNone()) {
        return finalError;
    }

    return ErrorEnum::eNone;
}

RetWithError<ObjectHandle> SessionContext::CreateObject(const Array<ObjectAttribute>& templ)
{
    if (!mFunctionList || !mFunctionList->C_CreateObject) {
        return {0, ErrorEnum::eWrongState};
    }

    StaticArray<CK_ATTRIBUTE, cObjectAttributesCount> pkcsTempl;

    Error err = ConvertToPKCS11Attributes(templ, pkcsTempl);
    if (!err.IsNone()) {
        return {0, err};
    }

    ObjectHandle objHandle = CK_INVALID_HANDLE;

    CK_RV rv = mFunctionList->C_CreateObject(mHandle, pkcsTempl.Get(), pkcsTempl.Size(), &objHandle);
    if (rv != CKR_OK) {
        return {0, static_cast<int>(rv)};
    }

    return {objHandle, ErrorEnum::eNone};
}

Error SessionContext::DestroyObject(ObjectHandle object)
{
    if (!mFunctionList || !mFunctionList->C_DestroyObject) {
        return ErrorEnum::eWrongState;
    }

    CK_RV rv = mFunctionList->C_DestroyObject(mHandle, object);
    if (rv != CKR_OK) {
        return static_cast<int>(rv);
    }

    return ErrorEnum::eNone;
}

Error SessionContext::Sign(
    CK_MECHANISM_PTR mechanism, ObjectHandle privKey, const Array<uint8_t>& data, Array<uint8_t>& signature) const
{
    auto err = SignInit(mechanism, privKey);
    if (!err.IsNone()) {
        return err;
    }

    CK_ULONG signSize = signature.MaxSize();

    signature.Resize(signature.MaxSize());

    err = Sign(data, signature.Get(), &signSize);
    if (!err.IsNone()) {
        return err;
    }

    return signature.Resize(signSize);
}

Error SessionContext::Decrypt(
    CK_MECHANISM_PTR mechanism, ObjectHandle privKey, const Array<uint8_t>& data, Array<uint8_t>& result) const
{
    auto err = DecryptInit(mechanism, privKey);
    if (!err.IsNone()) {
        return err;
    }

    // SoftHSM doesn't provide a precise size with Decrypt(data, nullptr, &resultSize) call.
    CK_ULONG resultSize = result.MaxSize();

    result.Resize(result.MaxSize());

    err = Decrypt(data, result.Get(), &resultSize);
    if (!err.IsNone()) {
        return err;
    }

    return result.Resize(resultSize);
}

SessionHandle SessionContext::GetHandle() const
{
    return mHandle;
}

CK_FUNCTION_LIST_PTR SessionContext::GetFunctionList() const
{
    return mFunctionList;
}

SessionContext::~SessionContext()
{
    if (!mFunctionList || !mFunctionList->C_CloseSession) {
        LOG_ERR() << "Close session failed: library is not initialized";

        return;
    }

    CK_RV rv = mFunctionList->C_CloseSession(mHandle);
    if (rv != CKR_OK) {
        LOG_ERR() << "Close session failed: error = " << rv;
    }
}

Error SessionContext::SignInit(CK_MECHANISM_PTR mechanism, ObjectHandle privKey) const
{
    if (!mFunctionList || !mFunctionList->C_SignInit) {
        return ErrorEnum::eWrongState;
    }

    CK_RV rv = mFunctionList->C_SignInit(mHandle, mechanism, privKey);
    if (rv != CKR_OK) {
        return static_cast<int>(rv);
    }

    return ErrorEnum::eNone;
}

Error SessionContext::Sign(const Array<uint8_t>& data, CK_BYTE_PTR signature, CK_ULONG_PTR signSize) const
{
    if (!mFunctionList || !mFunctionList->C_Sign) {
        return ErrorEnum::eWrongState;
    }

    CK_RV rv = mFunctionList->C_Sign(mHandle, const_cast<uint8_t*>(data.Get()), data.Size(), signature, signSize);
    if (rv != CKR_OK) {
        return static_cast<int>(rv);
    }

    return ErrorEnum::eNone;
}

Error SessionContext::DecryptInit(CK_MECHANISM_PTR mechanism, ObjectHandle privKey) const
{
    if (!mFunctionList || !mFunctionList->C_DecryptInit) {
        return ErrorEnum::eWrongState;
    }

    CK_RV rv = mFunctionList->C_DecryptInit(mHandle, mechanism, privKey);
    if (rv != CKR_OK) {
        return static_cast<int>(rv);
    }

    return ErrorEnum::eNone;
}

Error SessionContext::Decrypt(const Array<uint8_t>& data, CK_BYTE_PTR result, CK_ULONG_PTR resultSize) const
{
    if (!mFunctionList || !mFunctionList->C_Decrypt) {
        return ErrorEnum::eWrongState;
    }

    CK_RV rv = mFunctionList->C_Decrypt(mHandle, const_cast<uint8_t*>(data.Get()), data.Size(), result, resultSize);
    if (rv != CKR_OK) {
        return static_cast<int>(rv);
    }

    return ErrorEnum::eNone;
}

Error SessionContext::FindObjectsInit(const Array<ObjectAttribute>& templ) const
{
    if (!mFunctionList || !mFunctionList->C_FindObjectsInit) {
        return ErrorEnum::eWrongState;
    }

    StaticArray<CK_ATTRIBUTE, cObjectAttributesCount> pkcsTempl;

    Error err = ConvertToPKCS11Attributes(templ, pkcsTempl);
    if (!err.IsNone()) {
        return err;
    }

    CK_RV rv = mFunctionList->C_FindObjectsInit(mHandle, pkcsTempl.Get(), pkcsTempl.Size());
    if (rv != CKR_OK) {
        return static_cast<int>(rv);
    }

    return ErrorEnum::eNone;
}

Error SessionContext::FindObjects(Array<ObjectHandle>& objects) const
{
    if (!mFunctionList || !mFunctionList->C_FindObjects) {
        return ErrorEnum::eWrongState;
    }

    unsigned long foundObjectsCount = 0, chunk = 0;

    objects.Resize(objects.MaxSize());

    while (true) {
        if (foundObjectsCount == objects.MaxSize()) {
            return ErrorEnum::eNoMemory;
        }

        CK_RV rv = mFunctionList->C_FindObjects(
            mHandle, objects.begin() + foundObjectsCount, objects.MaxSize() - foundObjectsCount, &chunk);
        if (rv != CKR_OK) {
            return static_cast<int>(rv);
        }

        foundObjectsCount += chunk;

        // success only when we ensured that all objects found
        if (chunk == 0) {
            objects.Resize(foundObjectsCount);

            return objects.IsEmpty() ? ErrorEnum::eNotFound : ErrorEnum::eNone;
        }
    }

    return ErrorEnum::eFailed;
}

Error SessionContext::FindObjectsFinal() const
{
    if (!mFunctionList || !mFunctionList->C_FindObjectsFinal) {
        return ErrorEnum::eWrongState;
    }

    CK_RV rv = mFunctionList->C_FindObjectsFinal(mHandle);
    if (rv != CKR_OK) {
        return static_cast<int>(rv);
    }

    return ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * PKCS11Manager
 **********************************************************************************************************************/

SharedPtr<LibraryContext> PKCS11Manager::OpenLibrary(const String& library)
{
    LockGuard lock(mMutex);

    LOG_INF() << "Loading library: path = " << library;

    for (auto& lib : mLibraries) {
        if (lib.mFirst == library) {
            return lib.mSecond;
        }
    }

    if (mLibraries.MaxSize() == mLibraries.Size()) {
        return nullptr;
    }

    auto ctx = MakeShared<LibraryContext>(&mAllocator);
    if (!ctx) {
        return nullptr;
    }

#if !AOS_CONFIG_PKCS11_USE_STATIC_LIB

    dlerror(); // clean previous error status

    void* handle = dlopen(library.CStr(), RTLD_NOW);
    if (handle == nullptr) {
        LOG_ERR() << "Can't open PKCS11 library: path = " << library << ", err = " << dlerror();

        return nullptr;
    }

    ctx->SetHandle(handle);

#endif

    if (!ctx->Init().IsNone()) {
        return nullptr;
    }

    mLibraries.EmplaceBack(library, ctx);

    return ctx;
}

/***********************************************************************************************************************
 * Utils
 **********************************************************************************************************************/

Utils::Utils(const SharedPtr<SessionContext>& session, crypto::x509::ProviderItf& cryptoProvider, Allocator& allocator)
    : mSession(session)
    , mCryptoProvider(cryptoProvider)
    , mAllocator(allocator)
{
}

RetWithError<PrivateKey> Utils::GenerateRSAKeyPairWithLabel(
    const Array<uint8_t>& id, const String& label, size_t bitsCount)
{
    auto funcList = mSession->GetFunctionList();

    if (!funcList || !funcList->C_GenerateKeyPair) {
        return {{}, ErrorEnum::eWrongState};
    }

    CK_OBJECT_CLASS pubKeyClass = CKO_PUBLIC_KEY;
    CK_KEY_TYPE     keyTypeRSA  = CKK_RSA;
    CK_BBOOL        trueVal     = CK_TRUE;
    CK_BBOOL        falseVal    = CK_FALSE;
    CK_BYTE         publicExp[] = {1, 0, 1};
    CK_ULONG        modulusBits = bitsCount;

    StaticArray<CK_ATTRIBUTE, cObjectAttributesCount> pubKeyTempl;
    StaticArray<CK_ATTRIBUTE, cObjectAttributesCount> privKeyTempl;

    pubKeyTempl.PushBack({CKA_CLASS, &pubKeyClass, sizeof(pubKeyClass)});
    pubKeyTempl.PushBack({CKA_KEY_TYPE, &keyTypeRSA, sizeof(keyTypeRSA)});
    pubKeyTempl.PushBack({CKA_TOKEN, &trueVal, sizeof(trueVal)});
    pubKeyTempl.PushBack({CKA_VERIFY, &trueVal, sizeof(trueVal)});
    pubKeyTempl.PushBack({CKA_ENCRYPT, &trueVal, sizeof(trueVal)});
    pubKeyTempl.PushBack({CKA_PUBLIC_EXPONENT, publicExp, sizeof(publicExp)});
    pubKeyTempl.PushBack({CKA_MODULUS_BITS, &modulusBits, sizeof(modulusBits)});
    pubKeyTempl.PushBack({CKA_ID, const_cast<uint8_t*>(id.Get()), id.Size()});
    pubKeyTempl.PushBack({CKA_LABEL, const_cast<char*>(label.Get()), label.Size()});

    privKeyTempl.PushBack({CKA_TOKEN, &trueVal, sizeof(trueVal)});
    privKeyTempl.PushBack({CKA_SIGN, &trueVal, sizeof(trueVal)});
    privKeyTempl.PushBack({CKA_DECRYPT, &trueVal, sizeof(trueVal)});
    privKeyTempl.PushBack({CKA_SENSITIVE, &trueVal, sizeof(trueVal)});
    privKeyTempl.PushBack({CKA_EXTRACTABLE, &falseVal, sizeof(falseVal)});
    privKeyTempl.PushBack({CKA_ID, const_cast<uint8_t*>(id.Get()), id.Size()});
    privKeyTempl.PushBack({CKA_LABEL, const_cast<char*>(label.Get()), label.Size()});

    CK_MECHANISM mechanism = {CKM_RSA_PKCS_KEY_PAIR_GEN, nullptr, 0};

    ObjectHandle privKeyHandle = CK_INVALID_HANDLE;
    ObjectHandle pubKeyHandle  = CK_INVALID_HANDLE;

    CK_RV rv = funcList->C_GenerateKeyPair(mSession->GetHandle(), &mechanism, pubKeyTempl.Get(), pubKeyTempl.Size(),
        privKeyTempl.Get(), privKeyTempl.Size(), &pubKeyHandle, &privKeyHandle);

    if (rv != CKR_OK) {
        return {{}, static_cast<int>(rv)};
    }

    return ExportPrivateKey(privKeyHandle, pubKeyHandle, keyTypeRSA);
}

RetWithError<PrivateKey> Utils::GenerateECDSAKeyPairWithLabel(
    const Array<uint8_t>& id, const String& label, [[maybe_unused]] EllipticCurve curve)
{
    // only P384 curve is supported for now
    assert(curve == EllipticCurve::eP384);

    auto funcList = mSession->GetFunctionList();

    if (!funcList || !funcList->C_GenerateKeyPair) {
        return {{}, ErrorEnum::eWrongState};
    }

    CK_OBJECT_CLASS pubKeyClass  = CKO_PUBLIC_KEY;
    CK_KEY_TYPE     keyTypeECDSA = CKK_ECDSA;
    CK_BBOOL        trueVal      = CK_TRUE;
    CK_BBOOL        falseVal     = CK_FALSE;

    static uint8_t cP384OID[] = {0x06, 0x05, 0x2B, 0x81, 0x04, 0x00, 0x22};

    StaticArray<CK_ATTRIBUTE, cObjectAttributesCount> pubKeyTempl;
    StaticArray<CK_ATTRIBUTE, cObjectAttributesCount> privKeyTempl;

    pubKeyTempl.PushBack({CKA_CLASS, &pubKeyClass, sizeof(pubKeyClass)});
    pubKeyTempl.PushBack({CKA_KEY_TYPE, &keyTypeECDSA, sizeof(keyTypeECDSA)});
    pubKeyTempl.PushBack({CKA_TOKEN, &trueVal, sizeof(trueVal)});
    pubKeyTempl.PushBack({CKA_VERIFY, &trueVal, sizeof(trueVal)});
    pubKeyTempl.PushBack({CKA_ECDSA_PARAMS, cP384OID, sizeof(cP384OID)});
    pubKeyTempl.PushBack({CKA_ID, const_cast<uint8_t*>(id.Get()), id.Size()});
    pubKeyTempl.PushBack({CKA_LABEL, const_cast<char*>(label.Get()), label.Size()});

    privKeyTempl.PushBack({CKA_TOKEN, &trueVal, sizeof(trueVal)});
    privKeyTempl.PushBack({CKA_SIGN, &trueVal, sizeof(trueVal)});
    privKeyTempl.PushBack({CKA_SENSITIVE, &trueVal, sizeof(trueVal)});
    privKeyTempl.PushBack({CKA_EXTRACTABLE, &falseVal, sizeof(falseVal)});
    privKeyTempl.PushBack({CKA_ID, const_cast<uint8_t*>(id.Get()), id.Size()});
    privKeyTempl.PushBack({CKA_LABEL, const_cast<char*>(label.Get()), label.Size()});

    CK_MECHANISM mechanism = {CKM_ECDSA_KEY_PAIR_GEN, nullptr, 0};

    ObjectHandle privKeyHandle = CK_INVALID_HANDLE;
    ObjectHandle pubKeyHandle  = CK_INVALID_HANDLE;

    CK_RV rv = funcList->C_GenerateKeyPair(mSession->GetHandle(), &mechanism, pubKeyTempl.Get(), pubKeyTempl.Size(),
        privKeyTempl.Get(), privKeyTempl.Size(), &pubKeyHandle, &privKeyHandle);

    if (rv != CKR_OK) {
        return {{}, static_cast<int>(rv)};
    }

    return ExportPrivateKey(privKeyHandle, pubKeyHandle, keyTypeECDSA);
}

RetWithError<PrivateKey> Utils::FindPrivateKey(const Array<uint8_t>& id, const String& label)
{
    constexpr auto cSingleAttribute = 1;

    CK_OBJECT_CLASS privKeyClass = CKO_PRIVATE_KEY;
    CK_OBJECT_CLASS pubKeyClass  = CKO_PUBLIC_KEY;

    StaticArray<ObjectAttribute, cObjectAttributesCount> privKeyTempl;

    privKeyTempl.PushBack({CKA_CLASS, ConvertToAttributeValue(privKeyClass)});
    privKeyTempl.PushBack({CKA_ID, id});
    privKeyTempl.PushBack({CKA_LABEL, ConvertToAttributeValue(label)});

    StaticArray<ObjectHandle, cKeysPerToken> privKeys;

    auto err = mSession->FindObjects(privKeyTempl, privKeys);
    if (!err.IsNone()) {
        return {{}, err};
    }

    // Find public part with matching attributes: id, label & key type.
    StaticArray<AttributeType, cSingleAttribute> keyTypeAttribute;

    keyTypeAttribute.PushBack(CKA_KEY_TYPE);

    CK_KEY_TYPE                                          keyType;
    StaticArray<Array<uint8_t>, cSingleAttribute>        keyTypeValue;
    StaticArray<ObjectAttribute, cObjectAttributesCount> pubKeyTempl;

    keyTypeValue.PushBack(ConvertToAttributeValue(keyType));

    pubKeyTempl.PushBack({CKA_CLASS, ConvertToAttributeValue(pubKeyClass)});
    pubKeyTempl.PushBack({CKA_ID, id});
    pubKeyTempl.PushBack({CKA_LABEL, ConvertToAttributeValue(label)});
    pubKeyTempl.PushBack({CKA_KEY_TYPE, ConvertToAttributeValue(keyType)});

    StaticArray<ObjectHandle, cKeysPerToken> pubKeys;

    for (const auto& privKey : privKeys) {
        err = mSession->GetAttributeValues(privKey, keyTypeAttribute, keyTypeValue);
        if (!err.IsNone()) {
            return {{}, err};
        }

        err = mSession->FindObjects(pubKeyTempl, pubKeys);
        if (!err.IsNone()) {
            return {{}, err};
        }

        if (pubKeys.IsEmpty()) {
            continue;
        }

        return ExportPrivateKey(privKey, pubKeys[0], keyType);
    }

    return {{}, ErrorEnum::eNotFound};
}

Error Utils::DeletePrivateKey(const PrivateKey& key)
{
    auto err = mSession->DestroyObject(key.GetPrivHandle());
    if (!err.IsNone()) {
        return err;
    }

    return mSession->DestroyObject(key.GetPubHandle());
}

Error Utils::ImportCertificate(const Array<uint8_t>& id, const String& label, const crypto::x509::Certificate& cert)
{
    auto funcList = mSession->GetFunctionList();

    if (!funcList || !funcList->C_CreateObject) {
        return ErrorEnum::eWrongState;
    }

    CK_OBJECT_CLASS     certClass    = CKO_CERTIFICATE;
    CK_CERTIFICATE_TYPE certTypeX509 = CKC_X_509;
    CK_BBOOL            trueVal      = CK_TRUE;
    CK_BBOOL            falseVal     = CK_FALSE;

    StaticArray<uint8_t, crypto::cSerialNumDERSize> serialNum;

    auto err = mCryptoProvider.ASN1EncodeBigInt(cert.mSerial, serialNum);
    if (!err.IsNone()) {
        return err;
    }

    StaticArray<CK_ATTRIBUTE, cObjectAttributesCount> certTempl;

    certTempl.PushBack({CKA_CLASS, &certClass, sizeof(certClass)});
    certTempl.PushBack({CKA_CERTIFICATE_TYPE, &certTypeX509, sizeof(certTypeX509)});
    certTempl.PushBack({CKA_TOKEN, &trueVal, sizeof(trueVal)});
    certTempl.PushBack({CKA_PRIVATE, &falseVal, sizeof(falseVal)});
    certTempl.PushBack({CKA_SUBJECT, const_cast<uint8_t*>(cert.mSubject.Get()), cert.mSubject.Size()});
    certTempl.PushBack({CKA_ISSUER, const_cast<uint8_t*>(cert.mIssuer.Get()), cert.mIssuer.Size()});
    certTempl.PushBack({CKA_SERIAL_NUMBER, serialNum.Get(), serialNum.Size()});
    certTempl.PushBack({CKA_VALUE, const_cast<uint8_t*>(cert.mRaw.Get()), cert.mRaw.Size()});
    certTempl.PushBack({CKA_ID, const_cast<uint8_t*>(id.Get()), id.Size()});
    if (!label.IsEmpty()) {
        certTempl.PushBack({CKA_LABEL, const_cast<char*>(label.Get()), label.Size()});
    }

    ObjectHandle certHandle = CK_INVALID_HANDLE;

    CK_RV rv = funcList->C_CreateObject(mSession->GetHandle(), certTempl.Get(), certTempl.Size(), &certHandle);
    if (rv != CKR_OK) {
        return static_cast<int>(rv);
    }

    return ErrorEnum::eNone;
}

RetWithError<bool> Utils::HasCertificate(const Array<uint8_t>& issuer, const Array<uint8_t>& serialNumber)
{
    static constexpr auto cSingleObject = 1;

    CK_OBJECT_CLASS                                 certClass = CKO_CERTIFICATE;
    StaticArray<uint8_t, crypto::cSerialNumDERSize> asn1SerialNum;

    auto err = mCryptoProvider.ASN1EncodeBigInt(serialNumber, asn1SerialNum);
    if (!err.IsNone()) {
        return {false, err};
    }

    StaticArray<ObjectAttribute, cObjectAttributesCount> certTempl;
    certTempl.PushBack({CKA_CLASS, ConvertToAttributeValue(certClass)});
    certTempl.PushBack({CKA_ISSUER, issuer});
    certTempl.PushBack({CKA_SERIAL_NUMBER, asn1SerialNum});

    StaticArray<ObjectHandle, cSingleObject> certHandles;

    err = mSession->FindObjects(certTempl, certHandles);
    // "not enough memory" treat as success and certificate found
    if (err.Is(ErrorEnum::eNoMemory)) {
        return {true, ErrorEnum::eNone};
    }

    if (err.Is(ErrorEnum::eNotFound)) {
        return {false, ErrorEnum::eNone};
    }

    if (!err.IsNone()) {
        return {false, err};
    }

    return {!certHandles.IsEmpty(), ErrorEnum::eNone};
}

RetWithError<SharedPtr<crypto::x509::CertificateChain>> Utils::FindCertificateChain(
    const Array<uint8_t>& id, const String& label)
{
    StaticArray<ObjectHandle, cKeysPerToken> certHandles;

    auto err = FindCertificates(id, label, certHandles);
    if (!err.IsNone()) {
        return {nullptr, err};
    }

    SharedPtr<crypto::x509::Certificate> certificate;
    auto                                 chain = MakeShared<crypto::x509::CertificateChain>(&mAllocator);

    Tie(certificate, err) = GetCertificate(certHandles[0]);
    if (!err.IsNone()) {
        return {nullptr, err};
    }

    err = chain->PushBack(*certificate);
    if (!err.IsNone()) {
        return {nullptr, err};
    }

    err = FindCertificateChain(*certificate, *chain);
    if (!err.IsNone()) {
        return {nullptr, err};
    }

    return {chain, err};
}

Error Utils::DeleteCertificate(const Array<uint8_t>& id, const String& label)
{
    CK_OBJECT_CLASS                              certClass = CKO_CERTIFICATE;
    StaticArray<uint8_t, crypto::cSerialNumSize> asn1SerialNum;

    StaticArray<ObjectAttribute, cObjectAttributesCount> certTempl;

    certTempl.PushBack({CKA_CLASS, ConvertToAttributeValue(certClass)});
    certTempl.PushBack({CKA_ID, id});
    certTempl.PushBack({CKA_LABEL, ConvertToAttributeValue(label)});

    StaticArray<ObjectHandle, cKeysPerToken> certHandles;

    auto err = mSession->FindObjects(certTempl, certHandles);
    if (!err.IsNone()) {
        return err;
    }

    for (const auto handle : certHandles) {
        err = mSession->DestroyObject(handle);
        if (!err.IsNone()) {
            return err;
        }
    }

    return ErrorEnum::eNone;
}

Error Utils::ConvertPKCS11String(const Array<uint8_t>& src, String& dst)
{
    return ConvertFromPKCS11String(src, dst);
}

RetWithError<PrivateKey> Utils::ExportPrivateKey(
    ObjectHandle privKeyHandle, ObjectHandle pubKeyHandle, CK_KEY_TYPE keyType)
{
    switch (keyType) {
    case CKK_RSA: {
        StaticArray<Array<uint8_t>, cObjectAttributesCount> attrValues;
        StaticArray<AttributeType, cObjectAttributesCount>  attrTypes;

        attrTypes.PushBack(CKA_MODULUS);
        attrTypes.PushBack(CKA_PUBLIC_EXPONENT);

        auto n = MakeUnique<StaticArray<uint8_t, crypto::cRSAModulusSize>>(&mAllocator);
        auto e = MakeUnique<StaticArray<uint8_t, crypto::cRSAPubExponentSize>>(&mAllocator);

        attrValues.PushBack(*n);
        attrValues.PushBack(*e);

        auto err = mSession->GetAttributeValues(pubKeyHandle, attrTypes, attrValues);
        if (!err.IsNone()) {
            return {{}, err};
        }

        auto pubKey    = MakeUnique<crypto::RSAPublicKey>(&mAllocator, attrValues[0], attrValues[1]);
        auto cryptoKey = MakeShared<PKCS11RSAPrivateKey>(&mAllocator, mSession, privKeyHandle, *pubKey);

        PrivateKey pkcsKey = {privKeyHandle, pubKeyHandle, cryptoKey};

        return {pkcsKey, ErrorEnum::eNone};
    }

    case CKK_ECDSA: {
        StaticArray<Array<uint8_t>, cObjectAttributesCount> attrValues;
        StaticArray<AttributeType, cObjectAttributesCount>  attrTypes;

        attrTypes.PushBack(CKA_ECDSA_PARAMS);
        attrTypes.PushBack(CKA_EC_POINT);

        auto derEncodedParams = MakeUnique<StaticArray<uint8_t, crypto::cECDSAParamsOIDSize>>(&mAllocator);
        auto derEncodedPoint  = MakeUnique<StaticArray<uint8_t, crypto::cECDSAPointDERSize>>(&mAllocator);

        attrValues.PushBack(*derEncodedParams);
        attrValues.PushBack(*derEncodedPoint);

        auto err = mSession->GetAttributeValues(pubKeyHandle, attrTypes, attrValues);
        if (!err.IsNone()) {
            return {{}, err};
        }

        auto params = MakeUnique<StaticArray<uint8_t, crypto::cECDSAParamsOIDSize>>(&mAllocator);
        auto point  = MakeUnique<StaticArray<uint8_t, crypto::cECDSAPointDERSize>>(&mAllocator);

        err = mCryptoProvider.ASN1DecodeOID(attrValues[0], *params);
        if (!err.IsNone()) {
            return {{}, err};
        }

        err = mCryptoProvider.ASN1DecodeOctetString(attrValues[1], *point);
        if (!err.IsNone()) {
            return {{}, err};
        }

        auto pubKey = MakeUnique<crypto::ECDSAPublicKey>(&mAllocator, *params, *point);
        auto cryptoKey
            = MakeShared<PKCS11ECDSAPrivateKey>(&mAllocator, mSession, mCryptoProvider, privKeyHandle, *pubKey);

        PrivateKey pkcsKey = {privKeyHandle, pubKeyHandle, cryptoKey};

        return {pkcsKey, ErrorEnum::eNone};
    }
    }

    return {{}, ErrorEnum::eInvalidArgument};
}

Error Utils::FindCertificates(const Array<uint8_t>& id, const String& label, Array<ObjectHandle>& handles)
{
    CK_OBJECT_CLASS certClass = CKO_CERTIFICATE;

    StaticArray<ObjectAttribute, cObjectAttributesCount> certTempl;

    certTempl.PushBack({CKA_CLASS, ConvertToAttributeValue(certClass)});
    certTempl.PushBack({CKA_ID, id});
    certTempl.PushBack({CKA_LABEL, ConvertToAttributeValue(label)});

    return mSession->FindObjects(certTempl, handles);
}

Error Utils::FindCertificateChain(const crypto::x509::Certificate& certificate, crypto::x509::CertificateChain& chain)
{
    if (certificate.mIssuer.IsEmpty() || certificate.mIssuer == certificate.mSubject) {
        return ErrorEnum::eNone;
    }

    CK_OBJECT_CLASS                                      certClass = CKO_CERTIFICATE;
    StaticArray<ObjectAttribute, cObjectAttributesCount> certTempl;

    certTempl.PushBack({CKA_CLASS, ConvertToAttributeValue(certClass)});
    certTempl.PushBack({CKA_SUBJECT, certificate.mIssuer});

    StaticArray<ObjectHandle, cKeysPerToken> handles;
    SharedPtr<crypto::x509::Certificate>     foundCert;

    auto err = mSession->FindObjects(certTempl, handles);
    if (err.IsNone()) {
        Tie(foundCert, err) = GetCertificate(handles[0]);
    } else if (err == ErrorEnum::eNotFound && !certificate.mAuthorityKeyId.IsEmpty()) {
        Tie(foundCert, err) = FindCertificateByKeyID(certificate.mAuthorityKeyId);
    } else {
        return err;
    }

    if (!err.IsNone()) {
        return err;
    }

    for (const auto& cur : chain) {
        if (cur.mSubject == foundCert->mSubject) {
            return ErrorEnum::eNone;
        }
    }

    err = chain.PushBack(*foundCert);
    if (!err.IsNone()) {
        return err;
    }

    return FindCertificateChain(*foundCert, chain);
}

RetWithError<SharedPtr<crypto::x509::Certificate>> Utils::FindCertificateByKeyID(const Array<uint8_t>& keyID)
{
    CK_OBJECT_CLASS                                      certClass = CKO_CERTIFICATE;
    StaticArray<ObjectAttribute, cObjectAttributesCount> certTempl;

    certTempl.PushBack({CKA_CLASS, ConvertToAttributeValue(certClass)});

    StaticArray<ObjectHandle, cKeysPerToken> handles;

    auto err = mSession->FindObjects(certTempl, handles);
    if (err.IsNone()) {
        return {nullptr, err};
    }

    for (auto handle : handles) {
        SharedPtr<crypto::x509::Certificate> certificate;

        Tie(certificate, err) = GetCertificate(handle);
        if (!err.IsNone()) {
            return {nullptr, err};
        }

        if (certificate->mSubjectKeyId == keyID) {
            return {certificate, ErrorEnum::eNone};
        }
    }

    return {nullptr, ErrorEnum::eNotFound};
}

RetWithError<SharedPtr<crypto::x509::Certificate>> Utils::GetCertificate(ObjectHandle handle)
{
    auto certificate = MakeShared<crypto::x509::Certificate>(&mAllocator);
    StaticArray<Array<uint8_t>, cObjectAttributesCount> attrValues;
    StaticArray<AttributeType, cObjectAttributesCount>  attrTypes;

    certificate->mRaw.Resize(certificate->mRaw.MaxSize());

    attrTypes.PushBack(CKA_VALUE);
    attrValues.PushBack(certificate->mRaw);

    auto err = mSession->GetAttributeValues(handle, attrTypes, attrValues);
    if (!err.IsNone()) {
        return {nullptr, err};
    }

    certificate->mRaw.Resize(attrValues[0].Size());

    err = mCryptoProvider.DERToX509Cert(certificate->mRaw, *certificate);

    return {certificate, err};
}

} // namespace pkcs11
} // namespace aos
