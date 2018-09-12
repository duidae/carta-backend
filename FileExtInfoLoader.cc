//# FileExtInfoLoader.cc: fill FileInfoExtended for all supported file types

#include "FileExtInfoLoader.h"

#include <fmt/format.h>
#include <H5Cpp.h>
#include <H5File.h>
#include <casacore/fits/FITS/FITSTable.h>
#include <casacore/images/Images/PagedImage.h>
#include <casacore/images/Images/FITSImage.h>
#include <casacore/images/Images/MIRIADImage.h>
#include <casacore/images/Images/ImageSummary.h>

using namespace std;
using namespace CARTA;
using namespace H5;

FileExtInfoLoader::FileExtInfoLoader(const string& filename, const string& hdu) :
    m_file(filename), m_hdu(hdu) {
    m_type = fileType(filename);
}

casacore::ImageOpener::ImageTypes
FileExtInfoLoader::fileType(const std::string &file) {
    return casacore::ImageOpener::imageType(file);
}

bool FileExtInfoLoader::fillFileExtInfo(FileInfoExtended* extInfo, string& message) {
    bool extInfoOK(false);
    switch(m_type) {
    case casacore::ImageOpener::AIPSPP:
        extInfoOK = fillCASAExtFileInfo(extInfo, message);
        break;
    case casacore::ImageOpener::FITS:
        extInfoOK = fillFITSExtFileInfo(extInfo, message);
        break;
    case casacore::ImageOpener::HDF5:
        extInfoOK = fillHdf5ExtFileInfo(extInfo, message);
	break;
    case casacore::ImageOpener::MIRIAD:
        extInfoOK = fillCASAExtFileInfo(extInfo, message);
        break;
    default:
        break;
    }
    return extInfoOK;
}

bool FileExtInfoLoader::fillHdf5ExtFileInfo(FileInfoExtended* extendedInfo, string& message) {
    // Add extended info for HDF5 file
    bool extInfoOK(true);
    H5File file(m_file, H5F_ACC_RDONLY);
    bool hasHDU;
    if (m_hdu.length()) {
        hasHDU = H5Lexists(file.getId(), m_hdu.c_str(), 0);
    } else {
        auto N = file.getNumObjs();
        hasHDU = false;
        for (auto i = 0; i < N; i++) {
            if (file.getObjTypeByIdx(i) == H5G_GROUP) {
                m_hdu = file.getObjnameByIdx(i);
                hasHDU = true;
                break;
            }
        }
    }

    if (hasHDU) {
        H5::Group topLevelGroup = file.openGroup(m_hdu);
        if (H5Lexists(topLevelGroup.getId(), "DATA", 0)) {
            DataSet dataSet = topLevelGroup.openDataSet("DATA");
            vector<hsize_t> dims(dataSet.getSpace().getSimpleExtentNdims(), 0);
            dataSet.getSpace().getSimpleExtentDims(dims.data(), NULL);
            uint32_t N = dims.size();
            extendedInfo->set_dimensions(N);
            if (N < 2 || N > 4) {
                message = "Image must be 2D, 3D or 4D.";
                return false;
            }
            extendedInfo->set_width(dims[N - 1]);
            extendedInfo->set_height(dims[N - 2]);
            extendedInfo->set_depth((N > 2) ? dims[N - 3] : 1);
            extendedInfo->set_stokes((N > 3) ? dims[N - 4] : 1);

            H5O_info_t groupInfo;
            H5Oget_info(topLevelGroup.getId(), &groupInfo);
            for (auto i = 0; i < groupInfo.num_attrs; i++) {
                Attribute attr = topLevelGroup.openAttribute(i);
                hid_t attrTypeId = H5Aget_type(attr.getId());
                auto headerEntry = extendedInfo->add_header_entries();
                headerEntry->set_name(attr.getName());

                auto typeClass = H5Tget_class(attrTypeId);
                if (typeClass == H5T_STRING) {
                    attr.read(attr.getStrType(), *headerEntry->mutable_value());
                    headerEntry->set_entry_type(EntryType::STRING);
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

bool FileExtInfoLoader::fillFITSExtFileInfo(FileInfoExtended* extendedInfo, string& message) {
    bool extInfoOK(true);
    try {
        // convert string hdu to unsigned int
        casacore::String ccHdu(m_hdu);
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
    } catch (casacore::AipsError& err) {
        message = err.getMesg();
        extInfoOK = false;
    }
    return extInfoOK;
}

bool FileExtInfoLoader::fillCASAExtFileInfo(FileInfoExtended* extendedInfo, string& message) {
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
	// set dimensions
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
	for (casacore::uInt i=0; i<dim; ++i) {
            casacore::String suffix(casacore::String::toString(i+1));
	    // name = CTYPE
            headerEntry = extendedInfo->add_header_entries();
            headerEntry->set_name("CTYPE"+ suffix);
            *headerEntry->mutable_value() = axNames(i);
            headerEntry->set_entry_type(EntryType::STRING);
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
