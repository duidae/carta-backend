#pragma once

#include "AnimationQueue.h"
#include "Session.h"
#include <string>
#include <tbb/task.h>
#include <vector>

class OnMessageTask : public tbb::task {
    Session *session;
    std::string eventName;
    uint32_t requestId;
    std::vector<char> eventPayload;
    carta::AnimationQueue *aqueue;

    tbb::task* execute();

public:
    OnMessageTask(Session *session_, std::string eventName_,
                  std::vector<char> eventPayload_, uint32_t requestId_,
                  carta::AnimationQueue *aq = nullptr);
};
