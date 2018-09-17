#pragma once

#include <fmt/format.h>
#include <boost/multi_array.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <mutex>
#include <cstdio>
#include <uWS/uWS.h>
#include <cstdint>
#include <casacore/casa/aips.h>
#include <casacore/casa/OS/File.h>

#include <carta-protobuf/register_viewer.pb.h>
#include <carta-protobuf/file_list.pb.h>
#include <carta-protobuf/file_info.pb.h>
#include <carta-protobuf/open_file.pb.h>
#include <carta-protobuf/set_image_view.pb.h>
#include <carta-protobuf/set_image_channels.pb.h>
#include <carta-protobuf/close_file.pb.h>
#include <carta-protobuf/region_histogram.pb.h>
#include <carta-protobuf/raster_image.pb.h>

#include "ctpl.h"
#include "Frame.h"

#define MAX_SUBSETS 8

struct CompressionSettings {
    CARTA::CompressionType type;
    float quality;
    int nsubsets;
};

class Session {
public:
    boost::uuids::uuid uuid;
protected:
    // communication
    uWS::WebSocket<uWS::SERVER>* socket;
    std::vector<char> binaryPayloadCache;

    // permissions
    std::map<std::string, std::vector<std::string> >& permissionsMap;
    bool permissionsEnabled;
    std::string apiKey;

    std::string baseFolder;
    bool verboseLogging;

    // <file_id, Frame>: one frame per image file
    // TODO: clean up frames on session delete
    std::map<int, std::unique_ptr<Frame>> frames;

    // for data compression
    ctpl::thread_pool& threadPool;
    CompressionSettings compressionSettings;

public:
    Session(uWS::WebSocket<uWS::SERVER>* ws,
            boost::uuids::uuid uuid,
            std::map<std::string, std::vector<std::string>>& permissionsMap,
            bool enforcePermissions,
            std::string folder,
            ctpl::thread_pool& serverThreadPool,
            bool verbose = false);
    ~Session();

    // CARTA ICD
    void onRegisterViewer(const CARTA::RegisterViewer& message, uint32_t requestId);
    void onFileListRequest(const CARTA::FileListRequest& request, uint32_t requestId);
    void onFileInfoRequest(const CARTA::FileInfoRequest& request, uint32_t requestId);
    void onOpenFile(const CARTA::OpenFile& message, uint32_t requestId);
    void onCloseFile(const CARTA::CloseFile& message, uint32_t requestId);
    void onSetImageView(const CARTA::SetImageView& message, uint32_t requestId);
    void onSetImageChannels(const CARTA::SetImageChannels& message, uint32_t requestId);

protected:
    // ICD: File list response
    CARTA::FileListResponse getFileList(std::string folder);
    bool checkPermissionForDirectory(std:: string prefix);
    bool checkPermissionForEntry(std::string entry);

    // ICD: File info response
    bool fillFileInfo(CARTA::FileInfo* fileInfo, const std::string& filename);
    CARTA::FileType convertFileType(int ccImageType);
    bool getHduList(CARTA::FileInfo* fileInfo, casacore::String filename);
    bool fillExtendedFileInfo(CARTA::FileInfoExtended* extendedInfo, CARTA::FileInfo* fileInfo,
        const std::string folder, const std::string filename, std::string hdu, std::string& message);

    // ICD: Send raster image data, optionally with histogram
    void sendRasterImageData(int fileId, uint32_t requestId, CARTA::RegionHistogramData* channelHistogram = nullptr);
    CARTA::RegionHistogramData* getRegionHistogramData(const int32_t fileId, const int32_t regionId=-1);

    // data compression
    void setCompression(CARTA::CompressionType type, float quality, int nsubsets);

    // Send protobuf messages
    void sendEvent(std::string eventName, u_int64_t eventId, google::protobuf::MessageLite& message);
    void sendLogEvent(std::string message, std::vector<std::string> tags, CARTA::ErrorSeverity severity);
};

