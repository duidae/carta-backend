// RegionProfiler.cc: implementation of RegionProfiler class to create x, y, z region profiles

#include "RegionProfiler.h"

using namespace carta;

bool RegionProfiler::setSpatialRequirements(const std::vector<std::string>& profiles,
            const casacore::IPosition& imshape, const int defaultStokes) {
    // process profile strings into pairs <axis, stokes>
    m_profiles.clear();
    m_profilePairs.clear();
    int nstokes(imshape.size()>3 ? imshape(3) : 1);
    for (auto profile : profiles) {
        if (profile.empty() || profile.size() > 2) // ignore invalid profile string
            continue;
        // convert string to pair<axisIndex, stokesIndex>;
        std::pair<int, int> axisStokes = getAxisStokes(profile);
        if ((axisStokes.first < 0) || (axisStokes.first > 1)) // invalid axis
            continue;
        if (axisStokes.second > (nstokes-1)) // invalid stokes
            continue;
        if (axisStokes.second < 0) // not specified
            axisStokes.second = defaultStokes;
        m_profiles.push_back(profile);
        m_profilePairs.push_back(axisStokes);
    }
    return (profiles.size()==m_profiles.size());
}

std::pair<int, int> RegionProfiler::getAxisStokes(std::string profile) {
    // converts profile string into <axis, stokes> pair
    int axisIndex(-1), stokesIndex(-1);
    // axis
    char axisChar(profile.back());
    if (axisChar=='x') axisIndex = 0;
    else if (axisChar=='y') axisIndex = 1; 
    // stokes
    if (profile.size()==2) {
        char stokesChar(profile.front());
        if (stokesChar=='I') stokesIndex=0;
        else if (stokesChar=='Q') stokesIndex=1;
        else if (stokesChar=='U') stokesIndex=2;
        else if (stokesChar=='V') stokesIndex=3;
    }
    return std::make_pair(axisIndex, stokesIndex);
}

size_t RegionProfiler::numSpatialProfiles() {
    return m_profilePairs.size();
}

std::pair<int,int> RegionProfiler::getSpatialProfileReq(int profileIndex) {
    if (profileIndex < m_profilePairs.size())
        return m_profilePairs[profileIndex];
    else
        return std::pair<int,int>();
}

std::string RegionProfiler::getSpatialProfileStr(int profileIndex) {
    if (profileIndex < m_profiles.size())
        return m_profiles[profileIndex];
    else
        return std::string();
}
