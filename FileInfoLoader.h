//# FileInfoLoader.h: load FileInfo and FileInfoExtended fields for all supported file types

#pragma once

#include <carta-protobuf/file_info.pb.h>
#include <casacore/images/Images/ImageOpener.h>
#include <string>

class FileInfoLoader {

public:
    FileInfoLoader(const std::string& filename);
    ~FileInfoLoader() {}

    bool fillFileInfo(CARTA::FileInfo* fileInfo);
    bool fillFileExtInfo(CARTA::FileInfoExtended* extInfo, std::string& hdu, std::string& message);

private:
    casacore::ImageOpener::ImageTypes fileType(const std::string &file);
    CARTA::FileType convertFileType(int ccImageType);

    bool getHduList(CARTA::FileInfo* fileInfo, const std::string& filename);

    bool fillHdf5ExtFileInfo(CARTA::FileInfoExtended* extInfo, std::string& hdu, std::string& message);
    bool fillFITSExtFileInfo(CARTA::FileInfoExtended* extInfo, std::string& hdu, std::string& message);
    bool fillCASAExtFileInfo(CARTA::FileInfoExtended* extInfo, std::string& message);
    void addComputedEntries(CARTA::FileInfoExtended* extInfo, const std::string& coordinateTypeX,
        const std::string& coordinateTypeY, const std::string& radeSys, const bool stokesIsAxis4);

    std::string m_file;
    casacore::ImageOpener::ImageTypes m_type; 
};

