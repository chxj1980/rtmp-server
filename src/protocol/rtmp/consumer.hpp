/*
 * @Date: 2020-03-10 13:49:36
 * @LastEditors: linmin
 * @LastEditTime: 2020-03-18 15:06:51
 */
#ifndef RS_RTMP_CONSUMER_HPP
#define RS_RTMP_CONSUMER_HPP

#include <common/connection.hpp>
#include <common/core.hpp>

namespace rtmp {

enum class JitterAlgorithm;
class SharedPtrMessage;
class MessageArray;
class MessageQueue;
class Source;
class Jitter;

class IWakeable {
  public:
    IWakeable();
    virtual ~IWakeable();

  public:
    virtual void WakeUp() = 0;
};

class Consumer : public IWakeable {
  public:
    Consumer(Source* s, IConnection* c);
    virtual ~Consumer();

  public:
    virtual void SetQueueSize(double queue_size);
    virtual int  GetTime();
    virtual int
                 Enqueue(SharedPtrMessage* shared_msg, bool atc, JitterAlgorithm ag);
    virtual int  DumpPackets(MessageArray* msg_arr, int& count);
    virtual void Wait(int nb_msgs, int duration);
    virtual int  OnPlayClientPause(bool is_pause);
    virtual void UpdateSourceID();
    // IWakeable
    virtual void WakeUp() override;

  private:
    Source*       source_;
    IConnection*  conn_;
    bool          pause_;
    Jitter*       jitter_;
    MessageQueue* queue_;
    bool          should_update_source_id_;
    st_cond_t     mw_wait_;
    bool          mw_waiting_;
    int           mw_min_msgs_;
    int           mw_duration_;
};

}  // namespace rtmp

#endif