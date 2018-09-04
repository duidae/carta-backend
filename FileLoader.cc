#include "FileLoader.h"
#include "HDF5Loader.h"

FileLoader* FileLoader::getLoader(std::string file) {
    casacore::ImageOpener::ImageTypes type = fileType(file);
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
        return new HDF5Loader;
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
