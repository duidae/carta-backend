#pragma once
#include <vector>
#include <map>
#include <string>
#include <memory>

#include <carta-protobuf/defs.pb.h>
#include "ImageData/FileLoader.h"
#include "ImageData/RegionStats.h"

class Frame {

private:
    // setup
    std::string uuid;
    bool valid;
    std::string filename;

    // data components
    std::unique_ptr<carta::FileLoader> loader;
    std::unique_ptr<carta::RegionStats> stats;

    // data shape
    carta::FileInfo::ImageShape imgShape;

    // image view settings
    CARTA::ImageBounds bounds;
    int mip;

    // channel, stokes
    size_t channelIndex;
    size_t stokesIndex;
    casacore::Matrix<float> channelCache;

public:
    Frame(const std::string& uuidString, const std::string& filename, const std::string& hdu, int defaultChannel = 0);
    bool setBounds(CARTA::ImageBounds imageBounds, int newMip);
    bool setChannels(size_t newChannel, size_t newStokes);
    bool loadStats(bool loadPercentiles = false);
    bool isValid();

    int currentStokes();
    int currentChannel();
    CARTA::ImageBounds currentBounds();
    int currentMip();

    // data
    std::vector<float> getImageData(bool meanFilter = true);
    CARTA::Histogram currentHistogram();
};
