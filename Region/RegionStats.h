//# RegionStats.h: class for calculating region statistics and histograms

#pragma once

#include <carta-protobuf/defs.pb.h> // Histogram
#include <carta-protobuf/region_requirements.pb.h>  // HistogramConfig
#include <casacore/casa/Arrays/Matrix.h>
#include <casacore/casa/Arrays/IPosition.h>

#include <vector>
#include <unordered_map>

namespace carta {

class RegionStats {

public:
    void setHistogramRequirements(const std::vector<CARTA::SetHistogramRequirements_HistogramConfig>& histogramReqs);
    CARTA::SetHistogramRequirements_HistogramConfig getHistogramRequirement(int histogramIndex);
    size_t numHistogramReqs();

    CARTA::Histogram getHistogram(const casacore::Matrix<float>& chanMatrix,
        const size_t chanIndex, const size_t stokesIndex);

private:
    size_t m_stokes;
    std::unordered_map<int, CARTA::Histogram> m_channelHistograms;
    std::vector<CARTA::SetHistogramRequirements_HistogramConfig> m_configs;
};

}
