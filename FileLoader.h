#pragma once

// #include <casacore/images/Images/ImageInterface.h>
#include <casacore/images/Images/ImageOpener.h>
#include <H5Cpp.h>
#include <string>
#include <memory>

class FileLoader {
public:
    // using image_ptr = std::unique_ptr<casacore::ImageInterface<float>>;
    using image_ptr = H5::DataSet;

    static FileLoader* getLoader(std::string file);

    virtual ~FileLoader() = default;

    static casacore::ImageOpener::ImageTypes fileType(const std::string &file);

    // Do anything required to open the file (set up cache size, etc)
    virtual void openFile(const std::string &file) = 0;
    // Check to see if the file has a particular HDU/group/table/etc
    virtual bool hasData(const std::string &data) const = 0;
    // Return a casacore image type representing the data stored in the
    // specified HDU/group/table/etc.
    virtual image_ptr loadData(const std::string &data) = 0;
};

inline casacore::ImageOpener::ImageTypes
FileLoader::fileType(const std::string &file) {
    return casacore::ImageOpener::imageType(file);
}
