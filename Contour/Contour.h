#pragma once

#include <casacore/lattices/Lattices/SubLattice.h>
#include <carta-protobuf/defs.pb.h>
#include <vector>

namespace carta {

enum Smoothing {unity, block, gaussian};

google::protobuf::RepeatedPtrField<CARTA::ContourSet>
gatherContours(casacore::SubLattice<float> &lattice, const std::vector<float> &levels,
               CARTA::ContourMode mode, float smoothness);

} // namespace carta
