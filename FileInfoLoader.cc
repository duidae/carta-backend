//# FileInfoLoader.cc: fill FileInfoExtended for all supported file types

#include "FileInfoLoader.h"

#include <fmt/format.h>
#include <H5Cpp.h>
#include <H5File.h>

#include <casacore/casa/OS/File.h>
#include <casacore/fits/FITS/FITSTable.h>
#include <casacore/casa/HDF5/HDF5File.h>
#include <casacore/casa/HDF5/HDF5Group.h>
#include <casacore/images/Images/ImageSummary.h>
#include <casacore/images/Images/FITSImgParser.h>
#include <casacore/images/Images/MIRIADImage.h>
#include <casacore/images/Images/PagedImage.h>

using namespace std;
using namespace CARTA;
using namespace H5;

FileInfoLoader::FileInfoLoader(const string& filename) :
    m_file(filename) {
    m_type = fileType(filename);
}

casacore::ImageOpener::ImageTypes
FileInfoLoader::fileType(const std::string &file) {
    return casacore::ImageOpener::imageType(file);
}

//#################################################################################
// FILE INFO

bool FileInfoLoader::fillFileInfo(FileInfo* fileInfo) {
    // fill FileInfo submessage
    casacore::File ccfile(m_file);
    fileInfo->set_size(ccfile.size());
    fileInfo->set_name(ccfile.path().baseName());
    casacore::String absFileName(ccfile.path().absoluteName());
    fileInfo->set_type(convertFileType(m_type));
    return getHduList(fileInfo, absFileName);
}

CARTA::FileType FileInfoLoader::convertFileType(int ccImageType) {
    // convert casacore ImageType to protobuf FileType
    switch (ccImageType) {
        case casacore::ImageOpener::FITS:
            return FileType::FITS;
        case casacore::ImageOpener::AIPSPP:
            return FileType::CASA;
        case casacore::ImageOpener::HDF5:
            return FileType::HDF5;
        case casacore::ImageOpener::MIRIAD:
            return FileType::MIRIAD;
        default:
            return FileType::UNKNOWN;
    }
}

bool FileInfoLoader::getHduList(FileInfo* fileInfo, const std::string& filename) {
    // fill FileInfo hdu list
    bool hduOK(true);
    if (fileInfo->type()==CARTA::HDF5) {
        casacore::HDF5File hdfFile(filename);
        std::vector<casacore::String> hdus(casacore::HDF5Group::linkNames(hdfFile));
        for (auto groupName : hdus)
            fileInfo->add_hdu_list(groupName);
        hduOK = (fileInfo->hdu_list_size() > 0);
    } else if (fileInfo->type()==CARTA::FITS) {
        casacore::FITSImgParser fitsParser(filename.c_str());
        int numHdu(fitsParser.get_numhdu());
        for (int hdu=0; hdu<numHdu; ++hdu) {
            fileInfo->add_hdu_list(casacore::String::toString(hdu));
        }
        hduOK = (fileInfo->hdu_list_size() > 0);
    } else {
        fileInfo->add_hdu_list("");
    }
    return hduOK;
}

//#################################################################################
// FILE INFO EXTENDED

bool FileInfoLoader::fillFileExtInfo(FileInfoExtended* extInfo, string& hdu, string& message) {
    bool extInfoOK(false);
    switch(m_type) {
    case casacore::ImageOpener::AIPSPP:
        extInfoOK = fillCASAExtFileInfo(extInfo, message);
        break;
    case casacore::ImageOpener::FITS:
        extInfoOK = fillFITSExtFileInfo(extInfo, hdu, message);
        break;
    case casacore::ImageOpener::HDF5:
        extInfoOK = fillHdf5ExtFileInfo(extInfo, hdu, message);
        break;
    case casacore::ImageOpener::MIRIAD:
        extInfoOK = fillCASAExtFileInfo(extInfo, message);
        break;
    default:
        break;
    }
    return extInfoOK;
}

bool FileInfoLoader::fillHdf5ExtFileInfo(FileInfoExtended* extendedInfo, string& hdu, string& message) {
    // Add extended info for HDF5 file
    bool extInfoOK(true);
    H5File file(m_file, H5F_ACC_RDONLY);
    bool hasHDU;
    if (hdu.length()) {
        hasHDU = H5Lexists(file.getId(), hdu.c_str(), 0);
    } else {
        auto N = file.getNumObjs();
        hasHDU = false;
        for (auto i = 0; i < N; i++) {
            if (file.getObjTypeByIdx(i) == H5G_GROUP) {
                hdu = file.getObjnameByIdx(i);
                hasHDU = true;
                break;
            }
        }
    }

    if (hasHDU) {
        H5::Group topLevelGroup = file.openGroup(hdu);
        if (H5Lexists(topLevelGroup.getId(), "DATA", 0)) {
            DataSet dataSet = topLevelGroup.openDataSet("DATA");
            vector<hsize_t> dims(dataSet.getSpace().getSimpleExtentNdims(), 0);
            dataSet.getSpace().getSimpleExtentDims(dims.data(), NULL);
            uint32_t ndims = dims.size();
            extendedInfo->set_dimensions(ndims);
            if (ndims < 2 || ndims > 4) {
                message = "Image must be 2D, 3D or 4D.";
                return false;
            }
            extendedInfo->set_width(dims[ndims - 1]);
            extendedInfo->set_height(dims[ndims - 2]);
            extendedInfo->set_depth((ndims > 2) ? dims[ndims - 3] : 1);
            extendedInfo->set_stokes((ndims > 3) ? dims[ndims - 4] : 1);

            // if in header, save values for computed entries
            string coordinateTypeX, coordinateTypeY, radeSys;

            // header_entries
            H5O_info_t groupInfo;
            H5Oget_info(topLevelGroup.getId(), &groupInfo);
            for (auto i = 0; i < groupInfo.num_attrs; i++) {
                auto headerEntry = extendedInfo->add_header_entries();
                Attribute attr = topLevelGroup.openAttribute(i);
                headerEntry->set_name(attr.getName());

                hid_t attrTypeId = H5Aget_type(attr.getId());
                auto typeClass = H5Tget_class(attrTypeId);
                if (typeClass == H5T_STRING) {
                    attr.read(attr.getStrType(), *headerEntry->mutable_value());
                    headerEntry->set_entry_type(EntryType::STRING);
                    if (headerEntry->name() == "CTYPE1") 
                        coordinateTypeX = headerEntry->value();
                    else if (headerEntry->name() == "CTYPE2")
                        coordinateTypeY = headerEntry->value();
                    else if (headerEntry->name() == "RADESYS") 
                        radeSys = headerEntry->value();
                } else if (typeClass == H5T_INTEGER) {
                    int64_t valueInt;
                    DataType intType(PredType::NATIVE_INT64);
                    attr.read(intType, &valueInt);
                    *headerEntry->mutable_value() = fmt::format("{}", valueInt);
                    headerEntry->set_numeric_value(valueInt);
                    headerEntry->set_entry_type(EntryType::INT);
                } else if (typeClass == H5T_FLOAT) {
                    DataType doubleType(PredType::NATIVE_DOUBLE);
                    double numericValue = 0;
                    attr.read(doubleType, &numericValue);
                    headerEntry->set_numeric_value(numericValue);
                    headerEntry->set_entry_type(EntryType::FLOAT);
                    *headerEntry->mutable_value() = fmt::format("{:f}", numericValue);
                }
            }
            // computed_entries
            addComputedEntries(extendedInfo, coordinateTypeX, coordinateTypeY, radeSys);
        } else {
            message = "File is missing DATA dataset";
            extInfoOK = false;
        }
    } else {
        message = "File is missing top-level group";
        extInfoOK = false;
    }
    return extInfoOK;
}

bool FileInfoLoader::fillFITSExtFileInfo(FileInfoExtended* extendedInfo, string& hdu, string& message) {
    bool extInfoOK(true);
    try {
        // convert string hdu to unsigned int
        casacore::String ccHdu(hdu);
        casacore::uInt hdunum;
        ccHdu.fromString(hdunum, true);
        hdunum += 1;  // FITSTable starts at 1

        // use FITSTable to get Record of hdu entries
        casacore::FITSTable fitsTable(m_file, hdunum, true); 
        casacore::Record hduEntries(fitsTable.primaryKeywords().toRecord());
        // set dims
        casacore::Int dim = hduEntries.asInt("NAXIS");
        extendedInfo->set_dimensions(dim);
        if (dim < 2 || dim > 4) {
            message = "Image must be 2D, 3D or 4D.";
            return false;
        }
        extendedInfo->set_width(hduEntries.asInt("NAXIS1"));
        extendedInfo->set_height(hduEntries.asInt("NAXIS2"));
        extendedInfo->set_depth((dim > 2) ? hduEntries.asInt("NAXIS3") : 1);
        if (dim > 3) {
            extendedInfo->set_stokes(hduEntries.asInt("NAXIS4"));
        } else { 
            extendedInfo->set_stokes(1);
        }
        extendedInfo->add_stokes_vals(""); // not in header

        // if in header, save values for computed entries
        string coordinateTypeX, coordinateTypeY, radeSys;

        // set header entries 
        for (casacore::uInt field=0; field < hduEntries.nfields(); ++field) {
            casacore::String name = hduEntries.name(field);
        if ((name!="SIMPLE") && (name!="BITPIX") && !name.startsWith("PC")) {
                auto headerEntry = extendedInfo->add_header_entries();
                headerEntry->set_name(name);
                casacore::DataType dtype(hduEntries.type(field));
                switch (dtype) {
                    case casacore::TpString: {
                        *headerEntry->mutable_value() = hduEntries.asString(field);
                        headerEntry->set_entry_type(EntryType::STRING);
			if (headerEntry->name() == "CTYPE1") 
                            coordinateTypeX = headerEntry->value();
                        else if (headerEntry->name() == "CTYPE2")
                            coordinateTypeY = headerEntry->value();
                        else if (headerEntry->name() == "RADESYS") 
                            radeSys = headerEntry->value();
                        break;
                    }
                    case casacore::TpInt: {
                        int64_t valueInt(hduEntries.asInt(field));
                        *headerEntry->mutable_value() = fmt::format("{}", valueInt);
                        headerEntry->set_entry_type(EntryType::INT);
                        headerEntry->set_numeric_value(valueInt);
                        break;
                    }
                    case casacore::TpFloat:
                    case casacore::TpDouble: {
                        double numericValue(hduEntries.asDouble(field));
                        *headerEntry->mutable_value() = fmt::format("{}", numericValue);
                        headerEntry->set_entry_type(EntryType::FLOAT);
                        headerEntry->set_numeric_value(numericValue);
                        break;
                    }
                    default:
                        break;
                }
            }
        }
        // computed_entries
        addComputedEntries(extendedInfo, coordinateTypeX, coordinateTypeY, radeSys);
    } catch (casacore::AipsError& err) {
        message = err.getMesg();
        extInfoOK = false;
    }
    return extInfoOK;
}

bool FileInfoLoader::fillCASAExtFileInfo(FileInfoExtended* extendedInfo, string& message) {
    bool extInfoOK(true);
    casacore::ImageInterface<casacore::Float>* ccImage(nullptr);
    try {
        switch (m_type) {
            case casacore::ImageOpener::AIPSPP: {
                ccImage = new casacore::PagedImage<casacore::Float>(m_file);
                break;
            }
            case casacore::ImageOpener::MIRIAD: {
                ccImage = new casacore::MIRIADImage(m_file);
                break;
            }
            default:
                break;
        }
        casacore::ImageInfo imInfo(ccImage->imageInfo());
        casacore::ImageSummary<casacore::Float> imSummary(*ccImage);
        // set dim
        casacore::Int dim(imSummary.ndim());
        extendedInfo->set_dimensions(dim);
        if (dim < 2 || dim > 4) {
            message = "Image must be 2D, 3D or 4D.";
            return false;
        }
        casacore::IPosition imShape(imSummary.shape());
        extendedInfo->set_width(imShape(0));
        extendedInfo->set_height(imShape(1));
        extendedInfo->set_depth(dim > 2 ? imShape(2) : 1);
        extendedInfo->set_stokes(dim > 3 ? imShape(3) : 1);
        extendedInfo->add_stokes_vals(""); // not in header

        // if in header, save values for computed entries
        string coordinateTypeX, coordinateTypeY, radeSys;

        // set dims in header entries
        auto headerEntry = extendedInfo->add_header_entries();
        headerEntry->set_name("NAXIS");
        *headerEntry->mutable_value() = fmt::format("{}", dim);
        headerEntry->set_entry_type(EntryType::INT);
        headerEntry->set_numeric_value(dim);
        for (casacore::Int i=0; i<dim; ++i) {
            auto headerEntry = extendedInfo->add_header_entries();
            headerEntry->set_name("NAXIS"+ casacore::String::toString(i+1));
            *headerEntry->mutable_value() = fmt::format("{}", imShape(i));
            headerEntry->set_entry_type(EntryType::INT);
            headerEntry->set_numeric_value(imShape(i));
        }
        // BMAJ, BMIN, BPA
        if (imInfo.hasBeam() && imInfo.hasSingleBeam()) {
            // get values
            casacore::GaussianBeam rbeam(imInfo.restoringBeam());
            casacore::Quantity majAx(rbeam.getMajor()), minAx(rbeam.getMinor()),
                pa(rbeam.getPA(true));
            majAx.convert("deg");
            minAx.convert("deg");
            pa.convert("deg");
            if (majAx.getValue()<1.0 || minAx.getValue()<1.0) {
                majAx.convert(casacore::Unit("arcsec"));
                minAx.convert(casacore::Unit("arcsec"));
            }
            // add to header entries
            casacore::Double bmaj(majAx.getValue()), bmin(minAx.getValue());
            casacore::Float bpa(pa.getValue());
            headerEntry = extendedInfo->add_header_entries();
            headerEntry->set_name("BMAJ");
            *headerEntry->mutable_value() = fmt::format("{}", bmaj);
            headerEntry->set_entry_type(EntryType::FLOAT);
            headerEntry->set_numeric_value(bmaj);
            headerEntry = extendedInfo->add_header_entries();
            headerEntry->set_name("BMIN");
            *headerEntry->mutable_value() = fmt::format("{}", bmin);
            headerEntry->set_entry_type(EntryType::FLOAT);
            headerEntry->set_numeric_value(bmin);
            headerEntry = extendedInfo->add_header_entries();
            headerEntry->set_name("BPA");
            *headerEntry->mutable_value() = fmt::format("{}", bpa);
            headerEntry->set_entry_type(EntryType::FLOAT);
            headerEntry->set_numeric_value(bpa);
        }
        // type
        headerEntry = extendedInfo->add_header_entries();
        headerEntry->set_name("BTYPE");
        *headerEntry->mutable_value() = casacore::ImageInfo::imageType(imInfo.imageType());
        headerEntry->set_entry_type(EntryType::STRING);
        // object
        headerEntry = extendedInfo->add_header_entries();
        headerEntry->set_name("OBJECT");
        *headerEntry->mutable_value() = imInfo.objectName();
        headerEntry->set_entry_type(EntryType::STRING);
        // units
        headerEntry = extendedInfo->add_header_entries();
        headerEntry->set_name("BUNIT");
        *headerEntry->mutable_value() = imSummary.units().getName();
        headerEntry->set_entry_type(EntryType::STRING);
        // axes values
        casacore::Vector<casacore::String> axNames(imSummary.axisNames());
        casacore::Vector<casacore::Double> axRefPix(imSummary.referencePixels());
        casacore::Vector<casacore::Double> axRefVal(imSummary.referenceValues());
        casacore::Vector<casacore::Double> axInc(imSummary.axisIncrements());
        casacore::Vector<casacore::String> axUnits(imSummary.axisUnits());
        for (casacore::uInt i=0; i<axNames.size(); ++i) {
            casacore::String suffix(casacore::String::toString(i+1));
            // name = CTYPE
            headerEntry = extendedInfo->add_header_entries();
            headerEntry->set_name("CTYPE"+ suffix);
            headerEntry->set_entry_type(EntryType::STRING);
	    casacore::String axisName = axNames(i);
	    if (axisName == "Right Ascension") axisName = "RA";
	    if (axisName == "Declination") axisName = "DEC";
            *headerEntry->mutable_value() = axisName;
	    if (suffix=="1")
                coordinateTypeX = axisName;
	    else if (suffix=="2")
                coordinateTypeY = axisName;
            // ref val = CRVAL
            headerEntry = extendedInfo->add_header_entries();
            headerEntry->set_name("CRVAL"+ suffix);
            *headerEntry->mutable_value() = fmt::format("{}", axRefVal(i));
            headerEntry->set_entry_type(EntryType::FLOAT);
            headerEntry->set_numeric_value(imShape(i));
            // increment = CDELT
            headerEntry = extendedInfo->add_header_entries();
            headerEntry->set_name("CDELT"+ suffix);
            *headerEntry->mutable_value() = fmt::format("{}", axInc(i));
            headerEntry->set_entry_type(EntryType::FLOAT);
            headerEntry->set_numeric_value(imShape(i));
            // ref pix = CRPIX
            headerEntry = extendedInfo->add_header_entries();
            headerEntry->set_name("CRPIX"+ suffix);
            *headerEntry->mutable_value() = fmt::format("{}", axRefPix(i));
            headerEntry->set_entry_type(EntryType::FLOAT);
            headerEntry->set_numeric_value(imShape(i));
            // units = CUNIT
            headerEntry = extendedInfo->add_header_entries();
            headerEntry->set_name("CUNIT"+ suffix);
            *headerEntry->mutable_value() = axUnits(i);
            headerEntry->set_entry_type(EntryType::STRING);
        }
        // RESTFRQ
        casacore::String returnStr;
        casacore::Quantum<casacore::Double> restFreq;
        casacore::Bool ok = imSummary.restFrequency(returnStr, restFreq);
        if (ok) {
            headerEntry = extendedInfo->add_header_entries();
            headerEntry->set_name("RESTFRQ");
            casacore::Double restFreqVal(restFreq.getValue());
            *headerEntry->mutable_value() = returnStr;
            headerEntry->set_entry_type(EntryType::FLOAT);
            headerEntry->set_numeric_value(restFreqVal);
        }
        // SPECSYS
        casacore::MFrequency::Types freqTypes;
        ok = imSummary.frequencySystem(returnStr, freqTypes);
        if (ok) {
            headerEntry = extendedInfo->add_header_entries();
            headerEntry->set_name("SPECSYS");
            *headerEntry->mutable_value() = returnStr;
            headerEntry->set_entry_type(EntryType::STRING);
        }
        // telescope
        headerEntry = extendedInfo->add_header_entries();
        headerEntry->set_name("TELESCOP");
        *headerEntry->mutable_value() = imSummary.telescope();
        headerEntry->set_entry_type(EntryType::STRING);
        // observer
        headerEntry = extendedInfo->add_header_entries();
        headerEntry->set_name("OBSERVER");
        *headerEntry->mutable_value() = imSummary.observer();
        headerEntry->set_entry_type(EntryType::STRING);
        // obs date
        casacore::MEpoch epoch;
        headerEntry = extendedInfo->add_header_entries();
        headerEntry->set_name("DATE");
        *headerEntry->mutable_value() = imSummary.obsDate(epoch);
        headerEntry->set_entry_type(EntryType::STRING);

        // computed_entries
        addComputedEntries(extendedInfo, coordinateTypeX, coordinateTypeY, radeSys);
    } catch (casacore::AipsError& err) {
        if (ccImage != nullptr)
            delete ccImage;
        message = err.getMesg().c_str();
        extInfoOK = false;
    }
    if (ccImage != nullptr)
        delete ccImage;
    return extInfoOK;
}

void FileInfoLoader::addComputedEntries(CARTA::FileInfoExtended* extendedInfo, const std::string& coordinateTypeX,
    const std::string& coordinateTypeY, const std::string& radeSys) {
    // computed_entries
    auto computedEntryName = extendedInfo->add_computed_entries();

    // name
    casacore::File ccfile(m_file);
    string name(ccfile.path().baseName());
    computedEntryName->set_name("Name");
    computedEntryName->set_entry_type(EntryType::STRING);
    computedEntryName->set_value(name);

    // shape
    auto computedEntryShape = extendedInfo->add_computed_entries();
    computedEntryShape->set_name("Shape");
    computedEntryShape->set_entry_type(EntryType::STRING);
    string shapeString;
    int ndims(extendedInfo->dimensions());
    switch(ndims) {
        case 2:
            shapeString = fmt::format("[{}, {}]", extendedInfo->width(), extendedInfo->height());
            break;
        case 3:
            shapeString = fmt::format("[{}, {}, {}]", extendedInfo->width(), extendedInfo->height(),
                extendedInfo->depth());
            break;
        case 4:
            shapeString = fmt::format("[{}, {}, {}, {}]", extendedInfo->width(), extendedInfo->height(),
                extendedInfo->depth(), extendedInfo->stokes());
            break;
    }
    computedEntryShape->set_value(shapeString);

    if (coordinateTypeX.length() && coordinateTypeY.length()) {
        auto computedEntryCoordinateType = extendedInfo->add_computed_entries();
        computedEntryCoordinateType->set_name("Coordinate type");
        computedEntryCoordinateType->set_entry_type(EntryType::STRING);
        computedEntryCoordinateType->set_value(fmt::format("{}, {}", coordinateTypeX, coordinateTypeY));
    }

    if (radeSys.length()) {
        auto computedEntryCelestialFrame = extendedInfo->add_computed_entries();
        computedEntryCelestialFrame->set_name("Celestial frame");
        computedEntryCelestialFrame->set_entry_type(EntryType::STRING);
        computedEntryCelestialFrame->set_value(radeSys);
    }
}
