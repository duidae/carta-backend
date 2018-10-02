//# RegionStats.cc: implementation of class for calculating region statistics and histograms

#include "Histogram.h"
#include "MinMax.h"
#include "RegionStats.h"

#include <chrono>
#include <cmath>
#include <fmt/format.h>
#include <limits>
#include <tbb/blocked_range.h>
#include <tbb/blocked_range2d.h>
#include <tbb/parallel_for.h>
#include <tbb/parallel_reduce.h>
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
    CARTA::SetHistogramRequirements_HistogramConfig config;
    if (histogramIndex < m_configs.size())
        config = m_configs[histogramIndex];
    return config;
}

void RegionStats::fillHistogram(CARTA::Histogram* histogram, const casacore::Matrix<float>& chanMatrix,
        const size_t chanIndex, const size_t stokesIndex) {
    // stored?
    if (m_channelHistograms.count(chanIndex) && m_stokes==stokesIndex) {
        *histogram = m_channelHistograms[chanIndex];
    } else {
        // create histogram for this channel, stokes
        m_stokes = stokesIndex;

        // auto tStart = std::chrono::high_resolution_clock::now();

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

        tbb::blocked_range2d<size_t> range(0, ncol, 0, nrow);
        MinMax<float> mm(chanMatrix);
        tbb::parallel_reduce(range, mm);
        float minVal, maxVal;
        std::tie(minVal, maxVal) = mm.getMinMax();

        Histogram hist(numBins, minVal, maxVal, chanMatrix);
        tbb::parallel_reduce(range, hist);
        std::vector<int> histogramBins = hist.getHistogram();
        float binWidth = hist.getBinWidth();

        // auto tEnd = std::chrono::high_resolution_clock::now();
        // auto dt = std::chrono::duration_cast<std::chrono::microseconds>(tEnd - tStart).count();
        // fmt::print("histogram loops took {}ms\n", dt/1e3);

        // fill histogram
        histogram->set_channel(chanIndex);
        histogram->set_num_bins(numBins);
        histogram->set_bin_width(binWidth);
        histogram->set_first_bin_center(minVal + (binWidth / 2.0));
        *histogram->mutable_bins() = {histogramBins.begin(), histogramBins.end()};

        m_channelHistograms[chanIndex] = *histogram;
    }
}
