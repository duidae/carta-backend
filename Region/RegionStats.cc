//# RegionStats.cc: implementation of class for calculating region statistics and histograms

#include "RegionStats.h"

#include <cmath>
#include <limits>
#include <casacore/casa/Arrays/ArrayMath.h>

using namespace carta;
using namespace std;

void RegionStats::setHistogramRequirements(const std::vector<CARTA::SetHistogramRequirements_HistogramConfig>& histogramReqs) {
    m_configs = histogramReqs;
}

CARTA::SetHistogramRequirements_HistogramConfig RegionStats::getHistogramRequirement(int histogramIndex) {
    return m_configs[histogramIndex];
}

size_t RegionStats::numHistogramReqs() {
    return m_configs.size();
}

CARTA::Histogram RegionStats::getHistogram(const casacore::Matrix<float>& chanMatrix,
    const size_t chanIndex, const size_t stokesIndex) {

    if (!m_channelHistograms.empty() && m_channelHistograms.count(chanIndex) && m_stokes==stokesIndex) {
	return m_channelHistograms[chanIndex];
    }	
    // create histogram for this channel, stokes
    m_stokes = stokesIndex;

    size_t nrow(chanMatrix.nrow()), ncol(chanMatrix.ncolumn());
    int numBins(0);
    for (auto& chanConfig : m_configs) {
        if (chanConfig.channel()==chanIndex) {
            numBins = chanConfig.num_bins();
	    break;
        }
    }
    if (numBins < 0) 
        numBins = int(max(sqrt(nrow * ncol), 2.0));
    std::vector<int> histogramBins(numBins);

    // find max, min, bin width; values could be nan
    float minVal(std::numeric_limits<float>::max()), maxVal(std::numeric_limits<float>::min());
    for (auto i = 0; i < ncol; i++) {
        for (auto j = 0; j < nrow; ++j) {
            auto v = chanMatrix(j,i);
            if (std::isnan(v))
                continue;
	    minVal = std::fmin(minVal, v);
	    maxVal = std::fmax(maxVal, v);
        }
    }
    float binWidth = (maxVal - minVal) / numBins;

    for (auto i = 0; i < ncol; i++) {
        for (auto j = 0; j < nrow; ++j) {
            auto v = chanMatrix(j,i);
            if (std::isnan(v))
                continue;
            int bin = std::max(std::min((int) ((v - minVal) / binWidth), numBins - 1), 0);
            histogramBins[bin]++;
        }
    }

    // Create histogram object
    CARTA::Histogram chanHistogram;
    chanHistogram.set_channel(chanIndex);
    chanHistogram.set_num_bins(numBins);

    chanHistogram.set_bin_width(binWidth);
    chanHistogram.set_first_bin_center(minVal + (binWidth / 2.0));
    *chanHistogram.mutable_bins() = {histogramBins.begin(), histogramBins.end()};

    m_channelHistograms[chanIndex] = chanHistogram;
    return chanHistogram;
}
