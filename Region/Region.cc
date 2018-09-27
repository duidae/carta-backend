//# Region.cc: implementation of class for managing a region

#include "Region.h"

using namespace carta;

Region::Region(const std::string& name, const CARTA::RegionType type) :
    m_name(name), m_type(type), m_rotation(0.0) {
    m_stats = std::unique_ptr<RegionStats>(new RegionStats());
    m_profiler = std::unique_ptr<RegionProfiler>(new RegionProfiler());
}

void Region::setChannels(int minchan, int maxchan, std::vector<int>& stokes) {
    m_minchan = minchan;
    m_maxchan = maxchan;
    m_stokes = stokes;
}

void Region::setControlPoints(const std::vector<CARTA::Point>& points) {
    m_ctrl_pts = points;
}

void Region::setRotation(const float rotation) {
    m_rotation = rotation;
}

// ***********************************
// RegionStats

bool Region::setHistogramRequirements(const std::vector<CARTA::SetHistogramRequirements_HistogramConfig>& histogramReqs) {
    m_stats->setHistogramRequirements(histogramReqs);
    return true;
}

CARTA::SetHistogramRequirements_HistogramConfig Region::getHistogramConfig(int histogramIndex) {
    return m_stats->getHistogramConfig(histogramIndex);
}

size_t Region::numHistogramConfigs() {
    return m_stats->numHistogramConfigs();
}

void Region::fillHistogram(CARTA::Histogram* histogram, const casacore::Matrix<float>& chanMatrix,
        const size_t chanIndex, const size_t stokesIndex) {
    return m_stats->fillHistogram(histogram, chanMatrix, chanIndex, stokesIndex);
}

// ***********************************
// RegionProfiler

bool Region::setSpatialRequirements(const std::vector<std::string>& profiles,
        const casacore::IPosition& imshape, const int defaultStokes) {
    return m_profiler->setSpatialRequirements(profiles, imshape, defaultStokes);
}

CARTA::Point Region::getControlPoint(int pointIndex) {
    CARTA::Point point;
    if (pointIndex < m_ctrl_pts.size())
        point = m_ctrl_pts[pointIndex];
    return point;
}

size_t Region::numSpatialProfiles() {
    return m_profiler->numSpatialProfiles();
}

std::pair<int,int> Region::getSpatialProfileReq(int profileIndex) {
    return m_profiler->getSpatialProfileReq(profileIndex);
}

std::string Region::getSpatialProfileStr(int profileIndex) {
    return m_profiler->getSpatialProfileStr(profileIndex);
}
