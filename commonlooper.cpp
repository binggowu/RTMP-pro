#include "commonlooper.h"
#include "dlog.h"

namespace LQF
{
    // 线程函数, 就是调用子类的Loop()
    void *CommonLooper::trampoline(void *p)
    {
        LogInfo("at CommonLooper trampoline");
        ((CommonLooper *)p)->Loop();
        return NULL;
    }

    CommonLooper::CommonLooper()
    {
        request_exit_ = false;
    }

    CommonLooper::~CommonLooper()
    {
        if (running_)
        {
            LogInfo("CommonLooper deleted while still running. Some messages will not be processed");
            Stop();
        }
    }

    // 创建并启动线程
    RET_CODE CommonLooper::Start()
    {
        LogInfo("at CommonLooper create");
        worker_ = new std::thread(trampoline, this);
        if (worker_ == NULL)
        {
            LogError("new std::thread failed");
            return RET_FAIL;
        }

        running_ = true;
        return RET_OK;
    }

    // 退出线程
    void CommonLooper::Stop()
    {
        request_exit_ = true;
        if (worker_)
        {
            worker_->join();
            delete worker_;
            worker_ = NULL;
        }
        running_ = false;
    }

}
