//# FileExtInfoLoader.h: load FileInfoExtended fields for all supported file types

#ifndef FILEEXTINFOLOADER_H
#define FILEEXTINFOLOADER_H

#include <carta-protobuf/file_info.pb.h>
#include <casacore/images/Images/ImageOpener.h>
#include <string>

class FileExtInfoLoader {

public:
    FileExtInfoLoader(const std::string& filename, const std::string& hdu);
    ~FileExtInfoLoader() {}

    bool fillFileExtInfo(CARTA::FileInfoExtended* extInfo, std::string& message);

private:
    casacore::ImageOpener::ImageTypes fileType(const std::string &file);
    bool fillHdf5ExtFileInfo(CARTA::FileInfoExtended* extInfo, std::string& message);
    bool fillFITSExtFileInfo(CARTA::FileInfoExtended* extInfo, std::string& message);
    bool fillCASAExtFileInfo(CARTA::FileInfoExtended* extInfo, std::string& message);

    std::string m_file;
    std::string m_hdu;
    casacore::ImageOpener::ImageTypes m_type; 
};

#endif
