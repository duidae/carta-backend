#pragma once
#include <vector>
#include <map>
#include <string>
#include <memory>
#include <carta-protobuf/defs.pb.h>
#include "FileLoader.h"
#include <casacore/images/Images/ImageInterface.h>

struct ChannelStats {
    float minVal;
    float maxVal;
    float mean;
    std::vector<float> percentiles;
    std::vector<float> percentileRanks;
    std::vector<int> histogramBins;
    int64_t nanCount;
};

class Frame {
private:
    bool valid;
    size_t channelIndex;
    size_t stokesIndex;
    std::string uuid;
    std::string filename;
    std::string unit;
    size_t width;
    size_t height;
    size_t depth;
    size_t stokes;
    casacore::IPosition dimensions;
    CARTA::ImageBounds bounds;
    int mip;
    casacore::Matrix<float> channelCache;
    std::vector<float> zProfileCache;
    std::vector<int> zProfileCoords;
    std::vector<std::vector<ChannelStats>> channelStats;
    std::unique_ptr<carta::FileLoader> loader;
    std::unique_ptr<casacore::ImageInterface<float>> image;

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

    std::vector<float> getImageData(bool meanFilter = true);
    CARTA::Histogram currentHistogram();
};
