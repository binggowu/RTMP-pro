#include "ringbuffer.h"

#include <assert.h>
#include <memory.h>


namespace LQF
{
// 定义
RingBuffer::RingBuffer(int size)

{
    buf_size_ = size;
    read_pos_ = 0;
    write_pos_ = 0;
    p_buf_ = new char[buf_size_];
    is_empty_ = true;
    is_full_ = false;
}
RingBuffer::~RingBuffer()
{
    delete [] p_buf_;
}
/************************************************************************/
/* 向缓冲区写入数据，返回实际写入的字节数                               */
/************************************************************************/
int RingBuffer::write(char* buf,int count)
{
    if(count <=0)
        return 0;
    is_empty_ =false;
    // 缓冲区已满，不能继续写入
    if(is_full_)
    {
        return 0;
    }
    else if(read_pos_ == write_pos_)// 缓冲区为空时
    {
        /*                          == 内存模型 ==
     (empty)             read_pos_                (empty)
  |----------------------------------|-----------------------------------------|
         write_pos_        buf_size_
  */
        int leftcount = buf_size_ - write_pos_;
        if(leftcount > count)
        {
            memcpy(p_buf_ + write_pos_, buf, count);
            write_pos_ += count;
            is_full_ =(write_pos_ == read_pos_);
            return count;
        }
        else
        {
            memcpy(p_buf_ + write_pos_, buf, leftcount);
            write_pos_ =(read_pos_ > count - leftcount)? count - leftcount : write_pos_;
            memcpy(p_buf_, buf + leftcount, write_pos_);
            is_full_ =(write_pos_ == read_pos_);
            return leftcount + write_pos_;
        }
    }
    else if(read_pos_ < write_pos_)// 有剩余空间可写入
    {
        /*                           == 内存模型 ==
   (empty)                 (data)                     (empty)
  |-------------------|----------------------------|---------------------------|
     read_pos_                write_pos_       (leftcount)
  */
        // 剩余缓冲区大小(从写入位置到缓冲区尾)

        int leftcount = buf_size_ - write_pos_;
        int test = write_pos_;
        if(leftcount > count)   // 有足够的剩余空间存放
        {
            memcpy(p_buf_ + write_pos_, buf, count);
            write_pos_ += count;
            is_full_ =(read_pos_ == write_pos_);
            assert(read_pos_ <= buf_size_);
            assert(write_pos_ <= buf_size_);
            return count;
        }
        else       // 剩余空间不足
        {
            // 先填充满剩余空间，再回头找空间存放
            memcpy(p_buf_ + test, buf, leftcount);

            write_pos_ =(read_pos_ >= count - leftcount)? count - leftcount : read_pos_;
            memcpy(p_buf_, buf + leftcount, write_pos_);
            is_full_ =(read_pos_ == write_pos_);
            assert(read_pos_ <= buf_size_);
            assert(write_pos_ <= buf_size_);
            return leftcount + write_pos_;
        }
    }
    else
    {
        /*                          == 内存模型 ==
   (unread)                 (read)                     (unread)
  |-------------------|----------------------------|---------------------------|
      write_pos_    (leftcount)    read_pos_
  */
        int leftcount = read_pos_ - write_pos_;
        if(leftcount > count)
        {
            // 有足够的剩余空间存放
            memcpy(p_buf_ + write_pos_, buf, count);
            write_pos_ += count;
            is_full_ =(read_pos_ == write_pos_);
            assert(read_pos_ <= buf_size_);
            assert(write_pos_ <= buf_size_);
            return count;
        }
        else
        {
            // 剩余空间不足时要丢弃后面的数据
            memcpy(p_buf_ + write_pos_, buf, leftcount);
            write_pos_ += leftcount;
            is_full_ =(read_pos_ == write_pos_);
            assert(is_full_);
            assert(read_pos_ <= buf_size_);
            assert(write_pos_ <= buf_size_);
            return leftcount;
        }
    }
}
/************************************************************************/
/* 从缓冲区读数据，返回实际读取的字节数                                 */
/************************************************************************/
int RingBuffer::read(char* buf,int count)
{
    if(count <=0)
        return 0;
    is_full_ =false;
    if(is_empty_)       // 缓冲区空，不能继续读取数据
    {
        return 0;
    }
    else if(read_pos_ == write_pos_)   // 缓冲区满时
    {
        /*                          == 内存模型 ==
   (data)          read_pos_                (data)
|--------------------------------|--------------------------------------------|
    write_pos_         buf_size_
  */
        int leftcount = buf_size_ - read_pos_;
        if(leftcount > count)
        {
            memcpy(buf, p_buf_ + read_pos_, count);
            read_pos_ += count;
            is_empty_ =(read_pos_ == write_pos_);
            return count;
        }
        else
        {
            memcpy(buf, p_buf_ + read_pos_, leftcount);
            read_pos_ =(write_pos_ > count - leftcount)? count - leftcount : write_pos_;
            memcpy(buf + leftcount, p_buf_, read_pos_);
            is_empty_ =(read_pos_ == write_pos_);
            return leftcount + read_pos_;
        }
    }
    else if(read_pos_ < write_pos_)   // 写指针在前(未读数据是连接的)
    {
        /*                          == 内存模型 ==
   (read)                 (unread)                      (read)
  |-------------------|----------------------------|---------------------------|
     read_pos_                write_pos_                     buf_size_
  */
        int leftcount = write_pos_ - read_pos_;
        int c =(leftcount > count)? count : leftcount;
        memcpy(buf, p_buf_ + read_pos_, c);
        read_pos_ += c;
        is_empty_ =(read_pos_ == write_pos_);
        assert(read_pos_ <= buf_size_);
        assert(write_pos_ <= buf_size_);
        return c;
    }
    else          // 读指针在前(未读数据可能是不连接的)
    {
        /*                          == 内存模型 ==
     (unread)                (read)                      (unread)
  |-------------------|----------------------------|---------------------------|
      write_pos_                  read_pos_                  buf_size_

  */
        int leftcount = buf_size_ - read_pos_;
        if(leftcount > count)   // 未读缓冲区够大，直接读取数据
        {
            memcpy(buf, p_buf_ + read_pos_, count);
            read_pos_ += count;
            is_empty_ =(read_pos_ == write_pos_);
            assert(read_pos_ <= buf_size_);
            assert(write_pos_ <= buf_size_);
            return count;
        }
        else       // 未读缓冲区不足，需回到缓冲区头开始读
        {
            memcpy(buf, p_buf_ + read_pos_, leftcount);
            read_pos_ =(write_pos_ >= count - leftcount)? count - leftcount : write_pos_;
            memcpy(buf + leftcount, p_buf_, read_pos_);
            is_empty_ =(read_pos_ == write_pos_);
            assert(read_pos_ <= buf_size_);
            assert(write_pos_ <= buf_size_);
            return leftcount + read_pos_;
        }
    }
}
/************************************************************************/
/* 获取缓冲区有效数据长度                                               */
/************************************************************************/
int RingBuffer::getLength()
{
    if(is_empty_)
    {
        return 0;
    }
    else if(is_full_)
    {
        return buf_size_;
    }
    else if(read_pos_ < write_pos_)
    {
        return write_pos_ - read_pos_;
    }
    else
    {
        return buf_size_ - read_pos_ + write_pos_;
    }
}
void RingBuffer::empty()
{
    read_pos_ =0;
    write_pos_ =0;
    is_empty_ =true;
    is_full_ =false;
}
bool RingBuffer::isEmpty()
{
    return is_empty_;
}
bool RingBuffer::isFull()
{
    return is_full_;
}
}
