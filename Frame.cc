#include "Frame.h"
#include <cmath>
#include "util.h"

using namespace std;

Frame::Frame(const string& uuidString, const string& filename, const string& hdu, int defaultChannel)
    : uuid(uuidString),
      valid(true),
      filename(filename),
      loader(FileLoader::getLoader(filename)) {
    try {
        loader->openFile(filename);
        auto &dataSet = loader->loadData("DATA");

        dimensions = dataSet.shape();
        size_t ndims = dimensions.size();

        if (ndims < 2 || ndims > 4) {
            log(uuid, "Problem loading file {}: Image must be 2D, 3D or 4D.", filename);
            valid = false;
            return;
        }

        log(uuid, "Opening image with dimensions: {}", dimensions);
        // string axesInfo = fmt::format("Opening image with dimensions: {}", dimensions);
        // sendLogEvent(axesInfo, {"file"}, CARTA::ErrorSeverity::DEBUG);

        // TBD: use casacore::ImageInterface to get axes
        width = dimensions[0];
        height = dimensions[1];
        depth = (ndims > 2) ? dimensions[2] : 1;
        stokes = (ndims > 3) ? dimensions[3] : 1;

        loadStats(false);

        // Swizzled data loaded if it exists. Used for Z-profiles and region stats
        if (loader->hasData("SwizzledData")) {
            if (dimensions == 3 && loader->hasData("SwizzledData/ZYX")) {
                auto &dataSetSwizzled = loader->loadData("SwizzledData/ZYX");
                casacore::IPosition swizzledDims = dataSetSwizzled.shape();
                if (swizzledDims.size() != 3 || swizzledDims[0] != dimensions[2]) {
                    log(uuid, "Invalid swizzled data set in file {}, ignoring.", filename);
                } else {
                    log(uuid, "Found valid swizzled data set in file {}.", filename);
                }
            } else if (dimensions == 4 && loader->hasData("SwizzledData/ZYXW")) {
                auto &dataSetSwizzled = loader->loadData("SwizzledData/ZYXW");
                casacore::IPosition swizzledDims = dataSetSwizzled.shape();
                if (swizzledDims.size() != 4 || swizzledDims[1] != dimensions[3]) {
                    log(uuid, "Invalid swizzled data set in file {}, ignoring.", filename);
                } else {
                    log(uuid, "Found valid swizzled data set in file {}.", filename);
                }
            } else {
                log(uuid, "File {} missing optional swizzled data set, using fallback calculation.", filename);
            }
        } else {
            log(uuid, "File {} missing optional swizzled data set, using fallback calculation.", filename);
        }
        valid = setChannels(defaultChannel, 0);
    }
    //TBD: figure out what exceptions need to caught, if any
    catch (...) {
        log(uuid, "Problem loading file {}", filename);
        valid = false;
    }
}

bool Frame::isValid() {
    return valid;
}

bool Frame::setChannels(size_t newChannel, size_t newStokes) {
    if (!valid) {
        log(uuid, "No file loaded");
        return false;
    } else if (newChannel < 0 || newChannel >= depth || newStokes < 0 || newStokes >= stokes) {
        log(uuid, "Channel {} (stokes {}) is invalid in file {}", newChannel, newStokes, filename);
        return false;
    }

    casacore::IPosition count(2, width, height);
    casacore::IPosition start(2, 0, 0);
    if(dimensions.size() == 3) {
        count.append(casacore::IPosition(1, 1));
        start.append(casacore::IPosition(1, newChannel));
    } else if(dimensions == 4) {
        count.append(casacore::IPosition(2, 1, 1));
        start.append(casacore::IPosition(2, newStokes, newChannel));
    }
    casacore::Slicer section(start, count);
    casacore::Array<float> tmp;
    loader->loadData("DATA").getSlice(tmp, section, true);
    channelCache.reference(tmp);

    stokesIndex = newStokes;
    channelIndex = newChannel;
    //updateHistogram();
    return true;
}

bool Frame::setBounds(CARTA::ImageBounds imageBounds, int newMip) {
    if (!valid) {
        return false;
    }

    const int x = imageBounds.x_min();
    const int y = imageBounds.y_min();
    const int reqHeight = imageBounds.y_max() - imageBounds.y_min();
    const int reqWidth = imageBounds.x_max() - imageBounds.x_min();

    if (height < y + reqHeight || width < x + reqWidth) {
        return false;
    }

    bounds = imageBounds;
    mip = newMip;
    return true;
}

bool Frame::loadStats(bool loadPercentiles) {
    if (!valid) {
        log(uuid, "No file loaded");
        return false;
    }

    channelStats.resize(stokes);
    for (auto i = 0; i < stokes; i++) {
        channelStats[i].resize(depth);
    }

    //TODO: Support multiple HDUs
    if (loader->hasData("Statistics") && loader->hasData("Statistics/XY")) {
        if (loader->hasData("Statistics/XY/MAX")) {
            auto &dataSet = loader->loadData("Statistics/XY/MAX");
            casacore::IPosition statDims = dataSet.shape();

            // 2D cubes
            if (dimensions.size() == 2 && statDims.size() == 0) {
                casacore::Array<float> data;
                dataSet.get(data, true);
                auto it = data.begin();
                channelStats[0][0].maxVal = *it;
            } // 3D cubes
            else if (dimensions.size() == 3 && statDims.size() == 1 && statDims[0] == depth) {
                casacore::Array<float> data;
                dataSet.get(data, true);
                auto it = data.begin();
                for (auto i = 0; i < depth; ++i) {
                    channelStats[0][i].maxVal = *it++;
                }
            } // 4D cubes
            else if (dimensions.size() == 4 && statDims.size() == 2 &&
                     statDims[0] == stokes && statDims[1] == depth) {
                casacore::Array<float> data;
                dataSet.get(data, true);
                auto it = data.begin();
                for (auto i = 0; i < stokes; i++) {
                    for (auto j = 0; j < depth; j++) {
                        channelStats[i][j].maxVal = *it++;
                    }
                }
            } else {
                log(uuid, "Invalid MaxVals statistics");
                return false;
            }

        } else {
            log(uuid, "Missing MaxVals statistics");
            return false;
        }

        if (loader->hasData("Statistics/XY/MIN")) {
            auto &dataSet = loader->loadData("Statistics/XY/MIN");
            casacore::IPosition statDims = dataSet.shape();

            // 2D cubes
            if (dimensions.size() == 2 && statDims.size() == 0) {
                casacore::Array<float> data;
                dataSet.get(data, true);
                auto it = data.begin();
                channelStats[0][0].maxVal = *it;
            } // 3D cubes
            else if (dimensions.size() == 3 && statDims.size() == 1 && statDims[0] == depth) {
                casacore::Array<float> data;
                dataSet.get(data, true);
                auto it = data.begin();
                for (auto i = 0; i < depth; ++i) {
                    channelStats[0][i].maxVal = *it++;
                }
            } // 4D cubes
            else if (dimensions.size() == 4 && statDims.size() == 2 &&
                     statDims[0] == stokes && statDims[1] == depth) {
                casacore::Array<float> data;
                dataSet.get(data, true);
                auto it = data.begin();
                for (auto i = 0; i < stokes; i++) {
                    for (auto j = 0; j < depth; j++) {
                        channelStats[i][j].maxVal = *it++;
                    }
                }
            } else {
                log(uuid, "Invalid MinVals statistics");
                return false;
            }

        } else {
            log(uuid, "Missing MinVals statistics");
            return false;
        }

        if (loader->hasData("Statistics/XY/MEAN")) {
            auto &dataSet = loader->loadData("Statistics/XY/MEAN");
            casacore::IPosition statDims = dataSet.shape();

            // 2D cubes
            if (dimensions.size() == 2 && statDims.size() == 0) {
                casacore::Array<float> data;
                dataSet.get(data, true);
                auto it = data.begin();
                channelStats[0][0].maxVal = *it;
            } // 3D cubes
            else if (dimensions.size() == 3 && statDims.size() == 1 && statDims[0] == depth) {
                casacore::Array<float> data;
                dataSet.get(data, true);
                auto it = data.begin();
                for (auto i = 0; i < depth; ++i) {
                    channelStats[0][i].maxVal = *it++;
                }
            } // 4D cubes
            else if (dimensions.size() == 4 && statDims.size() == 2 &&
                     statDims[0] == stokes && statDims[1] == depth) {
                casacore::Array<float> data;
                dataSet.get(data, true);
                auto it = data.begin();
                for (auto i = 0; i < stokes; i++) {
                    for (auto j = 0; j < depth; j++) {
                        channelStats[i][j].maxVal = *it++;
                    }
                }
            } else {
                log(uuid, "Invalid Means statistics");
                return false;
            }
        } else {
            log(uuid, "Missing Means statistics");
            return false;
        }

        if (loader->hasData("Statistics/XY/NAN_COUNT")) {
            auto &dataSet = loader->loadData("Statistics/XY/NAN_COUNT");
            casacore::IPosition statDims = dataSet.shape();

            // 2D cubes
            if (dimensions.size() == 2 && statDims.size() == 0) {
                casacore::Array<float> data;
                dataSet.get(data, true);
                auto it = data.begin();
                channelStats[0][0].maxVal = *it;
            } // 3D cubes
            else if (dimensions.size() == 3 && statDims.size() == 1 && statDims[0] == depth) {
                casacore::Array<float> data;
                dataSet.get(data, true);
                auto it = data.begin();
                for (auto i = 0; i < depth; ++i) {
                    channelStats[0][i].maxVal = *it++;
                }
            } // 4D cubes
            else if (dimensions.size() == 4 && statDims.size() == 2 &&
                     statDims[0] == stokes && statDims[1] == depth) {
                casacore::Array<float> data;
                dataSet.get(data, true);
                auto it = data.begin();
                for (auto i = 0; i < stokes; i++) {
                    for (auto j = 0; j < depth; j++) {
                        channelStats[i][j].maxVal = *it++;
                    }
                }
            } else {
                log(uuid, "Invalid NaNCounts statistics");
                return false;
            }
        } else {
            log(uuid, "Missing NaNCounts statistics");
            return false;
        }

        if (loader->hasData("Statistics/XY/HISTOGRAM")) {
            auto &dataSet = loader->loadData("Statistics/XY/HISTOGRAM");
            casacore::IPosition statDims = dataSet.shape();
            auto numBins = statDims[2];

            // 2D cubes
            if (dimensions.size() == 2 && statDims.size() == 0) {
                casacore::Array<float> data;
                dataSet.get(data, true);
                auto it = data.begin();
                std::copy(data.begin(), data.end(),
                          std::back_inserter(channelStats[0][0].histogramBins));
            } // 3D cubes
            else if (dimensions.size() == 3 && statDims.size() == 1 && statDims[0] == depth) {
                casacore::Array<float> data;
                dataSet.get(data, true);
                auto it = data.begin();
                for (auto i = 0; i < depth; ++i) {
                    channelStats[0][i].histogramBins.resize(numBins);
                    for (auto j = 0; j < numBins; j++) {
                        channelStats[0][i].histogramBins[j] = *it++;
                    }
                }
            } // 4D cubes
            else if (dimensions.size() == 4 && statDims.size() == 2 &&
                     statDims[0] == stokes && statDims[1] == depth) {
                casacore::Array<float> data;
                dataSet.get(data, true);
                auto it = data.begin();
                for (auto i = 0; i < stokes; i++) {
                    for (auto j = 0; j < depth; j++) {
                        auto& stats = channelStats[i][j];
                        stats.histogramBins.resize(numBins);
                        for (auto k = 0; k < numBins; k++) {
                            stats.histogramBins[k] = *it++;
                        }
                    }
                }
            } else {
                log(uuid, "Invalid histogram statistics");
                return false;
            }

        } else {
            log(uuid, "Missing Histograms group");
            return false;
        }

        if (loadPercentiles) {
            if (loader->hasData("Statistics/XY/PERCENTILES") &&
                loader->hasData("PERCENTILE_RANKS")) {
                auto &dataSetPercentiles = loader->loadData("Statistics/XY/PERCENTILES");
                auto &dataSetPercentilesRank = loader->loadData("PERCENTILE_RANKS");

                casacore::IPosition dims = dataSetPercentiles.shape();
                casacore::IPosition dimsRanks = dataSetPercentilesRank.shape();

                auto numRanks = dimsRanks[0];
                casacore::Vector<float> ranks(numRanks);
                dataSetPercentilesRank.get(ranks, false);

                if (dimensions == 2 && dims.size() == 1 && dims[0] == numRanks) {
                    casacore::Vector<float> vals(numRanks);
                    dataSetPercentiles.get(vals, true);
                    vals.tovector(channelStats[0][0].percentiles);
                    ranks.tovector(channelStats[0][0].percentileRanks);
                }
                    // 3D cubes
                else if (dimensions == 3 && dims.size() == 2 && dims[0] == depth && dims[1] == numRanks) {
                    casacore::Matrix<float> vals(depth, numRanks);
                    dataSetPercentiles.get(vals, false);

                    for (auto i = 0; i < depth; i++) {
                        ranks.tovector(channelStats[0][i].percentileRanks);
                        channelStats[0][i].percentiles.resize(numRanks);
                        for (auto j = 0; j < numRanks; j++) {
                            channelStats[0][i].percentiles[j] = vals(i,j);
                        }
                    }
                }
                    // 4D cubes
                else if (dimensions == 4 && dims.size() == 3 && dims[0] == stokes && dims[1] == depth && dims[2] == numRanks) {
                    casacore::Cube<float> vals(stokes, depth, numRanks);
                    dataSetPercentiles.get(vals, false);

                    for (auto i = 0; i < stokes; i++) {
                        for (auto j = 0; j < depth; j++) {
                            auto& stats = channelStats[i][j];
                            stats.percentiles.resize(numRanks);
                            for (auto k = 0; k < numRanks; k++) {
                                stats.percentiles[k] = vals(i,j,k);
                            }
                            ranks.tovector(stats.percentileRanks);
                        }
                    }
                } else {
                    log(uuid, "Missing Percentiles datasets");
                    return false;
                }
            } else {
                log(uuid, "Missing Percentiles group");
                return false;
            }
        }
    } else {
        log(uuid, "Missing Statistics group");
        return false;
    }

    return true;
}

vector<float> Frame::getImageData(bool meanFilter) {
    if (!valid) {
        return vector<float>();
    }

    const int x = bounds.x_min();
    const int y = bounds.y_min();
    const int reqHeight = bounds.y_max() - bounds.y_min();
    const int reqWidth = bounds.x_max() - bounds.x_min();

    if (height < y + reqHeight || width < x + reqWidth) {
        return vector<float>();
    }

    size_t numRowsRegion = reqHeight / mip;
    size_t rowLengthRegion = reqWidth / mip;
    vector<float> regionData;
    regionData.resize(numRowsRegion * rowLengthRegion);

    if (meanFilter) {
        // Perform down-sampling by calculating the mean for each MIPxMIP block
        for (auto j = 0; j < numRowsRegion; j++) {
            for (auto i = 0; i < rowLengthRegion; i++) {
                float pixelSum = 0;
                int pixelCount = 0;
                for (auto pixelX = 0; pixelX < mip; pixelX++) {
                    for (auto pixelY = 0; pixelY < mip; pixelY++) {
                        auto imageRow = y + j * mip + pixelY;
                        auto imageCol = x + i * mip + pixelX;
                        float pixVal = channelCache(imageCol, imageRow);
                        if (!isnan(pixVal)) {
                            pixelCount++;
                            pixelSum += pixVal;
                        }
                    }
                }
                regionData[j * rowLengthRegion + i] = pixelCount ? pixelSum / pixelCount : NAN;
            }
        }
    } else {
        // Nearest neighbour filtering
        for (auto j = 0; j < numRowsRegion; j++) {
            for (auto i = 0; i < rowLengthRegion; i++) {
                auto imageRow = y + j * mip;
                auto imageCol = x + i * mip;
                regionData[j * rowLengthRegion + i] = channelCache(imageCol, imageRow);
            }
        }
    }
    return regionData;
}

CARTA::Histogram Frame::currentHistogram() {
    CARTA::Histogram histogram;
    histogram.set_channel(channelIndex);

    // Calculate histogram if it hasn't been stored
    if (channelStats[stokesIndex][channelIndex].histogramBins.empty()) {
        float minVal = channelCache(0,0);
        float maxVal = channelCache(0,0);
        float sum = 0.0f;
        int count = 0;
        for (auto i = 0; i < height; i++) {
            for (auto j = 0; j < width; ++j) {
                auto v = channelCache(j, i);
                minVal = fmin(minVal, v);
                maxVal = fmax(maxVal, v);
                sum += isnan(v) ? 0.0 : v;
                count += isnan(v) ? 0 : 1;
            }
        }

        ChannelStats& stats = channelStats[stokesIndex][channelIndex];
        stats.minVal = minVal;
        stats.maxVal = maxVal;
        stats.nanCount = count;
        stats.mean = sum / max(count, 1);
        int N = int(max(sqrt(width * height), 2.0));
        stats.histogramBins.resize(N, 0);
        float binWidth = (stats.maxVal - stats.minVal) / N;

        for (auto i = 0; i < height; i++) {
            for (auto j = 0; j < width; ++j) {
                auto v = channelCache(j,i);
                if (isnan(v)) {
                    continue;
                }
                int bin = max(min((int) ((v - minVal) / binWidth), N - 1), 0);
                stats.histogramBins[bin]++;
            }
        }
    }

    // Create histogram object
    auto& currentStats = channelStats[stokesIndex][channelIndex];
    histogram.set_num_bins(currentStats.histogramBins.size());
    histogram.set_bin_width((currentStats.maxVal - currentStats.minVal) / currentStats.histogramBins.size());
    histogram.set_first_bin_center(currentStats.minVal + histogram.bin_width() / 2.0);
    *histogram.mutable_bins() = {currentStats.histogramBins.begin(), currentStats.histogramBins.end()};

    return histogram;
}

int Frame::currentStokes() {
    return stokesIndex;
}

int Frame::currentChannel() {
    return channelIndex;
}

int Frame::currentMip() {
    return mip;
}

CARTA::ImageBounds Frame::currentBounds() {
    return bounds;
}
