#pragma once

#include "AnimationQueue.h"
#include "Session.h"
#include <string>
#include <tbb/concurrent_queue.h>
#include <tbb/task.h>
#include <vector>

class OnMessageTask : public tbb::task {
    Session *session;
    tbb::concurrent_queue<std::tuple<std::string,uint32_t,std::vector<char>>> &mqueue;
    carta::AnimationQueue *aqueue;

    tbb::task* execute();

public:
    OnMessageTask(Session *session_,
                  tbb::concurrent_queue<std::tuple<std::string,uint32_t,std::vector<char>>> &mq,
                  carta::AnimationQueue *aq = nullptr);
};
