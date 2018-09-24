//# Region.cc: implementation of class for managing a region

#include "Region.h"

using namespace carta;

Region::Region(const std::string& name, const CARTA::RegionType type) :
    m_name(name), m_type(type), m_rotation(0.0) {
    m_stats = std::unique_ptr<RegionStats>(new RegionStats());
    //m_profiler = std::unique_ptr<RegionProfiler>(new RegionProfiler());
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

/*
casacore::IPosition Region::getProfileParams() {
    // get Region parameters (x, y, channel, stokes) for spatial profile
    casacore::IPosition params(4);
    // TODO: this is all for cursor, what to do for region?
    if (!m_ctrl_pts.empty()) {
        CARTA::Point point(m_ctrl_pts[0]); // use first point
        params(0) = point.x();
        params(1) = point.y();
    }
    if (m_minchan==m_maxchan)  // frame.currentChannel() at the time
        params(2) = m_minchan;
    if (!m_stokes.empty())
        params(3) = m_stokes[0];
    return params;
}
*/

// ***********************************
// RegionStats

void Region::setHistogramRequirements(const std::vector<CARTA::SetHistogramRequirements_HistogramConfig>& histogramReqs) {
    m_stats->setHistogramRequirements(histogramReqs);
}

CARTA::SetHistogramRequirements_HistogramConfig Region::getHistogramRequirement(int histogramIndex) {
    return m_stats->getHistogramRequirement(histogramIndex);
}

size_t Region::numHistogramReqs() {
    return m_stats->numHistogramReqs();
}

CARTA::Histogram Region::getHistogram(const casacore::Matrix<float>& chanMatrix,
        const size_t chanIndex, const size_t stokesIndex) {
    return m_stats->getHistogram(chanMatrix, chanIndex, stokesIndex);
}

// ***********************************
// RegionProfiler
/*
bool Region::setSpatialRequirements(const std::vector<std::string>& profiles,
        const casacore::IPosition& imshape, const int defaultStokes) {
    return m_profiler->setSpatialRequirements(profiles, imshape, defaultStokes);
}

std::vector<CARTA::SpatialProfile> Region::getSpatialProfiles() {
    return m_profiler->getSpatialProfiles(getProfileParams());
}
*/
