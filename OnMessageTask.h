#pragma once

#include "Session.h"
#include <string>
#include <tbb/task.h>
#include <vector>

class OnMessageTask : public tbb::task {
    Session *session;
    string eventName;
    uint32_t requestId;
    std::vector<char> eventPayload;

    tbb::task* execute();

public:
    OnMessageTask(Session *session_, char *rawMessage_, size_t length_);
};
