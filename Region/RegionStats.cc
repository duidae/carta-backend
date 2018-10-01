//# RegionStats.cc: implementation of class for calculating region statistics and histograms

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

        // Calculate min
        tbb::blocked_range2d<size_t> range(0, ncol, 0, nrow);
        float minVal = tbb::parallel_reduce(
            range,
            std::numeric_limits<float>::max(),
            [&chanMatrix](const tbb::blocked_range2d<size_t> &r, const float &init) {
                float mv = init;
                for(size_t j = r.rows().begin(); j != r.rows().end(); ++j) {
                    for(size_t i = r.cols().begin(); i != r.cols().end(); ++i) {
                        mv = std::fmin(mv, chanMatrix(i,j));
                    }
                }
                return mv;
            },
            [](float x, float y) {
                return std::min(x, y);
            });
        // Calculate max
        float maxVal = tbb::parallel_reduce(
            range,
            std::numeric_limits<float>::min(),
            [&chanMatrix](const tbb::blocked_range2d<size_t> &r, const float &init) {
                float mv = init;
                for(size_t j = r.rows().begin(); j != r.rows().end(); ++j) {
                    for(size_t i = r.cols().begin(); i != r.cols().end(); ++i) {
                        mv = std::fmax(mv, chanMatrix(i,j));
                    }
                }
                return mv;
            },
            [](float x, float y) {
                return std::max(x, y);
            });

        float binWidth = (maxVal - minVal) / numBins;
        std::vector<int> histogramBins = tbb::parallel_reduce(
            range,
            std::vector<int>(numBins, 0),
            [&binWidth, &chanMatrix, &maxVal, &minVal, &numBins](const tbb::blocked_range2d<size_t> &r, const std::vector<int> &init) {
                std::vector<int> hist(init);
                for (auto j = r.rows().begin(); j != r.rows().end(); ++j) {
                    for (auto i = r.cols().begin(); i != r.cols().end(); ++i) {
                        auto v = chanMatrix(i,j);
                        if (std::isnan(v))
                            continue;
                        int bin = std::max(std::min((int) ((v - minVal) / binWidth), numBins - 1), 0);
                        ++hist[bin];
                    }
                }
                return hist;
            },
            [&numBins](const std::vector<int> &a, const std::vector<int> &b) {
                // TBF: This could probably be done more intelligently...
                std::vector<int> c(numBins);
                auto range = tbb::blocked_range<size_t>(0, numBins);
                auto loop = [&](const tbb::blocked_range<size_t> &r) {
                    size_t beg = r.begin();
                    size_t end = r.end();
                    std::transform(&a[beg], &a[end], &b[beg], &c[beg], std::plus<int>());
                };
                tbb::parallel_for(range, loop);
                return c;
            });

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
