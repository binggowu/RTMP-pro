#include "naluloop.h"
namespace LQF
{
NaluLoop::NaluLoop(int queue_nalu_len):
    max_nalu_(queue_nalu_len),Looper()
{}

//队列里面既有音频，也有视频
void NaluLoop::addmsg(LooperMessage *msg, bool flush)
{
    queue_mutex_.lock();
    if (flush)
    {
        msg_queue_.clear();
    }
    if(msg_queue_.size() >= max_nalu_)  //移除消息,直到下一个I帧，或者队列为空
    {

//        LooperMessage *tempMsg =  msg_queue_.front();
//        msg_queue_.pop_front();
//        delete (NaluStruct*)tempMsg->obj;
//        delete tempMsg;

        while(msg_queue_.size() > 0)
        {
            LooperMessage *tempMsg =  msg_queue_.front();
            // 从I帧开始
            if(tempMsg->what == RTMP_BODY_VID_RAW && ((NaluStruct*)tempMsg->obj)->type == 5)
            {
                LogWarn("drop msg, now have %d msg", msg_queue_.size());
                break;
            }
            msg_queue_.pop_front();

            delete tempMsg->obj;
            delete tempMsg;
        }
    }
    msg_queue_.push_back(msg);
    if(msg_queue_.size() > max_nalu_/3)
        LogWarn("have %d msg cache, it will cause delay", msg_queue_.size());
    queue_mutex_.unlock();
    head_data_available_->post();
}
}
