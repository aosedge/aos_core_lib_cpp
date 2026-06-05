#include "certhandlerstub.hpp"

namespace aos::iam::certhandler {

Error StorageStub::AddCertInfo(const String& certType, const CertInfo& certInfo)
{
    auto cell = FindCell(certType);

    if (cell == mStorage.end()) {
        Error err = mStorage.EmplaceBack();
        if (!err.IsNone()) {
            return err;
        }

        cell            = &mStorage.Back();
        cell->mCertType = certType;
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

Error StorageStub::GetCertsInfo(const String& certType, Array<CertInfo>& certsInfo)
{
    const auto* cell = FindCell(certType);
    if (cell == mStorage.end()) {
        return ErrorEnum::eNotFound;
    }

    certsInfo.Clear();

    for (const auto& cert : cell->mCertificates) {
        Error err = certsInfo.PushBack(cert);
        if (!err.IsNone()) {
            return err;
        }
    }

    return ErrorEnum::eNone;
}

Error StorageStub::RemoveCertInfo(const String& certType, const String& certURL)
{
    auto* cell = FindCell(certType);
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

Error StorageStub::RemoveAllCertsInfo(const String& certType)
{
    const auto* cell = FindCell(certType);
    if (cell == mStorage.end()) {
        return ErrorEnum::eNotFound;
    }

    mStorage.Erase(cell);

    return ErrorEnum::eNone;
}

StorageStub::StorageCell* StorageStub::FindCell(const String& certType)
{
    for (auto& cell : mStorage) {
        if (cell.mCertType == certType) {
            return &cell;
        }
    }

    return mStorage.end();
}

} // namespace aos::iam::certhandler
