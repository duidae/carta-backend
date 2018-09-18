//# RegionStats.h: class for calculating region statistics and histograms

#pragma once

#include "FileLoader.h"
#include <carta-protobuf/region_histogram.pb.h>

#include <vector>
//#include <memory>
#include <cstdint>

namespace carta {

struct ChannelStats {
    float minVal;
    float maxVal;
    float mean;
    std::vector<float> percentiles;
    std::vector<float> percentileRanks;
    std::vector<int> histogramBins;
    int64_t nanCount;
};

class RegionStats {
public:
    RegionStats() {};
    ~RegionStats(){};

    bool loadStats(bool loadPercentiles, FileInfo::ImageShape& imshape, std::unique_ptr<FileLoader>& loader, std::string& message);
    CARTA::Histogram currentHistogram(casacore::Matrix<float>& chanCache, FileInfo::ImageShape& imshape, size_t chanIndex, size_t stokesIndex);

private:
    std::vector<std::vector<ChannelStats>> m_stats;
};

}
