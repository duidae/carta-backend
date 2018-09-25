//# RegionStats.cc: implementation of class for calculating region statistics and histograms

#include "RegionStats.h"

#include <cmath>
#include <limits>
#include <casacore/casa/Arrays/ArrayMath.h>

using namespace carta;
using namespace std;

bool RegionStats::setHistogramRequirements(const std::vector<CARTA::SetHistogramRequirements_HistogramConfig>& histogramReqs) {
    m_configs = histogramReqs;
    return true;
}

size_t RegionStats::numHistogramConfigs() {
    return m_configs.size();
}

CARTA::SetHistogramRequirements_HistogramConfig RegionStats::getHistogramConfig(int histogramIndex) {
    return m_configs[histogramIndex];
}

void RegionStats::fillHistogram(CARTA::Histogram* histogram, const casacore::Matrix<float>& chanMatrix,
        const size_t chanIndex, const size_t stokesIndex) {
    // stored?
    if (m_channelHistograms.count(chanIndex) && m_stokes==stokesIndex) {
        *histogram = m_channelHistograms[chanIndex];
    } else {
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

        // fill histogram
        histogram->set_channel(chanIndex);
        histogram->set_num_bins(numBins);
        histogram->set_bin_width(binWidth);
        histogram->set_first_bin_center(minVal + (binWidth / 2.0));
        *histogram->mutable_bins() = {histogramBins.begin(), histogramBins.end()};

        m_channelHistograms[chanIndex] = *histogram;
    }
}
