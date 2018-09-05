#pragma once

#include "FileLoader.h"
#include <casacore/lattices/Lattices/HDF5Lattice.h>
#include <string>
#include <unordered_map>

class HDF5Loader : public FileLoader {
public:
    void openFile(const std::string &file) override;
    bool hasData(const std::string &data) const override;
    image_ref loadData(const std::string &data) override;

private:
    std::string file;
    std::unordered_map<std::string, casacore::HDF5Lattice<float>> dataSets;
};

void HDF5Loader::openFile(const std::string &filename) {
    file = filename;
}

bool HDF5Loader::hasData(const std::string &data) const {
    std::string parent = "main";
    auto it = dataSets.find(parent);
    if(it == dataSets.end()) return false;
    auto group_ptr = it->second.group();
    return casacore::HDF5Group::exists(*group_ptr, data);
}

typename HDF5Loader::image_ref HDF5Loader::loadData(const std::string &data) {
    if(dataSets.find(data) == dataSets.end()) {
        dataSets.emplace(data, casacore::HDF5Lattice<float>(file, data, "0"));
    }
    return dataSets[data];
}
