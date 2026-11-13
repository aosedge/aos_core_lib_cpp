#include "certhandlerstub.hpp"

namespace aos::iam::certhandler {

Error StorageStub::AddCertInfo(const String& storageID, const CertInfo& certInfo)
{
    auto cell = FindCell(storageID);

    if (cell == mStorage.end()) {
        auto err = mStorage.EmplaceBack();
        if (!err.IsNone()) {
            return err;
        }

        cell             = &mStorage.Back();
        cell->mStorageID = storageID;
    }

    for (const auto& cert : cell->mCertificates) {
        if (cert == certInfo) {
            return ErrorEnum::eAlreadyExist;
        }
    }

    return cell->mCertificates.PushBack(certInfo);
}

Error StorageStub::GetCertInfo(const Array<uint8_t>& issuer, const Array<uint8_t>& serial, CertInfo& cert)
{
    for (const auto& cell : mStorage) {
        for (const auto& cur : cell.mCertificates) {
            if (cur.mIssuer == issuer && cur.mSerial == serial) {
                cert = cur;
                return ErrorEnum::eNone;
            }
        }
    }

    return ErrorEnum::eNotFound;
}

Error StorageStub::GetCertsInfo(const String& storageID, Array<CertInfo>& certsInfo)
{
    const auto* cell = FindCell(storageID);
    if (cell == mStorage.end()) {
        return ErrorEnum::eNotFound;
    }

    certsInfo.Clear();

    for (const auto& cert : cell->mCertificates) {
        auto err = certsInfo.PushBack(cert);
        if (!err.IsNone()) {
            return err;
        }
    }

    return ErrorEnum::eNone;
}

Error StorageStub::RemoveCertInfo(const String& storageID, const String& certURL)
{
    auto* cell = FindCell(storageID);
    if (cell == mStorage.end()) {
        return ErrorEnum::eNotFound;
    }

    for (const auto& cur : cell->mCertificates) {
        if (cur.mCertURL == certURL) {
            cell->mCertificates.Erase(&cur);

            return ErrorEnum::eNone;
        }
    }

    return ErrorEnum::eNotFound;
}

Error StorageStub::RemoveAllCertsInfo(const String& storageID)
{
    const auto* cell = FindCell(storageID);
    if (cell == mStorage.end()) {
        return ErrorEnum::eNotFound;
    }

    mStorage.Erase(cell);

    return ErrorEnum::eNone;
}

StorageStub::StorageCell* StorageStub::FindCell(const String& storageID)
{
    for (auto& cell : mStorage) {
        if (cell.mStorageID == storageID) {
            return &cell;
        }
    }

    return mStorage.end();
}

} // namespace aos::iam::certhandler
