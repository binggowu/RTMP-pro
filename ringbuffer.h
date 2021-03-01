#ifndef RINGBUFFER_H
#define RINGBUFFER_H
namespace LQF
{
class RingBuffer
{
public:
    bool isFull();
    bool isEmpty();
    void empty();
    int getLength();
    RingBuffer(int size);
    virtual~RingBuffer();
    int write(char* buf,int count);
    int read(char* buf,int count);

private:
    bool is_empty_ = true;
    bool is_full_ = false;
    char* p_buf_;
    int buf_size_;
    int read_pos_;
    int write_pos_;
};
}
#endif // RINGBUFFER_H
