#pragma once

#include "FileLoader.h"
#include <H5Cpp.h>
#include <unordered_map>

class HDF5Loader : public FileLoader {
public:
    void openFile(const std::string &file) override;
    bool hasData(const std::string &data) const override;
    image_ptr loadData(const std::string &data) override;

private:
    H5::H5File file;
    H5::Group hduGroup;
    std::unordered_map<std::string, H5::DataSet> dataSets;
};

void HDF5Loader::openFile(const std::string &filename) {
    try {
        file = H5::H5File(filename, H5F_ACC_RDONLY);
        hduGroup = file.openGroup("0");
    }
    catch(const H5::FileIException &err) {
        //log(, "Problem loading file {}", filename);
    }
}

bool HDF5Loader::hasData(const std::string &data) const {
    return H5Lexists(hduGroup.getId(), data.c_str(), 0);
}

H5::DataSet HDF5Loader::loadData(const std::string &data) {
    if(dataSets.find(data) == dataSets.end()) {
        dataSets[data] = hduGroup.openDataSet(data);
    }
    return dataSets[data];
}
