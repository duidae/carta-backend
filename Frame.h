#pragma once
#include <vector>
#include <map>
#include <string>
#include <memory>

#include <carta-protobuf/defs.pb.h>
#include <casacore/casa/Arrays/IPosition.h>
#include "ImageData/FileLoader.h"
#include "Region/Region.h"

#define IMAGE_REGION_ID -1
#define CURSOR_REGION_ID 0

struct ChannelStats {
    float minVal;
    float maxVal;
    float mean;
    std::vector<float> percentiles;
    std::vector<float> percentileRanks;
    std::vector<int> histogramBins;
    int64_t nanCount;
};

class Frame {

private:
    // setup
    std::string uuid;
    bool valid;

    // image loader, shape, stats from image file
    std::string filename;
    std::unique_ptr<carta::FileLoader> loader;
    casacore::IPosition imageShape; // (width, height, depth, stokes)
    size_t ndims;
    std::vector<std::vector<ChannelStats>> channelStats;

    // set image view 
    CARTA::ImageBounds bounds;
    int mip;

    // set image channel
    size_t channelIndex;
    size_t stokesIndex;

    // saved matrix for channelIndex, stokesIndex
    casacore::Matrix<float> channelCache;

    // Region
    // <region_id, Region>: one Region per ID
    std::map<int, std::unique_ptr<carta::Region>> regions;

    bool loadImageChannelStats(bool loadPercentiles = false);
    void setImageRegion(); // set region for entire image
    // matrix for given channel and stokes
    casacore::Matrix<float> getChannelMatrix(size_t channel, size_t stokes);

public:
    Frame(const std::string& uuidString, const std::string& filename, const std::string& hdu, int defaultChannel = 0);
    ~Frame();

    bool isValid();

    // image data
    std::vector<float> getImageData(bool meanFilter = true);

    // image view
    bool setBounds(CARTA::ImageBounds imageBounds, int newMip);
    CARTA::ImageBounds currentBounds();
    int currentMip();

    // image channels
    bool setImageChannels(size_t newChannel, size_t newStokes);
    int currentStokes();
    int currentChannel();

    // region data: pass through to Region
    // SET_REGION fields:
    bool setRegion(int regionId, std::string name, CARTA::RegionType type, bool image=false);
    bool setRegionChannels(int regionId, int minchan, int maxchan, std::vector<int>& stokes);
    bool setRegionControlPoints(int regionId, std::vector<CARTA::Point>& points);
    bool setRegionRotation(int regionId, float rotation);

    // setRegion for cursor (defaults for fields not in SET_CURSOR)
    void setCursorRegion(int regionId, const CARTA::Point& point);

    // region histograms
    bool setRegionHistogramRequirements(int regionId,
        const std::vector<CARTA::SetHistogramRequirements_HistogramConfig>& histograms);
    std::vector<CARTA::Histogram> getRegionHistograms(int regionId);

    // region profiles
    //bool setRegionSpatialRequirements(int regionId, const std::vector<std::string>& profiles);
    // returns (x, y, channel, stokes) as an IPosition:
    //casacore::IPosition getRegionProfileParams(int regionId);
    //std::vector<CARTA::SpatialProfile> getRegionSpatialProfiles(int regionId);
};
