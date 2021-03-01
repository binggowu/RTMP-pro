#ifndef COMMONLOOPER_H
#define COMMONLOOPER_H
#include <thread>
#include "mediabase.h"

namespace LQF
{
    class CommonLooper
    {
    public:
        CommonLooper();
        virtual ~CommonLooper();
        virtual RET_CODE Start();
        virtual void Stop();
        virtual void Loop() = 0;

    private:
        static void *trampoline(void *p);

    protected:
        std::thread *worker_;
        bool request_exit_ = false;
        bool running_ = false;
    };

}

#endif // COMMONLOOPER_H
