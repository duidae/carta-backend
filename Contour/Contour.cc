#include "Contour.h"
#include <casacore/casa/Arrays/Matrix.h>
#include <cmath>
#include <tbb/blocked_range2d.h>
#include <tbb/parallel_for.h>

enum Edge {top, right, bottom, left, none};

casacore::Matrix<float> downSample(const casacore::Matrix<float> &matrix, int mip) {
    casacore::IPosition shape = matrix.shape();
    size_t numRowsRegion = shape(0) / mip;
    size_t rowLengthRegion = shape(1) / mip;
    casacore::Matrix<float> dsmat(numRowsRegion, rowLengthRegion);

    // Perform down-sampling by calculating the mean for each MIPxMIP block
    auto range = tbb::blocked_range2d<size_t>(0, numRowsRegion, 0, rowLengthRegion);
    auto loop = [&](const tbb::blocked_range2d<size_t> &r) {
        for(size_t j = r.rows().begin(); j != r.rows().end(); ++j) {
            for(size_t i = r.cols().begin(); i != r.cols().end(); ++i) {
                float pixelSum = 0;
                int pixelCount = 0;
                for (auto pixelX = 0; pixelX < mip; pixelX++) {
                    for (auto pixelY = 0; pixelY < mip; pixelY++) {
                        auto imageRow = j * mip + pixelY;
                        auto imageCol = i * mip + pixelX;
                        float pixVal = matrix(imageCol, imageRow);
                        if (!std::isnan(pixVal)) {
                            pixelCount++;
                            pixelSum += pixVal;
                        }
                    }
                }
                dsmat(i,j) = pixelCount ? pixelSum / pixelCount : std::numeric_limits<float>::quiet_NaN();
            }
        }
    };
    tbb::parallel_for(range, loop);
    return dsmat;
}

void traceContour(const casacore::Matrix<float> &matrix, float level,
                  size_t xstart, size_t ystart, int edgeStart,
                  casacore::Matrix<bool> &visited,
                  CARTA::ContourSet &contourSet) {
    size_t ii = xstart;
    size_t jj = ystart;
    int edge = edgeStart;

    casacore::IPosition shape = matrix.shape();

    bool init = true;
    bool done = ii < 0 || ii >= shape(0) || (jj < 0 && jj >= shape(1));

    std::vector<float> points;

    while(!done) {
        bool flag = false;
        float a = matrix(ii,jj);
        float b = matrix(ii+1,jj);
        float c = matrix(ii+1,jj+1);
        float d = matrix(ii,jj+1);

        float x, y;
        if(init) {
            init = false;
            switch(edge) {
            case Edge::top:
                x = (level-a) / (b-a) + ii;
                y = jj;
                break;
            case Edge::right:
                x = ii+1;
                y = (level-b) / (c-b) + jj;
                break;
            case Edge::bottom:
                x = (level-c) / (d-c) + ii;
                y = jj+1;
                break;
            case Edge::left:
                x = ii;
                y = (level-a) / (d-a) + jj;
                break;
            }
            continue;
        }

        if(edge == Edge::top) {
            visited(ii,jj) = true;
        }
        do {
            if(++edge == Edge::none) {
                edge = Edge::top;
            }

            switch(edge) {
            case Edge::top:
                if(a >= level && level > b) {
                    flag = true;
                    x = (level-a) / (b-a) + ii;
                    y = jj;
                    --jj;
                }
                break;
            case Edge::right:
                if(b >= level && level > c) {
                    flag = true;
                    x = ii+1;
                    y = (level-b) / (c-b) + jj;
                    ++ii;
                }
                break;
            case Edge::bottom:
                if(c >= level && level > d) {
                    flag = true;
                    x = (level-c) / (d-c) + ii;
                    y = jj+1;
                    ++jj;
                }
                break;
            case Edge::left:
                if(d >= level && level > a) {
                    flag = true;
                    x = ii;
                    y = (level-a) / (d-a) + jj;
                    --ii;
                }
                break;
            }
        } while(!flag);

        if(++edge == Edge::none) {
            edge = Edge::top;
        }
        if(++edge == Edge::none) {
            edge = Edge::top;
        }
        done |= (ii == xstart && jj == ystart && edge == edgeStart);
        done |= (ii < 0 || ii >= shape(0) || jj < 0 || jj >= shape(1));

        contourSet.add_coordinates(x+0.5);
        contourSet.add_coordinates(y+0.5);
    }
}

CARTA::ContourSet traceLevel(const casacore::Matrix<float> &matrix, float level) {
    casacore::IPosition shape = matrix.shape();
    CARTA::ContourSet contourSet;
    contourSet.set_value(level);
    casacore::Matrix<bool> visited(shape, false);
    // Search edges first
    auto plot_func = [&contourSet](float x, float y) {
        contourSet.add_coordinates(x);
        contourSet.add_coordinates(y);
    };
    // Top
    std::cout << "Searching top edge..." << std::endl;
    int64_t i, j;
    for(i=0, j=0; i < shape(0)-1; ++i) {
        if(matrix(i,j) < level && level <= matrix(i+1,j)) {
            contourSet.add_start_indices(contourSet.coordinates().size());
            traceContour(matrix, level, i, j, Edge::top, visited, contourSet);
        }
    }
    // Right
    std::cout << "Searching right edge..." << std::endl;
    for(j=0; j < shape(1)-1; ++j) {
        if(matrix(i,j) < level && level <= matrix(i,j+1)) {
            contourSet.add_start_indices(contourSet.coordinates().size());
            traceContour(matrix, level, i-1, j, Edge::right, visited, contourSet);
        }
    }
    // Bottom
    std::cout << "Searching bottom edge..." << std::endl;
    for(--i; i >= 0; --i) {
        if(matrix(i+1,j) < level && level <= matrix(i,j)) {
            contourSet.add_start_indices(contourSet.coordinates().size());
            traceContour(matrix, level, i, j-1, Edge::bottom, visited, contourSet);
        }
    }
    // Left
    std::cout << "Searching left edge..." << std::endl;
    for(i=0, --j; j >= 0; --j) {
        if(matrix(i,j+1) < level && level <= matrix(i,j)) {
            contourSet.add_start_indices(contourSet.coordinates().size());
            traceContour(matrix, level, i, j, Edge::left, visited, contourSet);
        }
    }
    // Search the rest of the image
    std::cout << "Searching image..." << std::endl;
    for(j=0; j < shape(1)-1; ++j) {
        for(i=0; i < shape(0)-1; ++i) {
            if(!visited(i,j) && matrix(i,j) < level && level <= matrix(i+1,j)) {
                contourSet.add_start_indices(contourSet.coordinates().size());
                traceContour(matrix, level, i, j, Edge::top, visited, contourSet);
            }
        }
    }
    std::cout << "Done searching" << std::endl;
    return contourSet;
}

google::protobuf::RepeatedPtrField<CARTA::ContourSet>
carta::gatherContours(casacore::SubLattice<float> &lattice, const std::vector<float> &levels,
                      CARTA::ContourMode contourMode, float smoothness) {
    casacore::Array<float> matrix;
    lattice.get(matrix, true);
    casacore::Matrix<float> downsampled;
    if(smoothness == 1 || contourMode == CARTA::ContourMode::ORIGINAL) {
        // Unity
        downsampled = matrix;
    } else if(contourMode == CARTA::ContourMode::BOXBLUR_3 ||
              contourMode == CARTA::ContourMode::BOXBLUR_5) {
        // Block
        // TBF: Map smoothness to an integer intelligently (based on image parameters)?
        downsampled = downSample(matrix, int(smoothness));
    }// else {
    //     // Gaussian
    // }

    google::protobuf::RepeatedPtrField<CARTA::ContourSet> contours;
    // contours.Reserve(levels.size());
    for(float level : levels) {
        *(contours.Add()) = traceLevel(downsampled, level);
    }
    return contours;
}
