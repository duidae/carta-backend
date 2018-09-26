#include "Frame.h"
#include "util.h"
#include <memory>
#include <tbb/tbb.h>

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

        imageShape = dataSet.shape(); //(width, height, chan, stokes)
        ndims = imageShape.size();
        if (ndims < 2 || ndims > 4) {
            log(uuid, "Problem loading file {}: Image must be 2D, 3D or 4D.", filename);
            valid = false;
            return;
        }

        log(uuid, "Opening image with dimensions: {}", imageShape);
        // string axesInfo = fmt::format("Opening image with dimensions: {}", dimensions);
        // sendLogEvent(axesInfo, {"file"}, CARTA::ErrorSeverity::DEBUG);

        // set current channel, stokes, channelCache
        valid = setImageChannels(defaultChannel, 0);

        // make Region for entire image (after current channel/stokes set)
        setImageRegion();
        loadImageChannelStats(false); // from image file if exists

        // Swizzled data loaded if it exists. Used for Z-profiles and region stats
        if (ndims == 3 && loader->hasData(FileInfo::Data::ZYX)) {
            auto &dataSetSwizzled = loader->loadData(FileInfo::Data::ZYX);
            casacore::IPosition swizzledDims = dataSetSwizzled.shape();
            if (swizzledDims.size() != 3 || swizzledDims[0] != imageShape(2)) {
                log(uuid, "Invalid swizzled data set in file {}, ignoring.", filename);
            } else {
                log(uuid, "Found valid swizzled data set in file {}.", filename);
            }
        } else if (ndims == 4 && loader->hasData(FileInfo::Data::ZYXW)) {
            auto &dataSetSwizzled = loader->loadData(FileInfo::Data::ZYXW);
            casacore::IPosition swizzledDims = dataSetSwizzled.shape();
            if (swizzledDims.size() != 4 || swizzledDims[1] != imageShape(3)) {
                log(uuid, "Invalid swizzled data set in file {}, ignoring.", filename);
            } else {
                log(uuid, "Found valid swizzled data set in file {}.", filename);
            }
        } else {
            log(uuid, "File {} missing optional swizzled data set, using fallback calculation.", filename);
        }
    }
    //TBD: figure out what exceptions need to caught, if any
    catch (casacore::AipsError& err) {
        log(uuid, "Problem loading file {}", filename);
        log(uuid, err.getMesg());
        valid = false;
    }
}

Frame::~Frame() {
    for (auto& region : regions) {
        region.second.reset();
    }
    regions.clear();
}

bool Frame::isValid() {
    return valid;
}

// ********************************************************************
// Image data

std::vector<float> Frame::getImageData(bool meanFilter) {
    if (!valid) {
        return std::vector<float>();
    }

    CARTA::ImageBounds bounds(currentBounds());
    const int x = bounds.x_min();
    const int y = bounds.y_min();
    const int reqHeight = bounds.y_max() - bounds.y_min();
    const int reqWidth = bounds.x_max() - bounds.x_min();

    if (imageShape(1) < y + reqHeight || imageShape(0) < x + reqWidth) {
        return std::vector<float>();
    }

    size_t numRowsRegion = reqHeight / mip;
    size_t rowLengthRegion = reqWidth / mip;
    vector<float> regionData;
    regionData.resize(numRowsRegion * rowLengthRegion);

    if (meanFilter) {
        // Perform down-sampling by calculating the mean for each MIPxMIP block
        auto range = tbb::blocked_range2d<size_t>(0, numRowsRegion, 0, rowLengthRegion);
        auto loop = [&](const tbb::blocked_range2d<size_t> &r) {
            for(size_t j = r.rows().begin(); j != r.rows().end(); ++j) {
                for(size_t i = r.cols().begin(); i != r.cols().end(); ++i) {
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
        };
        tbb::parallel_for(range, loop);
    } else {
        // Nearest neighbour filtering
        auto range = tbb::blocked_range2d<size_t>(0, numRowsRegion, 0, rowLengthRegion);
        auto loop = [&](const tbb::blocked_range2d<size_t> &r) {
            for (auto j = 0; j < numRowsRegion; j++) {
                for (auto i = 0; i < rowLengthRegion; i++) {
                    auto imageRow = y + j * mip;
                    auto imageCol = x + i * mip;
                    regionData[j * rowLengthRegion + i] = channelCache(imageCol, imageRow);
                }
            }
        };
        tbb::parallel_for(range, loop);
    }
    return regionData;
}

bool Frame::loadImageChannelStats(bool loadPercentiles) {
    // load channel stats for entire image (all channels and stokes) from header
    // channelStats[stokes][chan]
    if (!valid) {
        log(uuid, "No file loaded");
        return false;
    }

    size_t depth(ndims>2 ? imageShape(2) : 1);
    size_t nstokes(ndims>3 ? imageShape(3) : 1);
    channelStats.resize(nstokes);
    for (auto i = 0; i < nstokes; i++) {
        channelStats[i].resize(depth);
    }

    //TODO: Support multiple HDUs
    if (loader->hasData(FileInfo::Data::Stats) && loader->hasData(FileInfo::Data::Stats2D)) {
        if (loader->hasData(FileInfo::Data::S2DMax)) {
            auto &dataSet = loader->loadData(FileInfo::Data::S2DMax);
            casacore::IPosition statDims = dataSet.shape();

            // 2D cubes
            if (ndims == 2 && statDims.size() == 0) {
                casacore::Array<float> data;
                dataSet.get(data, true);
                auto it = data.begin();
                channelStats[0][0].maxVal = *it;
            } // 3D cubes
            else if (ndims == 3 && statDims.size() == 1 && statDims[0] == depth) {
                casacore::Array<float> data;
                dataSet.get(data, true);
                auto it = data.begin();
                for (auto i = 0; i < depth; ++i) {
                    channelStats[0][i].maxVal = *it++;
                }
            } // 4D cubes
            else if (ndims == 4 && statDims.size() == 2 &&
                     statDims[0] == nstokes && statDims[1] == depth) {
                casacore::Array<float> data;
                dataSet.get(data, true);
                auto it = data.begin();
                for (auto i = 0; i < nstokes; i++) {
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

        if (loader->hasData(FileInfo::Data::S2DMin)) {
            auto &dataSet = loader->loadData(FileInfo::Data::S2DMin);
            casacore::IPosition statDims = dataSet.shape();

            // 2D cubes
            if (ndims == 2 && statDims.size() == 0) {
                casacore::Array<float> data;
                dataSet.get(data, true);
                auto it = data.begin();
                channelStats[0][0].maxVal = *it;
            } // 3D cubes
            else if (ndims == 3 && statDims.size() == 1 && statDims[0] == depth) {
                casacore::Array<float> data;
                dataSet.get(data, true);
                auto it = data.begin();
                for (auto i = 0; i < depth; ++i) {
                    channelStats[0][i].maxVal = *it++;
                }
            } // 4D cubes
            else if (ndims == 4 && statDims.size() == 2 &&
                     statDims[0] == nstokes && statDims[1] == depth) {
                casacore::Array<float> data;
                dataSet.get(data, true);
                auto it = data.begin();
                for (auto i = 0; i < nstokes; i++) {
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

        if (loader->hasData(FileInfo::Data::S2DMean)) {
            auto &dataSet = loader->loadData(FileInfo::Data::S2DMean);
            casacore::IPosition statDims = dataSet.shape();

            // 2D cubes
            if (ndims == 2 && statDims.size() == 0) {
                casacore::Array<float> data;
                dataSet.get(data, true);
                auto it = data.begin();
                channelStats[0][0].maxVal = *it;
            } // 3D cubes
            else if (ndims == 3 && statDims.size() == 1 && statDims[0] == depth) {
                casacore::Array<float> data;
                dataSet.get(data, true);
                auto it = data.begin();
                for (auto i = 0; i < depth; ++i) {
                    channelStats[0][i].maxVal = *it++;
                }
            } // 4D cubes
            else if (ndims == 4 && statDims.size() == 2 &&
                     statDims[0] == nstokes && statDims[1] == depth) {
                casacore::Array<float> data;
                dataSet.get(data, true);
                auto it = data.begin();
                for (auto i = 0; i < nstokes; i++) {
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

        if (loader->hasData(FileInfo::Data::S2DNans)) {
            auto &dataSet = loader->loadData(FileInfo::Data::S2DNans);
            casacore::IPosition statDims = dataSet.shape();

            // 2D cubes
            if (ndims == 2 && statDims.size() == 0) {
                casacore::Array<float> data;
                dataSet.get(data, true);
                auto it = data.begin();
                channelStats[0][0].maxVal = *it;
            } // 3D cubes
            else if (ndims == 3 && statDims.size() == 1 && statDims[0] == depth) {
                casacore::Array<float> data;
                dataSet.get(data, true);
                auto it = data.begin();
                for (auto i = 0; i < depth; ++i) {
                    channelStats[0][i].maxVal = *it++;
                }
            } // 4D cubes
            else if (ndims == 4 && statDims.size() == 2 &&
                     statDims[0] == nstokes && statDims[1] == depth) {
                casacore::Array<float> data;
                dataSet.get(data, true);
                auto it = data.begin();
                for (auto i = 0; i < nstokes; i++) {
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

        if (loader->hasData(FileInfo::Data::S2DHist)) {
            auto &dataSet = loader->loadData(FileInfo::Data::S2DHist);
            casacore::IPosition statDims = dataSet.shape();
            auto numBins = statDims[2];

            // 2D cubes
            if (ndims == 2 && statDims.size() == 0) {
                casacore::Array<float> data;
                dataSet.get(data, true);
                auto it = data.begin();
                std::copy(data.begin(), data.end(),
                          std::back_inserter(channelStats[0][0].histogramBins));
            } // 3D cubes
            else if (ndims == 3 && statDims.size() == 1 && statDims[0] == depth) {
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
            else if (ndims == 4 && statDims.size() == 2 &&
                     statDims[0] == nstokes && statDims[1] == depth) {
                casacore::Array<float> data;
                dataSet.get(data, true);
                auto it = data.begin();
                for (auto i = 0; i < nstokes; i++) {
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
            if (loader->hasData(FileInfo::Data::S2DPercent) &&
                loader->hasData(FileInfo::Data::Ranks)) {
                auto &dataSetPercentiles = loader->loadData(FileInfo::Data::S2DPercent);
                auto &dataSetPercentilesRank = loader->loadData(FileInfo::Data::Ranks);

                casacore::IPosition dimsPercentiles = dataSetPercentiles.shape();
                casacore::IPosition dimsRanks = dataSetPercentilesRank.shape();

                auto numRanks = dimsRanks[0];
                casacore::Vector<float> ranks(numRanks);
                dataSetPercentilesRank.get(ranks, false);

                if (ndims == 2 && dimsPercentiles.size() == 1 && dimsPercentiles[0] == numRanks) {
                    casacore::Vector<float> vals(numRanks);
                    dataSetPercentiles.get(vals, true);
                    vals.tovector(channelStats[0][0].percentiles);
                    ranks.tovector(channelStats[0][0].percentileRanks);
                }
                    // 3D cubes
                else if (ndims == 3 && dimsPercentiles.size() == 2 && dimsPercentiles[0] == depth && 
                         dimsPercentiles[1] == numRanks) {
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
                else if (ndims == 4 && dimsPercentiles.size() == 3 && dimsPercentiles[0] == nstokes && 
                         dimsPercentiles[1] == depth && dimsPercentiles[2] == numRanks) {
                    casacore::Cube<float> vals(nstokes, depth, numRanks);
                    dataSetPercentiles.get(vals, false);

                    for (auto i = 0; i < nstokes; i++) {
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

// ********************************************************************
// Image view

bool Frame::setBounds(CARTA::ImageBounds imageBounds, int newMip) {
    if (!valid) {
        return false;
    }

    const int x = imageBounds.x_min();
    const int y = imageBounds.y_min();
    const int reqHeight = imageBounds.y_max() - imageBounds.y_min();
    const int reqWidth = imageBounds.x_max() - imageBounds.x_min();

    if (imageShape(1) < y + reqHeight || imageShape(0) < x + reqWidth) {
        return false;
    }

    bounds = imageBounds;
    mip = newMip;
    return true;
}

CARTA::ImageBounds Frame::currentBounds() {
    return bounds;
}

int Frame::currentMip() {
    return mip;
}

// ********************************************************************
// Image channels

bool Frame::setImageChannels(size_t newChannel, size_t newStokes) {
    if (!valid) {
        log(uuid, "No file loaded");
        return false;
    } else {
        size_t depth(ndims>2 ? imageShape(2) : 1);
        size_t nstokes(ndims>3 ? imageShape(3) : 1);
        if (newChannel < 0 || newChannel >= depth || newStokes < 0 || newStokes >= nstokes) {
            log(uuid, "Channel {} (stokes {}) is invalid in file {}", newChannel, newStokes, filename);
            return false;
        }
    }
    // update channelCache with new chan and stokes
    getChannelMatrix(channelCache, newChannel, newStokes);
    stokesIndex = newStokes;
    channelIndex = newChannel;
    //updateHistogram();
    return true;
}

void Frame::getChannelMatrix(casacore::Matrix<float>& chanMatrix, size_t channel, size_t stokes) {
    // matrix for given channel and stokes
    if (!channelCache.empty() && channel==channelIndex && stokes==stokesIndex) {
        // already cached
        chanMatrix.reference(channelCache);
    }
    casacore::IPosition count(2, imageShape(0), imageShape(1));
    casacore::IPosition start(2, 0, 0);

    // slice image data
    if(ndims == 3) {
        count.append(casacore::IPosition(1, 1));
        start.append(casacore::IPosition(1, channel));
    } else if(ndims == 4) {
        count.append(casacore::IPosition(2, 1, 1));
        start.append(casacore::IPosition(2, stokes, channel));
    }
    casacore::Slicer section(start, count);
    casacore::Array<float> tmp;
    loader->loadData(FileInfo::Data::XYZW).getSlice(tmp, section, true);
    chanMatrix.reference(tmp);
}


int Frame::currentChannel() {
    return channelIndex;
}

int Frame::currentStokes() {
    return stokesIndex;
}

// ********************************************************************
// Region

bool Frame::setRegion(int regionId, std::string name, CARTA::RegionType type, bool image) {
    // Create new Region and add to regions map.
    auto region = unique_ptr<carta::Region>(new carta::Region(name, type));
    // entire 2D image has regionId -1, but negative id also for creating new region
    if (regionId<0 && !image) { 
        // TODO: create regionId
    }
    regions[regionId] = move(region);
    return true;
}

bool Frame::setRegionChannels(int regionId, int minchan, int maxchan, std::vector<int>& stokes) {
    // set chans to current if -1; set stokes to current if empty
    if (regions.count(regionId)) {
        auto& region = regions[regionId];
        // ICD SET_REGION: if empty, use current Stokes value
        if (stokes.empty()) stokes.push_back(currentStokes());
        region->setChannels(minchan, maxchan, stokes);
        return true;
    } else {
        // TODO: error handling
        return false;
    }
}

bool Frame::setRegionControlPoints(int regionId, std::vector<CARTA::Point>& points) {
    if (regions.count(regionId)) {
        auto& region = regions[regionId];
        region->setControlPoints(points);
        return true;
    } else {
        // TODO: error handling
        return false;
    }
}

bool Frame::setRegionRotation(int regionId, float rotation) {
    if (regions.count(regionId)) {
        auto& region = regions[regionId];
        region->setRotation(rotation);
        return true;
    } else {
        // TODO: error handling
        return false;
    }
}

// special cases of setRegion for image and cursor
void Frame::setImageRegion() {
    // create a Region for the entire image, regionId = -1
    setRegion(IMAGE_REGION_ID, "image", CARTA::RECTANGLE, true);
    // channels
    int nchan(ndims>2 ? imageShape(2) : 1);
    std::vector<int> stokes;
    if (ndims > 3) {
        for (unsigned int i=0; i<imageShape(3); ++i)
            stokes.push_back(i);
    }
    setRegionChannels(IMAGE_REGION_ID, 0, nchan, stokes);
    // control points
    // rectangle from top left (0,height) to bottom right (width, 0)
    std::vector<CARTA::Point> points(2);
    CARTA::Point point;
    point.set_x(0);
    point.set_y(imageShape(1)); // height
    points.push_back(point);
    point.set_x(imageShape(0)); // width
    point.set_y(0);
    points.push_back(point);
    setRegionControlPoints(-1, points);
    // histogram requirements
    std::vector<CARTA::SetHistogramRequirements_HistogramConfig> configs;
    setRegionHistogramRequirements(IMAGE_REGION_ID, configs);
    // spatial requirements
    std::vector<std::string> profiles;
    setRegionSpatialRequirements(IMAGE_REGION_ID, profiles);
}

void Frame::setCursorRegion(int regionId, const CARTA::Point& point) {
    // a cursor is a region with one control point
    if (!regions.count(regionId)) 
        setRegion(regionId, "cursor", CARTA::POINT);
    // use current channel and stokes for cursor
    int currentChan(currentChannel());
    std::vector<int> stokes;
    stokes.push_back(currentStokes());
    setRegionChannels(regionId, currentChan, currentChan, stokes);
    // control point is cursor position
    std::vector<CARTA::Point> points;
    points.push_back(point);
    setRegionControlPoints(regionId, points);
    // histogram requirements
    std::vector<CARTA::SetHistogramRequirements_HistogramConfig> configs;
    setRegionHistogramRequirements(regionId, configs);
    // spatial requirements
    std::vector<std::string> profiles;
    setRegionSpatialRequirements(regionId, profiles);
}

// ****************************************************
// region histograms

bool Frame::setRegionHistogramRequirements(int regionId,
        const std::vector<CARTA::SetHistogramRequirements_HistogramConfig>& histograms) {
    // set channel and num_bins for required histograms
    if (regions.count(regionId)) {
        auto& region = regions[regionId];
        if (histograms.empty()) {  // default to current channel, auto bin size
            std::vector<CARTA::SetHistogramRequirements_HistogramConfig> defaultConfigs;
            CARTA::SetHistogramRequirements_HistogramConfig config;
            config.set_channel(currentChannel());
            config.set_num_bins(-1);
            defaultConfigs.push_back(config);
            return region->setHistogramRequirements(defaultConfigs);
        } else {
            return region->setHistogramRequirements(histograms);
        }
    } else {
        // TODO: error handling
        return false;
    }
}

void Frame::fillRegionHistogramData(int regionId, CARTA::RegionHistogramData* histogramData) {
    if (regions.count(regionId)) {
        auto& region = regions[regionId];
        histogramData->set_stokes(currentStokes());
        for (int i=0; i<region->numHistogramConfigs(); ++i) {
            CARTA::SetHistogramRequirements_HistogramConfig config = region->getHistogramConfig(i);
            int reqChannel(config.channel()), reqNumBins(config.num_bins());
            // get channel(s) and stokes
            int reqStokes(currentStokes());
            std::vector<int> reqChannels;
            if (reqChannel==-1) {
                reqChannels.push_back(currentChannel());
            } else if (reqChannel == -2) { // all channels
                for (size_t i=0; i<imageShape(2); ++i)
                   reqChannels.push_back(i);
            } else {
                reqChannels.push_back(reqChannel);
            }
            // fill Histograms
            for (auto channel : reqChannels) {
                auto newHistogram = histogramData->add_histograms();
                if ((!channelStats[reqStokes][channel].histogramBins.empty())) {
                    // use histogram from image file
                    auto& currentStats = channelStats[reqStokes][currentChannel()];
                    int nbins(currentStats.histogramBins.size());
                    newHistogram->set_num_bins(nbins);
                    newHistogram->set_bin_width((currentStats.maxVal - currentStats.minVal) / nbins);
                    newHistogram->set_first_bin_center(currentStats.minVal + (newHistogram->bin_width()/2.0));
                    *newHistogram->mutable_bins() = {currentStats.histogramBins.begin(), currentStats.histogramBins.end()};
                } else {
                    // get new or stored histogram from Region
                    casacore::Matrix<float> chanMatrix;
                    getChannelMatrix(chanMatrix, channel, reqStokes);
                    region->fillHistogram(newHistogram, chanMatrix, channel, reqStokes);
                }
            }
        }
    }
}

// ****************************************************
// region profiles

bool Frame::setRegionSpatialRequirements(int regionId, const std::vector<std::string>& profiles) {
    // set requested spatial profiles e.g. ["Qx", "Uy"] or just ["x","y"] to use current stokes
    if (!regions.count(regionId) && regionId==0) {
        // frontend sends spatial reqs for cursor before SET_CURSOR; need to set cursor region
        CARTA::Point centerPoint;
        centerPoint.set_x(imageShape(0)/2);
        centerPoint.set_y(imageShape(1)/2);
        setCursorRegion(regionId, centerPoint);
    }
    if (regions.count(regionId)) {
        auto& region = regions[regionId];
        if (profiles.empty()) {  // default to ["x", "y"]
            std::vector<std::string> defaultProfiles;
            defaultProfiles.push_back("x");
            defaultProfiles.push_back("y");
            return region->setSpatialRequirements(defaultProfiles, imageShape, currentStokes());
        } else {
            return region->setSpatialRequirements(profiles, imageShape, currentStokes());
        }
    } else {
        // TODO: error handling
        return false;
    }
}

void Frame::fillSpatialProfileData(int regionId, CARTA::SpatialProfileData& profileData) {
    if (regions.count(regionId)) {
        auto& region = regions[regionId];
        // set profile parameters
        casacore::IPosition params = region->getProfileParams(); // (x, y, chan, stokes)
        profileData.set_x(params(0));
        profileData.set_y(params(1));
        profileData.set_channel(params(2));
        profileData.set_stokes(params(3));
        casacore::Matrix<float> chanMatrix;
        getChannelMatrix(chanMatrix, params(2), params(3));  // (chan, stokes)
        float value = chanMatrix(casacore::IPosition(2, params(0), params(1))); // (x, y)
        profileData.set_value(value);
        // set SpatialProfiles
        for (size_t i=0; i<region->numSpatialProfiles(); ++i) {
            auto newProfile = profileData.add_profiles();
            newProfile->set_coordinate(region->getSpatialProfileStr(i));
            newProfile->set_start(0);
            // get <axis, stokes> for slicing image data
            std::pair<int,int> axisStokes = region->getSpatialProfileReq(i);
            getChannelMatrix(chanMatrix, params(2), axisStokes.second);
            std::vector<float> profile;
            switch (axisStokes.first) {
                case 0: {  // x
                    newProfile->set_end(imageShape(0));
                    profile = chanMatrix.column(params(1)).tovector();
                    break;
                }
                case 1: { // y
                    newProfile->set_end(imageShape(1));
                    profile = chanMatrix.row(params(0)).tovector();
                    break;
                }
            }
            *newProfile->mutable_values() = {profile.begin(), profile.end()};
        }

    }
}

