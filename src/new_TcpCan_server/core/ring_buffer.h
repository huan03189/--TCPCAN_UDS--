#ifndef RING_BUFFER_H
#define RING_BUFFER_H
#include<atomic>
#include<cstring>
#define RING_BUF_SIZE 512
#define CAN_MAX_DLEN 8
struct CanFrame{
    uint32_t id;
    uint8_t len;
    uint8_t data[CAN_MAX_DLEN];
};
class ringbuffer{
public:
    ringbuffer():m_head(0),m_tail(0){
        memset(m_buffer,0,sizeof(m_buffer));
    }

    //生产者
    bool push(CanFrame& frame){
        int next=(m_head+1)%RING_BUF_SIZE;
        if(next==m_tail){
            return false;
        }
        m_buffer[m_head]=frame;
        m_head=next;
        return true;
    }

    //消费者
    bool pop(CanFrame& frame){
        if(m_tail==m_head){
            return false;
        }
        frame=m_buffer[m_tail];
        m_tail=(m_tail+1)%RING_BUF_SIZE;
        return true;
    }

    bool isEmpty()const{
        return m_head==m_tail;
    }

    int count()const{
        return (m_head - m_tail + RING_BUF_SIZE) % RING_BUF_SIZE;
    }

public:
    CanFrame m_buffer[RING_BUF_SIZE];
    std::atomic<int> m_head;
    std::atomic<int> m_tail;
};


#endif // RING_BUFFER_H
