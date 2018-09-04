#include "Frame.h"
#include <cmath>
#include "util.h"
#include <H5Cpp.h>

using namespace std;

Frame::Frame(const string& uuidString, const string& filename, const string& hdu, int defaultChannel)
    : uuid(uuidString),
      valid(true),
      filename(filename),
      loader(FileLoader::getLoader(filename)) {
    try {
        loader->openFile(filename);
        H5::DataSet dataSet = loader->loadData("DATA");
        vector<hsize_t> dims(dataSet.getSpace().getSimpleExtentNdims(), 0);
        dataSet.getSpace().getSimpleExtentDims(dims.data(), NULL);
        dimensions = dims.size();

        if (dimensions < 2 || dimensions > 4) {
            log(uuid, "Problem loading file {}: Image must be 2D, 3D or 4D.", filename);
            valid = false;
            return;
        }

        width = dims[dimensions - 1];
        height = dims[dimensions - 2];
        depth = (dimensions > 2) ? dims[dimensions - 3] : 1;
        stokes = (dimensions > 3) ? dims[dimensions - 4] : 1;

        dataSets.clear();
        dataSets["main"] = dataSet;

        loadStats(false);

        // Swizzled data loaded if it exists. Used for Z-profiles and region stats
        if (loader->hasData("SwizzledData")) {
            if (dimensions == 3 && loader->hasData("SwizzledData/ZYX")) {
                auto dataSetSwizzled = loader->loadData("SwizzledData/ZYX");
                vector<hsize_t> swizzledDims(dataSetSwizzled.getSpace().getSimpleExtentNdims(), 0);
                dataSetSwizzled.getSpace().getSimpleExtentDims(swizzledDims.data(), NULL);

                if (swizzledDims.size() != 3 || swizzledDims[0] != dims[2]) {
                    log(uuid, "Invalid swizzled data set in file {}, ignoring.", filename);
                } else {
                    log(uuid, "Found valid swizzled data set in file {}.", filename);
                    dataSets["swizzled"] = dataSetSwizzled;
                }
            } else if (dimensions == 4 && loader->hasData("SwizzledData/ZYXW")) {
                auto dataSetSwizzled = loader->loadData("SwizzledData/ZYXW");
                vector<hsize_t> swizzledDims(dataSetSwizzled.getSpace().getSimpleExtentNdims(), 0);
                dataSetSwizzled.getSpace().getSimpleExtentDims(swizzledDims.data(), NULL);
                if (swizzledDims.size() != 4 || swizzledDims[1] != dims[3]) {
                    log(uuid, "Invalid swizzled data set in file {}, ignoring.", filename);
                } else {
                    log(uuid, "Found valid swizzled data set in file {}.", filename);
                    dataSets["swizzled"] = dataSetSwizzled;
                }
            } else {
                log(uuid, "File {} missing optional swizzled data set, using fallback calculation.", filename);
            }
        } else {
            log(uuid, "File {} missing optional swizzled data set, using fallback calculation.", filename);
        }
        valid = setChannels(defaultChannel, 0);
    }
    catch (H5::FileIException& err) {
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

    // Define dimensions of hyperslab in 2D
    vector<hsize_t> count = {height, width};
    vector<hsize_t> start = {0, 0};

    // Append channel (and stokes in 4D) to hyperslab dims
    if (dimensions == 3) {
        count.insert(count.begin(), 1);
        start.insert(start.begin(), newChannel);
    } else if (dimensions == 4) {
        count.insert(count.begin(), {1, 1});
        start.insert(start.begin(), {newStokes, newChannel});
    }

    // Read data into memory space
    hsize_t memDims[] = {height, width};
    H5::DataSpace memspace(2, memDims);
    channelCache.resize(width * height);
    auto sliceDataSpace = dataSets["main"].getSpace();
    sliceDataSpace.selectHyperslab(H5S_SELECT_SET, count.data(), start.data());
    dataSets["main"].read(channelCache.data(), H5::PredType::NATIVE_FLOAT, memspace, sliceDataSpace);

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
            auto dataSet = loader->loadData("Statistics/XY/MAX");
            auto dataSpace = dataSet.getSpace();
            vector<hsize_t> dims(dataSpace.getSimpleExtentNdims(), 0);
            dataSpace.getSimpleExtentDims(dims.data(), NULL);

            // 2D cubes
            if (dimensions == 2 && dims.size() == 0) {
                dataSet.read(&channelStats[0][0].maxVal, H5::PredType::NATIVE_FLOAT);
            } // 3D cubes
            else if (dimensions == 3 && dims.size() == 1 && dims[0] == depth) {
                vector<float> data(depth);
                dataSet.read(data.data(), H5::PredType::NATIVE_FLOAT);
                for (auto i = 0; i < depth; i++) {
                    channelStats[0][i].maxVal = data[i];
                }
            } // 4D cubes
            else if (dimensions == 4 && dims.size() == 2 && dims[0] == stokes && dims[1] == depth) {
                vector<float> data(depth * stokes);
                dataSet.read(data.data(), H5::PredType::NATIVE_FLOAT);
                for (auto i = 0; i < stokes; i++) {
                    for (auto j = 0; j < depth; j++) {
                        channelStats[i][j].maxVal = data[i * depth + j];
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
            auto dataSet = loader->loadData("Statistics/XY/MIN");
            auto dataSpace = dataSet.getSpace();
            vector<hsize_t> dims(dataSpace.getSimpleExtentNdims(), 0);
            dataSpace.getSimpleExtentDims(dims.data(), NULL);

            // 2D cubes
            if (dimensions == 2 && dims.size() == 0) {
                dataSet.read(&channelStats[0][0].minVal, H5::PredType::NATIVE_FLOAT);
            } // 3D cubes
            else if (dimensions == 3 && dims.size() == 1 && dims[0] == depth) {
                vector<float> data(depth);
                dataSet.read(data.data(), H5::PredType::NATIVE_FLOAT);
                for (auto i = 0; i < depth; i++) {
                    channelStats[0][i].minVal = data[i];
                }
            } // 4D cubes
            else if (dimensions == 4 && dims.size() == 2 && dims[0] == stokes && dims[1] == depth) {
                vector<float> data(stokes * depth);
                dataSet.read(data.data(), H5::PredType::NATIVE_FLOAT);
                for (auto i = 0; i < stokes; i++) {
                    for (auto j = 0; j < depth; j++) {
                        channelStats[i][j].minVal = data[i * depth + j];
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
            auto dataSet = loader->loadData("Statistics/XY/MEAN");
            auto dataSpace = dataSet.getSpace();
            vector<hsize_t> dims(dataSpace.getSimpleExtentNdims(), 0);
            dataSpace.getSimpleExtentDims(dims.data(), NULL);

            // 2D cubes
            if (dimensions == 2 && dims.size() == 0) {
                dataSet.read(&channelStats[0][0].mean, H5::PredType::NATIVE_FLOAT);
            } // 3D cubes
            else if (dimensions == 3 && dims.size() == 1 && dims[0] == depth) {
                vector<float> data(depth);
                dataSet.read(data.data(), H5::PredType::NATIVE_FLOAT);
                for (auto i = 0; i < depth; i++) {
                    channelStats[0][i].mean = data[i];
                }
            } // 4D cubes
            else if (dimensions == 4 && dims.size() == 2 && dims[0] == stokes && dims[1] == depth) {
                vector<float> data(stokes * depth);
                dataSet.read(data.data(), H5::PredType::NATIVE_FLOAT);
                for (auto i = 0; i < stokes; i++) {
                    for (auto j = 0; j < depth; j++) {
                        channelStats[i][j].mean = data[i * depth + j];
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
            auto dataSet = loader->loadData("Statistics/XY/NAN_COUNT");
            auto dataSpace = dataSet.getSpace();
            vector<hsize_t> dims(dataSpace.getSimpleExtentNdims(), 0);
            dataSpace.getSimpleExtentDims(dims.data(), NULL);

            // 2D cubes
            if (dimensions == 2 && dims.size() == 0) {
                dataSet.read(&channelStats[0][0].nanCount, H5::PredType::NATIVE_INT64);
            } // 3D cubes
            else if (dimensions == 3 && dims.size() == 1 && dims[0] == depth) {
                vector<int64_t> data(depth);
                dataSet.read(data.data(), H5::PredType::NATIVE_INT64);
                for (auto i = 0; i < depth; i++) {
                    channelStats[0][i].nanCount = data[i];
                }
            } // 4D cubes
            else if (dimensions == 4 && dims.size() == 2 && dims[0] == stokes && dims[1] == depth) {
                vector<int64_t> data(stokes * depth);
                dataSet.read(data.data(), H5::PredType::NATIVE_INT64);
                for (auto i = 0; i < stokes; i++) {
                    for (auto j = 0; j < depth; j++) {
                        channelStats[i][j].nanCount = data[i * depth + j];
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
            auto dataSet = loader->loadData("Statistics/XY/HISTOGRAM");
            auto dataSpace = dataSet.getSpace();
            vector<hsize_t> dims(dataSpace.getSimpleExtentNdims(), 0);
            dataSpace.getSimpleExtentDims(dims.data(), NULL);

            // 2D cubes
            if (dimensions == 2) {
                auto numBins = dims[0];
                vector<int> data(numBins);
                dataSet.read(data.data(), H5::PredType::NATIVE_INT);
                channelStats[0][0].histogramBins = data;
            } // 3D cubes
            else if (dimensions == 3 && dims.size() == 2 && dims[0] == depth) {
                auto numBins = dims[1];
                vector<int> data(depth * numBins);
                dataSet.read(data.data(), H5::PredType::NATIVE_INT);
                for (auto i = 0; i < depth; i++) {
                    channelStats[0][i].histogramBins.resize(numBins);
                    for (auto j = 0; j < numBins; j++) {
                        channelStats[0][i].histogramBins[j] = data[i * numBins + j];
                    }
                }
            } // 4D cubes
            else if (dimensions == 4 && dims.size() == 3 && dims[0] == stokes && dims[1] == depth) {
                auto numBins = dims[2];
                vector<int> data(stokes * depth * numBins);
                dataSet.read(data.data(), H5::PredType::NATIVE_INT);
                for (auto i = 0; i < stokes; i++) {
                    for (auto j = 0; j < depth; j++) {
                        auto& stats = channelStats[i][j];
                        stats.histogramBins.resize(numBins);
                        for (auto k = 0; k < numBins; k++) {
                            stats.histogramBins[k] = data[(i * depth + j) * numBins + k];
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
                auto dataSetPercentiles = loader->loadData("Statistics/XY/PERCENTILES");
                auto dataSetPercentilesRank = loader->loadData("PERCENTILE_RANKS");

                auto dataSpacePercentiles = dataSetPercentiles.getSpace();
                vector<hsize_t> dims(dataSpacePercentiles.getSimpleExtentNdims(), 0);
                dataSpacePercentiles.getSimpleExtentDims(dims.data(), NULL);
                auto dataSpaceRank = dataSetPercentilesRank.getSpace();
                vector<hsize_t> dimsRanks(dataSpaceRank.getSimpleExtentNdims(), 0);
                dataSpaceRank.getSimpleExtentDims(dimsRanks.data(), NULL);

                auto numRanks = dimsRanks[0];
                vector<float> ranks(numRanks);
                dataSetPercentilesRank.read(ranks.data(), H5::PredType::NATIVE_FLOAT);

                if (dimensions == 2 && dims.size() == 1 && dims[0] == numRanks) {
                    vector<float> vals(numRanks);
                    dataSetPercentiles.read(vals.data(), H5::PredType::NATIVE_FLOAT);
                    channelStats[0][0].percentiles = vals;
                    channelStats[0][0].percentileRanks = ranks;
                }
                    // 3D cubes
                else if (dimensions == 3 && dims.size() == 2 && dims[0] == depth && dims[1] == numRanks) {
                    vector<float> vals(depth * numRanks);
                    dataSetPercentiles.read(vals.data(), H5::PredType::NATIVE_FLOAT);

                    for (auto i = 0; i < depth; i++) {
                        channelStats[0][i].percentileRanks = ranks;
                        channelStats[0][i].percentiles.resize(numRanks);
                        for (auto j = 0; j < numRanks; j++) {
                            channelStats[0][i].percentiles[j] = vals[i * numRanks + j];
                        }
                    }
                }
                    // 4D cubes
                else if (dimensions == 4 && dims.size() == 3 && dims[0] == stokes && dims[1] == depth && dims[2] == numRanks) {
                    vector<float> vals(stokes * depth * numRanks);
                    dataSetPercentiles.read(vals.data(), H5::PredType::NATIVE_FLOAT);

                    for (auto i = 0; i < stokes; i++) {
                        for (auto j = 0; j < depth; j++) {
                            auto& stats = channelStats[i][j];
                            stats.percentiles.resize(numRanks);
                            for (auto k = 0; k < numRanks; k++) {
                                stats.percentiles[k] = vals[(i * depth + j) * numRanks + k];
                            }
                            stats.percentileRanks = ranks;
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
                        float pixVal = channelCache[imageRow * width + imageCol];
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
                regionData[j * rowLengthRegion + i] = channelCache[imageRow * width + imageCol];
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
        float minVal = channelCache[0];
        float maxVal = channelCache[0];
        float sum = 0.0f;
        int count = 0;
        for (auto i = 0; i < width * height; i++) {
            auto v = channelCache[i];
            minVal = fmin(minVal, v);
            maxVal = fmax(maxVal, v);
            sum += isnan(v) ? 0.0 : v;
            count += isnan(v) ? 0 : 1;
        }

        ChannelStats& stats = channelStats[stokesIndex][channelIndex];
        stats.minVal = minVal;
        stats.maxVal = maxVal;
        stats.nanCount = count;
        stats.mean = sum / max(count, 1);
        int N = int(max(sqrt(width * height), 2.0));
        stats.histogramBins.resize(N, 0);
        float binWidth = (stats.maxVal - stats.minVal) / N;

        for (auto i = 0; i < width * height; i++) {
            auto v = channelCache[i];
            if (isnan(v)) {
                continue;
            }
            int bin = max(min((int) ((v - minVal) / binWidth), N - 1), 0);
            stats.histogramBins[bin]++;
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
