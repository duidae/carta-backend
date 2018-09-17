#include "Session.h"
#include "FileExtInfoLoader.h"

#include <carta-protobuf/raster_image.pb.h>
#include <carta-protobuf/region_histogram.pb.h>
#include <carta-protobuf/error.pb.h>

#include <casacore/casa/OS/Path.h>
#include <casacore/casa/OS/DirectoryIterator.h>
#include <casacore/images/Images/ImageOpener.h>
#include <casacore/images/Images/FITSImgParser.h>

using namespace std;
using namespace CARTA;

// Default constructor. Associates a websocket with a UUID and sets the base folder for all files
Session::Session(uWS::WebSocket<uWS::SERVER>* ws, boost::uuids::uuid uuid, map<string, vector<string>>& permissionsMap, bool enforcePermissions, string folder, ctpl::thread_pool& serverThreadPool, bool verbose)
    : uuid(uuid),
      permissionsMap(permissionsMap),
      permissionsEnabled(enforcePermissions),
      baseFolder(folder),
      verboseLogging(verbose),
      threadPool(serverThreadPool),
      rateSum(0),
      rateCount(0),
      socket(ws) {
}

Session::~Session() {

}

bool Session::checkPermissionForEntry(string entry) {
    // skip permissions map if we're not running with permissions enabled
    if (!permissionsEnabled) {
        return true;
    }
    if (!permissionsMap.count(entry)) {
        return false;
    }
    auto& keys = permissionsMap[entry];
    return (find(keys.begin(), keys.end(), "*") != keys.end()) || (find(keys.begin(), keys.end(), apiKey) != keys.end());
}

// Checks whether the user's API key is valid for a particular directory.
// This function is called recursively, starting with the requested directory, and then working
// its way up parent directories until it finds a matching directory in the permissions map.
bool Session::checkPermissionForDirectory(std::string prefix) {
    // skip permissions map if we're not running with permissions enabled
    if (!permissionsEnabled) {
        return true;
    }
    // Check for root folder permissions
    if (!prefix.length() || prefix == "/") {
        if (permissionsMap.count("/")) {
            return checkPermissionForEntry("/");
        }
        return false;
    } else {
        // trim trailing and leading slash
        if (prefix[prefix.length() - 1] == '/') {
            prefix = prefix.substr(0, prefix.length() - 1);
        }
        if (prefix[0] == '/') {
            prefix = prefix.substr(1);
        }
        while (prefix.length() > 0) {
            if (permissionsMap.count(prefix)) {
                return checkPermissionForEntry(prefix);
            }
            auto lastSlash = prefix.find_last_of('/');

            if (lastSlash == string::npos) {
                return false;
            } else {
                prefix = prefix.substr(0, lastSlash);
            }
        }
        return false;
    }
}

FileListResponse Session::getFileList(string folder) {
    // fill FileListResponse
    casacore::Path fullPath(baseFolder);
    FileListResponse fileList;
    if (folder.length() && folder != "/") {
        fullPath.append(folder);
        fileList.set_directory(folder);
        fileList.set_parent(fullPath.dirName());
    }

    casacore::File folderPath(fullPath);
    string message;

    try {
        if (checkPermissionForDirectory(folder) && folderPath.exists() && folderPath.isDirectory()) {
            casacore::Directory startDir(fullPath);
            casacore::DirectoryIterator dirIter(startDir);
            while (!dirIter.pastEnd()) {
                casacore::File ccfile(dirIter.file());  // casacore File
                casacore::String fullpath(ccfile.path().absoluteName());
                casacore::ImageOpener::ImageTypes imType = casacore::ImageOpener::imageType(fullpath);
                bool addImage(false);
                if (ccfile.isDirectory()) {
                    if ((imType==casacore::ImageOpener::AIPSPP) || (imType==casacore::ImageOpener::MIRIAD))
                        addImage = true;
                    else if (imType==casacore::ImageOpener::UNKNOWN) {
                        // Check if it is a directory and the user has permission to access it
                        casacore::String dirname(ccfile.path().baseName());
                        string pathNameRelative = (folder.length() && folder != "/") ? folder.append("/" + dirname) : dirname;
                        if (checkPermissionForDirectory(pathNameRelative))
                           fileList.add_subdirectories(dirname);
                    }
                } else if (ccfile.isRegular() &&
                    ((imType==casacore::ImageOpener::FITS) || (imType==casacore::ImageOpener::HDF5))) {
                        addImage = true;
                }

                if (addImage) { // add image to file list
                    auto fileInfo = fileList.add_files();
                    fillFileInfo(fileInfo, ccfile);
                }
                dirIter++;
            }
        } else {
            fileList.set_success(false);
            fileList.set_message("Cannot read directory; check name and permissions.");
            return fileList;
        }
    } catch (casacore::AipsError& err) {
        fmt::print("Error: {}\n", err.getMesg().c_str());
        sendLogEvent(err.getMesg(), {"file-list"}, CARTA::ErrorSeverity::ERROR);
        fileList.set_success(false);
        fileList.set_message(err.getMesg());
        return fileList;
    }
    fileList.set_success(true);
    return fileList;
}

bool Session::fillFileInfo(FileInfo* fileInfo, casacore::File& ccfile) {
    // fill FileInfo submessage
    bool fileInfoOK(true);
    fileInfo->set_size(ccfile.size());
    fileInfo->set_name(ccfile.path().baseName());
    casacore::String absFileName(ccfile.path().absoluteName());
    casacore::ImageOpener::ImageTypes imType = casacore::ImageOpener::imageType(absFileName);
    fileInfo->set_type(convertFileType(imType));
    fileInfoOK = getHduList(fileInfo, absFileName);
    return fileInfoOK;
}

CARTA::FileType Session::convertFileType(int ccImageType) {
    // convert casacore ImageType to protobuf FileType
    switch (ccImageType) {
        case casacore::ImageOpener::FITS:
            return FileType::FITS;
        case casacore::ImageOpener::AIPSPP:
            return FileType::CASA;
        case casacore::ImageOpener::HDF5:
            return FileType::HDF5;
        case casacore::ImageOpener::MIRIAD:
            return FileType::MIRIAD;
        default:
            return FileType::UNKNOWN;
    }
}

bool Session::getHduList(FileInfo* fileInfo, casacore::String filename) {
    // fill FileInfo hdu list
    bool hduOK(true);
    if (fileInfo->type()==CARTA::HDF5) {
        casacore::HDF5File hdfFile(filename);
        std::vector<casacore::String> hdus(casacore::HDF5Group::linkNames(hdfFile));
        for (auto groupName : hdus)
            fileInfo->add_hdu_list(groupName);
        hduOK = (fileInfo->hdu_list_size() > 0);
    } else if (fileInfo->type()==CARTA::FITS) {
        casacore::FITSImgParser fitsParser(filename.c_str());
        int numHdu(fitsParser.get_numhdu());
        for (int hdu=0; hdu<numHdu; ++hdu) {
            fileInfo->add_hdu_list(casacore::String::toString(hdu));
        }
        hduOK = (fileInfo->hdu_list_size() > 0);
    } else {
        fileInfo->add_hdu_list("");
    }
    return hduOK;
}

bool Session::fillExtendedFileInfo(FileInfoExtended* extendedInfo, FileInfo* fileInfo, 
        const string folder, const string filename, string hdu, string& message) {
    // fill FileInfoResponse submessages FileInfo and FileInfoExtended
    bool extFileInfoOK(true);
    casacore::Path ccpath(baseFolder);
    ccpath.append(folder);
    ccpath.append(filename);
    casacore::File ccfile(ccpath);
    try {
        if (!fillFileInfo(fileInfo, ccfile)) {
             return false;
        }
        casacore::String fullname(ccfile.path().absoluteName());
        FileExtInfoLoader extLoader(fullname, hdu);
        extFileInfoOK = extLoader.fillFileExtInfo(extendedInfo, message);
    } catch (casacore::AipsError& ex) {
        message = ex.getMesg();
        extFileInfoOK = false;
    }
    return extFileInfoOK;
}

// *********************************************************************************
// CARTA ICD implementation

void Session::onRegisterViewer(const RegisterViewer& message, uint32_t requestId) {
    apiKey = message.api_key();
    RegisterViewerAck ackMessage;
    ackMessage.set_success(true);
    ackMessage.set_session_id(boost::uuids::to_string(uuid));
    sendEvent("REGISTER_VIEWER_ACK", requestId, ackMessage);
}

void Session::onFileListRequest(const FileListRequest& request, uint32_t requestId) {
    string folder = request.directory();
    // strip baseFolder from folder
    string basePath(baseFolder);
    if (basePath.back()=='/') basePath.pop_back();
    if (folder.find(basePath)==0) {
        folder.replace(0, basePath.length(), "");
	if (folder.front()=='/') folder.replace(0,1,""); // remove leading '/'
    }
    FileListResponse response = getFileList(folder);
    sendEvent("FILE_LIST_RESPONSE", requestId, response);
}

void Session::onFileInfoRequest(const FileInfoRequest& request, uint32_t requestId) {
    FileInfoResponse response;
    auto fileInfo = response.mutable_file_info();
    auto fileInfoExtended = response.mutable_file_info_extended();
    string message;
    bool success = fillExtendedFileInfo(fileInfoExtended, fileInfo, request.directory(), request.file(), request.hdu(), message);
    response.set_success(success);
    response.set_message(message);
    sendEvent("FILE_INFO_RESPONSE", requestId, response);
}

void Session::onOpenFile(const OpenFile& message, uint32_t requestId) {
    OpenFileAck ack;
    ack.set_file_id(message.file_id());
    auto fileInfo = ack.mutable_file_info();
    auto fileInfoExtended = ack.mutable_file_info_extended();
    string errMessage;
    bool infoSuccess = fillExtendedFileInfo(fileInfoExtended, fileInfo, message.directory(), message.file(), message.hdu(), errMessage);
    if (infoSuccess && fileInfo->hdu_list_size()) {
	casacore::Path path(baseFolder);
	path.append(message.directory());
	path.append(message.file());
        string filename(path.absoluteName());
        string hdu = fileInfo->hdu_list(0);
        auto frame = unique_ptr<Frame>(new Frame(boost::uuids::to_string(uuid), filename, hdu));
        if (frame->isValid()) {
            ack.set_success(true);
            frames[message.file_id()] = move(frame);
        } else {
            ack.set_success(false);
            ack.set_message("Could not load file");
        }
    } else {
        ack.set_success(false);
        ack.set_message(errMessage);
    }
    sendEvent("OPEN_FILE_ACK", requestId, ack);
}

CARTA::RegionHistogramData* Session::getRegionHistogram(const int32_t fileId, const int32_t regionId) {
    RegionHistogramData* histogramMessage(nullptr);
    if (frames.count(fileId)) {
        histogramMessage = new RegionHistogramData();
        histogramMessage->set_file_id(fileId);
        // default -1 corresponds to the entire current XY plane
        histogramMessage->set_region_id(regionId);
        auto& frame = frames[fileId];
        histogramMessage->set_stokes(frame->currentStokes());
        histogramMessage->mutable_histograms()->AddAllocated(new Histogram(frames[fileId]->currentHistogram()));
    }
    return histogramMessage;
}

void Session::onCloseFile(const CloseFile& message, uint32_t requestId) {
    auto id = message.file_id();
    if (id == -1) {
        frames.clear();
    } else if (frames.count(id)) {
        frames[id].reset();
    }
}

void Session::onSetImageView(const SetImageView& message, uint32_t requestId) {
    compressionType = message.compression_type();
    compressionQuality = message.compression_quality();
    numSubsets = message.num_subsets();

    // Check if frame is loaded
    if (frames.count(message.file_id())) {
        auto& frame = frames[message.file_id()];
        CARTA::ImageBounds frameBounds(frame->currentBounds());
        CARTA::RegionHistogramData* histogram(nullptr);
	// send histogram for new frame
	if (frameBounds.x_max()==0 && frameBounds.y_max()==0)
            histogram = getRegionHistogram(message.file_id());
        if (!frame->setBounds(message.image_bounds(), message.mip())) {
            // TODO: Error handling on bounds
        }
        sendImageData(message.file_id(), requestId, histogram);
    } else {
        // TODO: error handling
    }

}

void Session::sendImageData(int fileId, uint32_t requestId, CARTA::RegionHistogramData* channelHistogram) {
    RasterImageData rasterImageData;
    // Add histogram, if it exists
    if (channelHistogram) {
        rasterImageData.set_allocated_channel_histogram_data(channelHistogram);
    }
    if (frames.count(fileId)) {
        auto& frame = frames[fileId];
        auto imageData = frame->getImageData();
        // Check if image data is valid
        if (!imageData.empty()) {
            rasterImageData.set_file_id(fileId);
            rasterImageData.set_stokes(frame->currentStokes());
            rasterImageData.set_channel(frame->currentChannel());
            rasterImageData.set_mip(frame->currentMip());
            // Copy over image bounds
            auto imageBounds = frame->currentBounds();
            auto mip = frame->currentMip();
            rasterImageData.mutable_image_bounds()->set_x_min(imageBounds.x_min());
            rasterImageData.mutable_image_bounds()->set_x_max(imageBounds.x_max());
            rasterImageData.mutable_image_bounds()->set_y_min(imageBounds.y_min());
            rasterImageData.mutable_image_bounds()->set_y_max(imageBounds.y_max());

            if (compressionType == CompressionType::NONE) {
                rasterImageData.set_compression_type(CompressionType::NONE);
                rasterImageData.set_compression_quality(0);
                rasterImageData.add_image_data(imageData.data(), imageData.size() * sizeof(float));
            } else if (compressionType == CompressionType::ZFP) {

                int precision = lround(compressionQuality);
                auto rowLength = (imageBounds.x_max() - imageBounds.x_min()) / mip;
                auto numRows = (imageBounds.y_max() - imageBounds.y_min()) / mip;
                rasterImageData.set_compression_type(CompressionType::ZFP);
                rasterImageData.set_compression_quality(precision);

                vector<size_t> compressedSizes(numSubsets);
                vector<vector<int32_t>> nanEncodings(numSubsets);
                vector<future<size_t>> futureSizes;

                auto tStartCompress = chrono::high_resolution_clock::now();
                int N = min(numSubsets, MAX_SUBSETS);;
                for (auto i = 0; i < N; i++) {
                    auto& compressionBuffer = compressionBuffers[i];
                    futureSizes.push_back(threadPool.push(0,0,[&nanEncodings, &imageData, &compressionBuffer, numRows, N, rowLength, i, precision](int) {
                        int subsetRowStart = i * (numRows / N);
                        int subsetRowEnd = (i + 1) * (numRows / N);
                        if (i == N - 1) {
                            subsetRowEnd = numRows;
                        }
                        int subsetElementStart = subsetRowStart * rowLength;
                        int subsetElementEnd = subsetRowEnd * rowLength;

                        size_t compressedSize;
                        // nanEncodings[i] = getNanEncodingsSimple(imageData, subsetElementStart, subsetElementEnd - subsetElementStart);
                        nanEncodings[i] = getNanEncodingsBlock(imageData, subsetElementStart, rowLength, subsetRowEnd - subsetRowStart);
                        compress(imageData, subsetElementStart, compressionBuffer, compressedSize, rowLength, subsetRowEnd - subsetRowStart, precision);
                        return compressedSize;
                    }));
                }

                // Wait for completed compression threads
                for (auto i = 0; i < numSubsets; i++) {
                    compressedSizes[i] = futureSizes[i].get();

                }
                auto tEndCompress = chrono::high_resolution_clock::now();
                auto dtCompress = chrono::duration_cast<chrono::microseconds>(tEndCompress - tStartCompress).count();

                // Complete message
                for (auto i = 0; i < numSubsets; i++) {
                    rasterImageData.add_image_data(compressionBuffers[i].data(), compressedSizes[i]);
                    rasterImageData.add_nan_encodings((char*) nanEncodings[i].data(), nanEncodings[i].size() * sizeof(int));
                }

                if (verboseLogging) {
                    string compressionInfo = fmt::format("Image data of size {:.1f} kB compressed to {:.1f} kB in {} ms at {:.2f} MPix/s\n",
                               numRows * rowLength * sizeof(float) / 1e3,
                               accumulate(compressedSizes.begin(), compressedSizes.end(), 0) * 1e-3,
                               1e-3 * dtCompress,
                               (float) (numRows * rowLength) / dtCompress);
                    fmt::print(compressionInfo);
                    sendLogEvent(compressionInfo, {"zfp"}, CARTA::ErrorSeverity::DEBUG);
                }
            } else {
                // TODO: error handling for SZ
            }
            // Send completed event to client
            sendEvent("RASTER_IMAGE_DATA", requestId, rasterImageData);
        }
    }
}

void Session::onSetImageChannels(const CARTA::SetImageChannels& message, uint32_t requestId) {
    if (frames.count(message.file_id())) {
        auto& frame = frames[message.file_id()];
        if (!frame->setChannels(message.channel(), message.stokes())) {
            // TODO: Error handling on bounds
        }
        // Send updated histogram
        RegionHistogramData* histogramMessage = getRegionHistogram(message.file_id());
        // Histogram message now managed by the image data object
        sendImageData(message.file_id(), requestId, histogramMessage);
    } else {
        // TODO: error handling
    }
}

// *********************************************************************************
// SEND uWEBSOCKET MESSAGES

// Sends an event to the client with a given event name (padded/concatenated to 32 characters) and a given ProtoBuf message
void Session::sendEvent(string eventName, u_int64_t eventId, google::protobuf::MessageLite& message) {
    size_t eventNameLength = 32;
    int messageLength = message.ByteSize();
    size_t requiredSize = eventNameLength + 8 + messageLength;
    if (binaryPayloadCache.size() < requiredSize) {
        binaryPayloadCache.resize(requiredSize);
    }
    memset(binaryPayloadCache.data(), 0, eventNameLength);
    memcpy(binaryPayloadCache.data(), eventName.c_str(), min(eventName.length(), eventNameLength));
    memcpy(binaryPayloadCache.data() + eventNameLength, &eventId, 4);
    message.SerializeToArray(binaryPayloadCache.data() + eventNameLength + 8, messageLength);
    socket->send(binaryPayloadCache.data(), requiredSize, uWS::BINARY);
}

void Session::sendLogEvent(std::string message, std::vector<std::string> tags, CARTA::ErrorSeverity severity) {
    CARTA::ErrorData errorData;
    errorData.set_message(message);
    errorData.set_severity(severity);
    *errorData.mutable_tags() = {tags.begin(), tags.end()};
    sendEvent("ERROR_DATA", 0, errorData);
}
