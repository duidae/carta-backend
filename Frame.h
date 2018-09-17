#pragma once
#include <vector>
#include <map>
#include <string>
#include <memory>
#include <carta-protobuf/defs.pb.h>
#include "FileLoader.h"

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
    // setup
    std::string uuid;
    bool valid;
    std::string filename;
    std::unique_ptr<carta::FileLoader> loader;

    // data shape
    casacore::IPosition dimensions;
    size_t width;
    size_t height;
    size_t depth;
    size_t stokes;

    // image view settings
    CARTA::ImageBounds bounds;
    int mip;
    bool boundsDefined;

    // channel, stokes
    size_t channelIndex;
    size_t stokesIndex;
    casacore::Matrix<float> channelCache;

    // statistics
    std::vector<std::vector<ChannelStats>> channelStats;

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
    bool boundsSet();

    // data
    std::vector<float> getImageData(bool meanFilter = true);

    // statistics
    CARTA::Histogram currentHistogram();
};
