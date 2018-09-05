#pragma once

#include <casacore/images/Images/HDF5Image.h>
#include <casacore/images/Images/ImageInterface.h>
#include <casacore/images/Images/ImageOpener.h>
#include <string>
#include <memory>

class FileLoader {
public:
    using image_ref = casacore::Lattice<float>&;
    virtual ~FileLoader() = default;

    static FileLoader* getLoader(const std::string &file);
    static casacore::ImageOpener::ImageTypes fileType(const std::string &file);

    // Do anything required to open the file (set up cache size, etc)
    virtual void openFile(const std::string &file) = 0;
    // Check to see if the file has a particular HDU/group/table/etc
    virtual bool hasData(const std::string &data) const = 0;
    // Return a casacore image type representing the data stored in the
    // specified HDU/group/table/etc.
    virtual image_ref loadData(const std::string &data) = 0;
};

template <typename T>
casacore::ImageInterface<T>* getImage(const std::string &file) {
    casacore::ImageOpener::ImageTypes type = FileLoader::fileType(file);
    switch(type) {
    case casacore::ImageOpener::AIPSPP:
        break;
    case casacore::ImageOpener::FITS:
        break;
    case casacore::ImageOpener::MIRIAD:
        break;
    case casacore::ImageOpener::GIPSY:
        break;
    case casacore::ImageOpener::CAIPS:
        break;
    case casacore::ImageOpener::NEWSTAR:
        break;
    case casacore::ImageOpener::HDF5:
        return new casacore::HDF5Image<T>(file);
    case casacore::ImageOpener::IMAGECONCAT:
        break;
    case casacore::ImageOpener::IMAGEEXPR:
        break;
    case casacore::ImageOpener::COMPLISTIMAGE:
        break;
    default:
        break;
    }
    return nullptr;
}

inline casacore::ImageOpener::ImageTypes
FileLoader::fileType(const std::string &file) {
    return casacore::ImageOpener::imageType(file);
}
