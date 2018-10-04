// RegionProfiler.cc: implementation of RegionProfiler class to create x, y, z region profiles

#include "RegionProfiler.h"
#include <casacore/lattices/LatticeMath/LatticeStatistics.h>

using namespace carta;

// ***** spatial *****

bool RegionProfiler::setSpatialRequirements(const std::vector<std::string>& profiles,
            const int nstokes, const int defaultStokes) {
    // process profile strings into pairs <axis, stokes>
    m_spatialProfiles.clear();
    m_profilePairs.clear();
    for (auto profile : profiles) {
        if (profile.empty() || profile.size() > 2) // ignore invalid profile string
            continue;
        // convert string to pair<axisIndex, stokesIndex>;
        std::pair<int, int> axisStokes = getAxisStokes(profile);
        if ((axisStokes.first < 0) || (axisStokes.first > 1)) // invalid axis
            continue;
	if (axisStokes.second > (nstokes-1)) // invalid stokes
            continue;
	else if (axisStokes.second < 0) // not specified
            axisStokes.second = defaultStokes;
        m_spatialProfiles.push_back(profile);
        m_profilePairs.push_back(axisStokes);
    }
    return (profiles.size()==m_spatialProfiles.size());
}

std::pair<int, int> RegionProfiler::getAxisStokes(std::string profile) {
    // converts profile string into <axis, stokes> pair
    int axisIndex(-1), stokesIndex(-1);
    // axis
    char axisChar(profile.back());
    if (axisChar=='x') axisIndex = 0;
    else if (axisChar=='y') axisIndex = 1; 
    else if (axisChar=='z') axisIndex = 2; 
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
    if (profileIndex < m_spatialProfiles.size())
        return m_spatialProfiles[profileIndex];
    else
        return std::string();
}

// ***** spectral *****

bool RegionProfiler::setSpectralRequirements(const std::vector<CARTA::SetSpectralRequirements_SpectralConfig>& profiles,
        const int nstokes, const int defaultStokes) {
    // parse stokes into index
    m_spectralProfiles.clear();
    m_spectralStokes.clear();
    for (auto profile : profiles) {
        std::string coordinate(profile.coordinate());
        if (coordinate.empty() || coordinate.size() > 2) // ignore invalid profile string
            continue;
        std::pair<int, int> axisStokes = getAxisStokes(coordinate);
	if (axisStokes.first != 2)  // invalid axis
            continue;
	if (axisStokes.second > (nstokes-1)) // invalid stokes
            continue;
        int stokes;
	if (axisStokes.second < 0) { // use default
            stokes = defaultStokes;
	} else {
            stokes = axisStokes.second;
        }
        m_spectralProfiles.push_back(profile);
        m_spectralStokes.push_back(stokes);
    }
    return (profiles.size()==m_spectralProfiles.size());
}

size_t RegionProfiler::numSpectralProfiles() {
    return m_spectralProfiles.size();
}

int RegionProfiler::getSpectralConfigStokes(int profileIndex) {
    int stokes;
    if (profileIndex < m_spectralStokes.size())
        stokes = m_spectralStokes[profileIndex];
    return stokes;
}

CARTA::SetSpectralRequirements_SpectralConfig RegionProfiler::getSpectralConfig(int profileIndex) {
    CARTA::SetSpectralRequirements_SpectralConfig config;
    if (profileIndex < m_spectralProfiles.size())
        config = m_spectralProfiles[profileIndex];
    return config;
}

void RegionProfiler::setSpectralLattice(const casacore::SubLattice<float>& lattice) {
    m_spectralLattice = lattice;
}

void RegionProfiler::getStats(std::vector<float>& statistics, CARTA::StatsType type) {
    if (type != CARTA::StatsType::None) {
        // use LatticeStatistics to fill statistics values according to type
        casacore::LatticeStatsBase::StatisticsTypes lattStatsType(casacore::LatticeStatsBase::NSTATS);
        switch (type) {
            case CARTA::StatsType::None:
                break;
            case CARTA::StatsType::Sum:
	        lattStatsType = casacore::LatticeStatsBase::SUM;
	        break;
            case CARTA::StatsType::FluxDensity:
	        lattStatsType = casacore::LatticeStatsBase::FLUX;
	        break;
            case CARTA::StatsType::Mean:
	        lattStatsType = casacore::LatticeStatsBase::MEAN;
	        break;
            case CARTA::StatsType::RMS:
	        lattStatsType = casacore::LatticeStatsBase::RMS;
	        break;
            case CARTA::StatsType::Sigma:
	        lattStatsType = casacore::LatticeStatsBase::SIGMA;
	        break;
            case CARTA::StatsType::SumSq:
	        lattStatsType = casacore::LatticeStatsBase::SUMSQ;
	        break;
            case CARTA::StatsType::Min:
	        lattStatsType = casacore::LatticeStatsBase::MIN;
	        break;
            case CARTA::StatsType::Max:
	        lattStatsType = casacore::LatticeStatsBase::MAX;
	        break;
	    case CARTA::StatsType::Blc:
	    case CARTA::StatsType::Trc:
	    case CARTA::StatsType::MinPos:
	    case CARTA::StatsType::MaxPos:
	    case CARTA::StatsType::Blcf:
	    case CARTA::StatsType::Trcf:
	    case CARTA::StatsType::MinPosf:
	    case CARTA::StatsType::MaxPosf:
            default:
                break;
        }

        if (lattStatsType < casacore::LatticeStatsBase::NSTATS) {
            casacore::LatticeStatistics<float> latticeStats = casacore::LatticeStatistics<float>(m_spectralLattice,
                /*showProgress*/ false, /*forceDisk*/ false, /*clone*/ false);
            casacore::Array<casacore::Double> result;  // has to be a Double
            latticeStats.getStatistic(result, lattStatsType);
            if (!result.empty()) {
                std::vector<double> dblResult(result.tovector());
                for (unsigned int i=0; i<dblResult.size(); ++i)  // convert to float
                    statistics.push_back(static_cast<float>(dblResult[i]));
            }
        }
    }
}

