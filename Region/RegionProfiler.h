// RegionProfiler.h: class for creating requested profiles for and axis (x, y, z) and stokes

#pragma once

#include <vector>
#include <utility>
#include <string>
#include <casacore/casa/Arrays/IPosition.h>

#include <carta-protobuf/defs.pb.h>

namespace carta {

class RegionProfiler {

public:

    bool setSpatialRequirements(const std::vector<std::string>& profiles,
        const casacore::IPosition& imshape, const int defaultStokes);
    size_t numSpatialProfiles();
    std::pair<int,int> getSpatialProfileReq(int profileIndex);
    std::string getSpatialProfileStr(int profileIndex);

private:
    // parse profile strings into <axisIndex, stokesIndex> pairs
    std::pair<int, int> getAxisStokes(std::string profile);

    std::vector<std::pair<int, int>> m_profilePairs; // <axisIndex, stokesIndex>
    std::vector<std::string> m_profiles; // for SpatialProfile 
};

}
