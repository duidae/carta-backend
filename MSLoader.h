#pragma once

#include "FileLoader.h"
#include <casacore/images/Images/PagedImage.h>
#include <string>
#include <unordered_map>

namespace carta {

class MSLoader : public FileLoader {
public:
    MSLoader(const std::string &file);
    void openFile(const std::string &file) override;
    bool hasData(FileInfo::Data ds) const override;
    image_ref loadData(FileInfo::Data ds) override;

private:
    std::string file;
    casacore::PagedImage<float> image;
};

MSLoader::MSLoader(const std::string &filename)
    : file(filename),
      image(filename)
{}

void MSLoader::openFile(const std::string &filename) {
    file = filename;
    image = casacore::PagedImage<float>(filename);
}

bool MSLoader::hasData(FileInfo::Data dl) const {
    switch(dl) {
    case FileInfo::Data::XY:
        return image.shape().size() >= 2;
    case FileInfo::Data::XYZ:
        return image.shape().size() >= 3;
    case FileInfo::Data::XYZW:
        return image.shape().size() >= 4;
    default:
        break;
    }
    return false;
}

typename MSLoader::image_ref MSLoader::loadData(FileInfo::Data) {
    return image;
}

} // namespace carta
