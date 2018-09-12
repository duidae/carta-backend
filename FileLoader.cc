#include "CasaLoader.h"
#include "FileLoader.h"
#include "HDF5Loader.h"
#include "FITSLoader.h"

using namespace carta;

FileLoader* FileLoader::getLoader(const std::string &file) {
    casacore::ImageOpener::ImageTypes type = FileInfo::fileType(file);
    switch(type) {
    case casacore::ImageOpener::AIPSPP:
        return new CasaLoader(file);
    case casacore::ImageOpener::FITS:
        return new FITSLoader(file);
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
        return new HDF5Loader(file);
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

template <typename T>
casacore::ImageInterface<T>* getImage(const std::string &file, const std::string &hdu) {
    casacore::ImageOpener::ImageTypes type = FileInfo::fileType(file);
    switch(type) {
    case casacore::ImageOpener::AIPSPP:
        break;
    case casacore::ImageOpener::FITS: {
        casacore::uInt fitsHdu(FileInfo::getFITShdu(hdu));
        return new casacore::FITSImage(file, 0, fitsHdu);
	}
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
