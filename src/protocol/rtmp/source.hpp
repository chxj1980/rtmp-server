/*
 * @Author: linmin
 * @Date: 2020-03-10 16:10:07
 * @LastEditTime: 2020-03-18 15:00:29
 * @LastEditors: linmin
 * @Description: In User Settings Edit
 * @FilePath: \rtmp_server\protocol\rtmp\source.hpp
 */
#ifndef RS_RTMP_SOURCE_HPP
#define RS_RTMP_SOURCE_HPP

#include <common/connection.hpp>
#include <common/core.hpp>
#include <common/queue.hpp>

#include <vector>

namespace rtmp {

enum class JitterAlgorithm;

class GopCache;
class Dvr;
class Request;
class Reponse;
class CommonMessage;
class OnMetadataPacket;
class Consumer;
class Source;
class Connection;
class Jitter;
class SharedPtrMessage;

class ISourceHandler {
  public:
    ISourceHandler();
    virtual ~ISourceHandler();

  public:
    virtual int OnPublish(Source* s, Request* r)   = 0;
    virtual int OnUnPublish(Source* s, Request* r) = 0;
};

class Source {
  public:
    Source();
    virtual ~Source();

  public:
    static int   FetchOrCreate(Request* r, ISourceHandler* h, Source** pps);
    static int   CycleAll();
    virtual int  Initialize(Request* r, ISourceHandler* h);
    virtual bool CanPublish(bool is_edge);
    virtual void OnConsumerDestroy(Consumer* consumer);
    virtual int  OnAudio(CommonMessage* msg);
    virtual int  OnVideo(CommonMessage* msg);
    virtual int  OnMetadata(CommonMessage* msg, OnMetadataPacket* pkt);
    virtual int  OnDvrRequestSH();
    virtual int  OnPublish();
    virtual void OnUnpublish();
    virtual int  SourceId();
    virtual int  Cycle();
    virtual bool Expired();
    virtual int  GetPrevSourceID();
    virtual int  GetSouceID();
    virtual void OnSourceIDChange(int id);
    virtual int  CreateConsumer(Connection* conn,
                                Consumer*&  consumer,
                                bool        ds = true,
                                bool        dm = true,
                                bool        dg = true);

  protected:
    static Source* fetch(Request* r);

  private:
    int        on_audio_impl(SharedPtrMessage* msg);
    int        on_video_impl(SharedPtrMessage* msg);
    static int do_cycle_all();

  private:
    static std::map<std::string, Source*> pool_;
    Request*                              request_;
    bool                                  atc_;
    ISourceHandler*                       handler_;
    bool                                  can_publish_;
    bool                                  mix_correct_;
    bool                                  is_monotonically_increase_;
    int64_t                               last_packet_time_;
    SharedPtrMessage*                     cache_metadata_;
    SharedPtrMessage*                     cache_sh_video_;
    SharedPtrMessage*                     cache_sh_audio_;
    std::vector<Consumer*>                consumers_;
    JitterAlgorithm                       ag_;
    MixQueue<SharedPtrMessage>*           mix_queue_;
    Dvr*                                  dvr_;
    GopCache*                             gop_cache_;
    int64_t                               die_at_;
    int                                   source_id_;
    int                                   prev_source_id_;
};
}  // namespace rtmp
#endif
