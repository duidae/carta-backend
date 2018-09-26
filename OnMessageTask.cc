#include "OnMessageTask.h"
#include <algorithm>
#include <carta-protobuf/close_file.pb.h>
#include <carta-protobuf/file_info.pb.h>
#include <carta-protobuf/file_list.pb.h>
#include <carta-protobuf/open_file.pb.h>
#include <carta-protobuf/raster_image.pb.h>
#include <carta-protobuf/region_histogram.pb.h>
#include <carta-protobuf/region_requirements.pb.h>
#include <carta-protobuf/register_viewer.pb.h>
#include <carta-protobuf/set_cursor.pb.h>
#include <carta-protobuf/set_image_channels.pb.h>
#include <carta-protobuf/set_image_view.pb.h>
#include <cstring>

// Looks for null termination in a char array to determine event names from message payloads
std::string getEventName(char* rawMessage) {
    static const size_t max_len = 32;
    return std::string(rawMessage, std::min(std::strlen(rawMessage), max_len));
}

OnMessageTask::OnMessageTask(Session *session_, char *rawMessage_, size_t length_)
    : session(session_),
      eventName(getEventName(rawMessage_)),
      requestId(*reinterpret_cast<uint32_t*>(rawMessage_+32)),
      eventPayload(&rawMessage_[36], &rawMessage_[length_])
{}

tbb::task* OnMessageTask::execute() {
    //CARTA ICD
    if (eventName == "REGISTER_VIEWER") {
        CARTA::RegisterViewer message;
        if (message.ParseFromArray(eventPayload.data(), eventPayload.size())) {
            session->onRegisterViewer(message, requestId);
        }
    } else if (eventName == "FILE_LIST_REQUEST") {
        CARTA::FileListRequest message;
        if (message.ParseFromArray(eventPayload.data(), eventPayload.size())) {
            session->onFileListRequest(message, requestId);
        }
    } else if (eventName == "FILE_INFO_REQUEST") {
        CARTA::FileInfoRequest message;
        if (message.ParseFromArray(eventPayload.data(), eventPayload.size())) {
            session->onFileInfoRequest(message, requestId);
        }
    } else if (eventName == "OPEN_FILE") {
        CARTA::OpenFile message;
        if (message.ParseFromArray(eventPayload.data(), eventPayload.size())) {
            session->onOpenFile(message, requestId);
        }
    } else if (eventName == "CLOSE_FILE") {
        CARTA::CloseFile message;
        if (message.ParseFromArray(eventPayload.data(), eventPayload.size())) {
            session->onCloseFile(message, requestId);
        }
    } else if (eventName == "SET_IMAGE_VIEW") {
        CARTA::SetImageView message;
        if (message.ParseFromArray(eventPayload.data(), eventPayload.size())) {
            session->onSetImageView(message, requestId);
        }
    } else if (eventName == "SET_IMAGE_CHANNELS") {
        CARTA::SetImageChannels message;
        if (message.ParseFromArray(eventPayload.data(), eventPayload.size())) {
            session->onSetImageChannels(message, requestId);
        }
    } else if (eventName == "SET_CURSOR") {
        CARTA::SetCursor message;
        if (message.ParseFromArray(eventPayload.data(), eventPayload.size())) {
            session->onSetCursor(message, requestId);
        }
    } else if (eventName == "SET_SPATIAL_REQUIREMENTS") {
        CARTA::SetSpatialRequirements message;
        if (message.ParseFromArray(eventPayload.data(), eventPayload.size())) {
            session->onSetSpatialRequirements(message, requestId);
        }
    } else if (eventName == "SET_HISTOGRAM_REQUIREMENTS") {
        CARTA::SetHistogramRequirements message;
        if (message.ParseFromArray(eventPayload.data(), eventPayload.size())) {
            session->onSetHistogramRequirements(message, requestId);
        }
    } else {
        fmt::print("Unknown event type {}\n", eventName);
    }
    return nullptr;
}
