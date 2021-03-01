#ifndef NALULOOP_H
#define NALULOOP_H

#include "looper.h"

namespace LQF
{
class NaluLoop : public Looper
{
public:
    NaluLoop(int queue_nalu_len);
private:
    virtual void addmsg(LooperMessage *msg, bool flush);
private:
    int max_nalu_;
};
}
#endif // NALULOOP_H
