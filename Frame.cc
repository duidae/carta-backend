#include "Frame.h"
#include "util.h"
#include <memory>

using namespace carta;
using namespace std;

Frame::Frame(const string& uuidString, const string& filename, const string& hdu, int defaultChannel)
    : uuid(uuidString),
      valid(true),
      filename(filename),
      loader(FileLoader::getLoader(filename)) {
    try {
        if (loader==nullptr) {
            log(uuid, "Problem loading file {}: loader not implemented", filename);
            valid = false;
            return;
        }
        loader->openFile(filename, hdu);
        auto &dataSet = loader->loadData(FileInfo::Data::XYZW);

        imgShape.dimensions = dataSet.shape();
        size_t ndims = imgShape.dimensions.size();

        if (ndims < 2 || ndims > 4) {
            log(uuid, "Problem loading file {}: Image must be 2D, 3D or 4D.", filename);
            valid = false;
            return;
        }

        log(uuid, "Opening image with dimensions: {}", imgShape.dimensions);
        // string axesInfo = fmt::format("Opening image with dimensions: {}", dimensions);
        // sendLogEvent(axesInfo, {"file"}, CARTA::ErrorSeverity::DEBUG);

        // TBD: use casacore::ImageInterface to get axes
        imgShape.width = imgShape.dimensions[0];
        imgShape.height = imgShape.dimensions[1];
        imgShape.depth = (ndims > 2) ? imgShape.dimensions[2] : 1;
        imgShape.stokes = (ndims > 3) ? imgShape.dimensions[3] : 1;

	stats = unique_ptr<RegionStats>(new RegionStats());
        loadStats(false);

        // Swizzled data loaded if it exists. Used for Z-profiles and region stats
        if (imgShape.dimensions == 3 && loader->hasData(FileInfo::Data::ZYX)) {
            auto &dataSetSwizzled = loader->loadData(FileInfo::Data::ZYX);
            casacore::IPosition swizzledDims = dataSetSwizzled.shape();
            if (swizzledDims.size() != 3 || swizzledDims[0] != imgShape.dimensions[2]) {
                log(uuid, "Invalid swizzled data set in file {}, ignoring.", filename);
            } else {
                log(uuid, "Found valid swizzled data set in file {}.", filename);
            }
        } else if (imgShape.dimensions == 4 && loader->hasData(FileInfo::Data::ZYXW)) {
            auto &dataSetSwizzled = loader->loadData(FileInfo::Data::ZYXW);
            casacore::IPosition swizzledDims = dataSetSwizzled.shape();
            if (swizzledDims.size() != 4 || swizzledDims[1] != imgShape.dimensions[3]) {
                log(uuid, "Invalid swizzled data set in file {}, ignoring.", filename);
            } else {
                log(uuid, "Found valid swizzled data set in file {}.", filename);
            }
        } else {
            log(uuid, "File {} missing optional swizzled data set, using fallback calculation.", filename);
        }
        valid = setChannels(defaultChannel, 0);
    }
    //TBD: figure out what exceptions need to caught, if any
    catch (casacore::AipsError& err) {
        log(uuid, "Problem loading file {}", filename);
        log(uuid, err.getMesg());
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
    } else if (newChannel < 0 || newChannel >= imgShape.depth || newStokes < 0 || newStokes >= imgShape.stokes) {
        log(uuid, "Channel {} (stokes {}) is invalid in file {}", newChannel, newStokes, filename);
        return false;
    }

    casacore::IPosition count(2, imgShape.width, imgShape.height);
    casacore::IPosition start(2, 0, 0);
    if(imgShape.dimensions.size() == 3) {
        count.append(casacore::IPosition(1, 1));
        start.append(casacore::IPosition(1, newChannel));
    } else if(imgShape.dimensions.size() == 4) {
        count.append(casacore::IPosition(2, 1, 1));
        start.append(casacore::IPosition(2, newStokes, newChannel));
    }
    casacore::Slicer section(start, count);
    casacore::Array<float> tmp;
    loader->loadData(FileInfo::Data::XYZW).getSlice(tmp, section, true);
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

    if (imgShape.height < y + reqHeight || imgShape.width < x + reqWidth) {
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
    std::string msg;
    bool statsLoaded = stats->loadStats(loadPercentiles, imgShape, loader, msg);
    if (!msg.empty())
        log(uuid, msg);
    return statsLoaded;
}

std::vector<float> Frame::getImageData(bool meanFilter) {
    if (!valid) {
        return std::vector<float>();
    }

    CARTA::ImageBounds bounds(currentBounds());
    const int x = bounds.x_min();
    const int y = bounds.y_min();
    const int reqHeight = bounds.y_max() - bounds.y_min();
    const int reqWidth = bounds.x_max() - bounds.x_min();

    if (imgShape.height < y + reqHeight || imgShape.width < x + reqWidth) {
        return std::vector<float>();
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
    return stats->currentHistogram(channelCache, imgShape, channelIndex, stokesIndex);
}

CARTA::ImageBounds Frame::currentBounds() {
    return bounds;
}

int Frame::currentChannel() {
    return channelIndex;
}

int Frame::currentStokes() {
    return stokesIndex;
}

int Frame::currentMip() {
    return mip;
}

