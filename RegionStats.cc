//# RegionStats.cc: implementation of class for calculating region statistics and histograms

#include "RegionStats.h"

#include <string>
#include <cmath>

using namespace carta;
using namespace std;

bool RegionStats::loadStats(bool loadPercentiles, FileInfo::ImageShape& imshape, std::unique_ptr<FileLoader>& loader, std::string& message) {
    m_stats.resize(imshape.stokes);
    for (auto i = 0; i < imshape.stokes; i++) {
        m_stats[i].resize(imshape.depth);
    }

    //TODO: Support multiple HDUs
    if (loader->hasData(FileInfo::Data::Stats) && loader->hasData(FileInfo::Data::Stats2D)) {
        if (loader->hasData(FileInfo::Data::S2DMax)) {
            std::string hdu;
            auto &dataSet = loader->loadData(FileInfo::Data::S2DMax);
            casacore::IPosition statDims = dataSet.shape();

            // 2D cubes
            if (imshape.dimensions.size() == 2 && statDims.size() == 0) {
                casacore::Array<float> data;
                dataSet.get(data, true);
                auto it = data.begin();
                m_stats[0][0].maxVal = *it;
            } // 3D cubes
            else if (imshape.dimensions.size() == 3 && statDims.size() == 1 && statDims[0] == imshape.depth) {
                casacore::Array<float> data;
                dataSet.get(data, true);
                auto it = data.begin();
                for (auto i = 0; i < imshape.depth; ++i) {
                    m_stats[0][i].maxVal = *it++;
                }
            } // 4D cubes
            else if (imshape.dimensions.size() == 4 && statDims.size() == 2 &&
                     statDims[0] == imshape.stokes && statDims[1] == imshape.depth) {
                casacore::Array<float> data;
                dataSet.get(data, true);
                auto it = data.begin();
                for (auto i = 0; i < imshape.stokes; i++) {
                    for (auto j = 0; j < imshape.depth; j++) {
                        m_stats[i][j].maxVal = *it++;
                    }
                }
            } else {
                message = "Invalid MaxVals statistics";
                return false;
            }

        } else {
            message = "Missing MaxVals statistics";
            return false;
        }

        if (loader->hasData(FileInfo::Data::S2DMin)) {
            auto &dataSet = loader->loadData(FileInfo::Data::S2DMin);
            casacore::IPosition statDims = dataSet.shape();

            // 2D cubes
            if (imshape.dimensions.size() == 2 && statDims.size() == 0) {
                casacore::Array<float> data;
                dataSet.get(data, true);
                auto it = data.begin();
                m_stats[0][0].maxVal = *it;
            } // 3D cubes
            else if (imshape.dimensions.size() == 3 && statDims.size() == 1 && statDims[0] == imshape.depth) {
                casacore::Array<float> data;
                dataSet.get(data, true);
                auto it = data.begin();
                for (auto i = 0; i < imshape.depth; ++i) {
                    m_stats[0][i].maxVal = *it++;
                }
            } // 4D cubes
            else if (imshape.dimensions.size() == 4 && statDims.size() == 2 &&
                     statDims[0] == imshape.stokes && statDims[1] == imshape.depth) {
                casacore::Array<float> data;
                dataSet.get(data, true);
                auto it = data.begin();
                for (auto i = 0; i < imshape.stokes; i++) {
                    for (auto j = 0; j < imshape.depth; j++) {
                        m_stats[i][j].maxVal = *it++;
                    }
                }
            } else {
                message = "Invalid MinVals statistics";
                return false;
            }

        } else {
            message = "Missing MinVals statistics";
            return false;
        }

        if (loader->hasData(FileInfo::Data::S2DMean)) {
            auto &dataSet = loader->loadData(FileInfo::Data::S2DMean);
            casacore::IPosition statDims = dataSet.shape();

            // 2D cubes
            if (imshape.dimensions.size() == 2 && statDims.size() == 0) {
                casacore::Array<float> data;
                dataSet.get(data, true);
                auto it = data.begin();
                m_stats[0][0].maxVal = *it;
            } // 3D cubes
            else if (imshape.dimensions.size() == 3 && statDims.size() == 1 && statDims[0] == imshape.depth) {
                casacore::Array<float> data;
                dataSet.get(data, true);
                auto it = data.begin();
                for (auto i = 0; i < imshape.depth; ++i) {
                    m_stats[0][i].maxVal = *it++;
                }
            } // 4D cubes
            else if (imshape.dimensions.size() == 4 && statDims.size() == 2 &&
                     statDims[0] == imshape.stokes && statDims[1] == imshape.depth) {
                casacore::Array<float> data;
                dataSet.get(data, true);
                auto it = data.begin();
                for (auto i = 0; i < imshape.stokes; i++) {
                    for (auto j = 0; j < imshape.depth; j++) {
                        m_stats[i][j].maxVal = *it++;
                    }
                }
            } else {
                message = "Invalid Means statistics";
                return false;
            }
        } else {
            message = "Missing Means statistics";
            return false;
        }

        if (loader->hasData(FileInfo::Data::S2DNans)) {
            auto &dataSet = loader->loadData(FileInfo::Data::S2DNans);
            casacore::IPosition statDims = dataSet.shape();

            // 2D cubes
            if (imshape.dimensions.size() == 2 && statDims.size() == 0) {
                casacore::Array<float> data;
                dataSet.get(data, true);
                auto it = data.begin();
                m_stats[0][0].maxVal = *it;
            } // 3D cubes
            else if (imshape.dimensions.size() == 3 && statDims.size() == 1 && statDims[0] == imshape.depth) {
                casacore::Array<float> data;
                dataSet.get(data, true);
                auto it = data.begin();
                for (auto i = 0; i < imshape.depth; ++i) {
                    m_stats[0][i].maxVal = *it++;
                }
            } // 4D cubes
            else if (imshape.dimensions.size() == 4 && statDims.size() == 2 &&
                     statDims[0] == imshape.stokes && statDims[1] == imshape.depth) {
                casacore::Array<float> data;
                dataSet.get(data, true);
                auto it = data.begin();
                for (auto i = 0; i < imshape.stokes; i++) {
                    for (auto j = 0; j < imshape.depth; j++) {
                        m_stats[i][j].maxVal = *it++;
                    }
                }
            } else {
                message = "Invalid NaNCounts statistics";
                return false;
            }
        } else {
            message = "Missing NaNCounts statistics";
            return false;
        }

        if (loader->hasData(FileInfo::Data::S2DHist)) {
            auto &dataSet = loader->loadData(FileInfo::Data::S2DHist);
            casacore::IPosition statDims = dataSet.shape();
            auto numBins = statDims[2];

            // 2D cubes
            if (imshape.dimensions.size() == 2 && statDims.size() == 0) {
                casacore::Array<float> data;
                dataSet.get(data, true);
                auto it = data.begin();
                std::copy(data.begin(), data.end(),
                          std::back_inserter(m_stats[0][0].histogramBins));
            } // 3D cubes
            else if (imshape.dimensions.size() == 3 && statDims.size() == 1 && statDims[0] == imshape.depth) {
                casacore::Array<float> data;
                dataSet.get(data, true);
                auto it = data.begin();
                for (auto i = 0; i < imshape.depth; ++i) {
                    m_stats[0][i].histogramBins.resize(numBins);
                    for (auto j = 0; j < numBins; j++) {
                        m_stats[0][i].histogramBins[j] = *it++;
                    }
                }
            } // 4D cubes
            else if (imshape.dimensions.size() == 4 && statDims.size() == 2 &&
                     statDims[0] == imshape.stokes && statDims[1] == imshape.depth) {
                casacore::Array<float> data;
                dataSet.get(data, true);
                auto it = data.begin();
                for (auto i = 0; i < imshape.stokes; i++) {
                    for (auto j = 0; j < imshape.depth; j++) {
                        auto& stats = m_stats[i][j];
                        stats.histogramBins.resize(numBins);
                        for (auto k = 0; k < numBins; k++) {
                            stats.histogramBins[k] = *it++;
                        }
                    }
                }
            } else {
                message = "Invalid histogram statistics";
                return false;
            }

        } else {
            message = "Missing Histograms group";
            return false;
        }

        if (loadPercentiles) {
            if (loader->hasData(FileInfo::Data::S2DPercent) &&
                loader->hasData(FileInfo::Data::Ranks)) {
                auto &dataSetPercentiles = loader->loadData(FileInfo::Data::S2DPercent);
                auto &dataSetPercentilesRank = loader->loadData(FileInfo::Data::Ranks);

                casacore::IPosition dims = dataSetPercentiles.shape();
                casacore::IPosition dimsRanks = dataSetPercentilesRank.shape();

                auto numRanks = dimsRanks[0];
                casacore::Vector<float> ranks(numRanks);
                dataSetPercentilesRank.get(ranks, false);

                if (imshape.dimensions == 2 && dims.size() == 1 && dims[0] == numRanks) {
                    casacore::Vector<float> vals(numRanks);
                    dataSetPercentiles.get(vals, true);
                    vals.tovector(m_stats[0][0].percentiles);
                    ranks.tovector(m_stats[0][0].percentileRanks);
                }
                    // 3D cubes
                else if (imshape.dimensions == 3 && dims.size() == 2 && dims[0] == imshape.depth && dims[1] == numRanks) {
                    casacore::Matrix<float> vals(imshape.depth, numRanks);
                    dataSetPercentiles.get(vals, false);

                    for (auto i = 0; i < imshape.depth; i++) {
                        ranks.tovector(m_stats[0][i].percentileRanks);
                        m_stats[0][i].percentiles.resize(numRanks);
                        for (auto j = 0; j < numRanks; j++) {
                            m_stats[0][i].percentiles[j] = vals(i,j);
                        }
                    }
                }
                    // 4D cubes
                else if (imshape.dimensions == 4 && dims.size() == 3 && dims[0] == imshape.stokes && dims[1] == imshape.depth && dims[2] == numRanks) {
                    casacore::Cube<float> vals(imshape.stokes, imshape.depth, numRanks);
                    dataSetPercentiles.get(vals, false);

                    for (auto i = 0; i < imshape.stokes; i++) {
                        for (auto j = 0; j < imshape.depth; j++) {
                            auto& stats = m_stats[i][j];
                            stats.percentiles.resize(numRanks);
                            for (auto k = 0; k < numRanks; k++) {
                                stats.percentiles[k] = vals(i,j,k);
                            }
                            ranks.tovector(stats.percentileRanks);
                        }
                    }
                } else {
                    message = "Missing Percentiles datasets";
                    return false;
                }
            } else {
                message = "Missing Percentiles group";
                return false;
            }
        }
    } else {
        message = "Missing Statistics group";
        return false;
    }

    return true;
}

CARTA::Histogram RegionStats::currentHistogram(casacore::Matrix<float>& chanCache, FileInfo::ImageShape& imshape, size_t chanIndex, size_t stokesIndex) {
    CARTA::Histogram histogram;
    histogram.set_channel(chanIndex);

    // Calculate histogram if it hasn't been stored
    if (m_stats[stokesIndex][chanIndex].histogramBins.empty()) {
        float minVal = chanCache(0,0);
        float maxVal = chanCache(0,0);
        float sum = 0.0f;
        int count = 0;
        for (auto i = 0; i < imshape.height; i++) {
            for (auto j = 0; j < imshape.width; ++j) {
                auto v = chanCache(j, i);
                minVal = fmin(minVal, v);
                maxVal = fmax(maxVal, v);
                sum += isnan(v) ? 0.0 : v;
                count += isnan(v) ? 0 : 1;
            }
        }

        ChannelStats& stats = m_stats[stokesIndex][chanIndex];
        stats.minVal = minVal;
        stats.maxVal = maxVal;
        stats.nanCount = count;
        stats.mean = sum / max(count, 1);
        int N = int(max(sqrt(imshape.width * imshape.height), 2.0));
        stats.histogramBins.resize(N, 0);
        float binWidth = (stats.maxVal - stats.minVal) / N;

        for (auto i = 0; i < imshape.height; i++) {
            for (auto j = 0; j < imshape.width; ++j) {
                auto v = chanCache(j,i);
                if (isnan(v)) {
                    continue;
                }
                int bin = max(min((int) ((v - minVal) / binWidth), N - 1), 0);
                stats.histogramBins[bin]++;
            }
        }
    }

    // Create histogram object
    auto& currentStats = m_stats[stokesIndex][chanIndex];
    histogram.set_num_bins(currentStats.histogramBins.size());
    histogram.set_bin_width((currentStats.maxVal - currentStats.minVal) / currentStats.histogramBins.size());
    histogram.set_first_bin_center(currentStats.minVal + histogram.bin_width() / 2.0);
    *histogram.mutable_bins() = {currentStats.histogramBins.begin(), currentStats.histogramBins.end()};

    return histogram;
}
