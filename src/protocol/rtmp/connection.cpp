/*
 * @Author: linmin
 * @Date: 2020-02-06 17:27:12
 * @LastEditTime: 2020-04-07 13:05:37
 */

#include <app/server.hpp>
#include <common/config.hpp>
#include <common/error.hpp>
#include <common/log.hpp>
#include <common/utils.hpp>
#include <protocol/rtmp/connection.hpp>
#include <protocol/rtmp/consumer.hpp>
#include <protocol/rtmp/defines.hpp>
#include <protocol/rtmp/message.hpp>
#include <protocol/rtmp/recv_thread.hpp>
#include <protocol/rtmp/server.hpp>
#include <protocol/rtmp/source.hpp>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

namespace rtmp {

Connection::Connection(StreamServer* server, st_netfd_t stfd)
    : IConnection(server, stfd)
{
    server_      = server;
    socket_      = new StSocket(stfd);
    rtmp_        = new Server(socket_);
    request_     = new Request;
    response_    = new Response;
    type_        = ConnType::UNKNOW;
    tcp_nodelay_ = false;
    mw_sleep_    = RTMP_MR_SLEEP_MS;
    wakeable_    = nullptr;
}

Connection::~Connection()
{
    rs_freep(response_);
    rs_freep(request_);
    rs_freep(rtmp_);
    rs_freep(socket_);
}

void Connection::Dispose()
{
    ::IConnection::Dispose();
    if (wakeable_) {
        wakeable_->WakeUp();
    }
}

int32_t Connection::do_cycle()
{
    int ret = ERROR_SUCCESS;

    rtmp_->SetRecvTimeout(RTMP_RECV_TIMEOUT_US);
    rtmp_->SetSendTimeout(RTMP_SEND_TIMEOUT_US);

    if ((ret = rtmp_->Handshake()) != ERROR_SUCCESS) {
        rs_error("rtmp handshake failed. ret=%d", ret);
        return ret;
    }

    if ((ret = rtmp_->ConnectApp(request_)) != ERROR_SUCCESS) {
        rs_error("rtmp connect app failed. ret=%d", ret);
        return ret;
    }

    request_->ip = client_ip_;

    ServiceCycle();

    return ret;
}

int32_t Connection::StreamServiceCycle()
{
    int ret = ERROR_SUCCESS;

    ConnType type;

    if ((ret = rtmp_->IdentifyClient(response_->stream_id, type,
                                     request_->stream, request_->duration)) !=
        ERROR_SUCCESS) {
        rs_error("identify client failed. ret=%d", ret);
        return ret;
    }

    DiscoveryTcUrl(request_->tc_url, request_->schema, request_->host,
                   request_->vhost, request_->app, request_->stream,
                   request_->port, request_->param);
    request_->Strip();

    if (request_->schema.empty() || request_->vhost.empty() ||
        request_->port.empty() || request_->app.empty()) {
        ret = ERROR_RTMP_REQ_TCURL;
        rs_error("discovery tcUrl failed. ret=%d", ret);
        return ret;
    }

    if (request_->stream.empty()) {
        ret = ERROR_RTMP_STREAM_NAME_EMPTY;
        rs_error("empty stream name is not allowed, ret=%d", ret);
        return ret;
    }

    Source* source = nullptr;
    if ((ret = Source::FetchOrCreate(request_, server_, &source)) !=
        ERROR_SUCCESS) {
        return ret;
    }

    type_ = type;
    switch (type) {
        case ConnType::FMLE_PUBLISH:
            if ((ret = rtmp_->StartFmlePublish(response_->stream_id)) !=
                ERROR_SUCCESS) {
                rs_error("start to publish stream failed. ret=%d", ret);
                return ret;
            }
            return Publishing(source);
        case ConnType::PLAY:
            if ((ret = rtmp_->StartPlay(response_->stream_id)) !=
                ERROR_SUCCESS) {
                rs_error("start to play stream failed. ret=%d", ret);
                return ret;
            }
            return Playing(source);
        case ConnType::HIVISION_PUBLISH:
            if ((ret = rtmp_->StartHivisionPublish(response_->stream_id)) !=
                ERROR_SUCCESS) {
                rs_error("start to hivision publish stream failed. ret=%d",
                         ret);
            }
            return Publishing(source);
        default: break;
    }

    return ret;
}

void Connection::release_publish(Source* source, bool is_edge)
{
    if (is_edge) {
        // TODO
    }
    else {
        source->OnUnpublish();
    }
}

int32_t Connection::Publishing(Source* source)
{
    int ret = ERROR_SUCCESS;

    bool vhost_is_edge = _config->GetVhostIsEdge(request_->vhost);
    if ((ret = acquire_publish(source, false)) == ERROR_SUCCESS) {
        PublishRecvThread recv_thread(
            rtmp_, request_, st_netfd_fileno(client_stfd_), 0, this, source,
            type_ != ConnType::FLASH_PUBLISH, vhost_is_edge);

        ret = do_publishing(source, &recv_thread);

        recv_thread.Stop();
    }

    if (ret != ERROR_SYSTEM_STREAM_BUSY) {
        release_publish(source, vhost_is_edge);
    }

    return ret;
}

int32_t Connection::do_playing(Source*          source,
                               Consumer*        consumer,
                               QueueRecvThread* recv_thread)
{
    int          ret = ERROR_SUCCESS;
    MessageArray msgs(RTMP_MR_MSGS);

    while (!disposed_) {
        if (expired_) {
            ret = ERROR_USER_DISCONNECT;
            rs_error("connection expired. ret=%d", ret);
            return ret;
        }

        while (!recv_thread->Empty()) {
            CommonMessage* msg = recv_thread->Pump();
            // 先不处理,直接释放
            // TODO 实现客户端控制协议
            rs_freep(msg);
        }

        if ((ret = recv_thread->ErrorCode()) != ERROR_SUCCESS) {
            if (!is_client_gracefully_close(ret) &&
                !is_system_control_error(ret)) {
                rs_error("recv thread failed. ret=%d", ret);
            }
            return ret;
        }

        consumer->Wait(RTMP_MR_MIN_MSGS, mw_sleep_);

        int count = 0;
        if ((ret = consumer->DumpPackets(&msgs, count)) != ERROR_SUCCESS) {
            rs_error("get message form consumer failed. ret=%d", ret);
            return ret;
        }

        if (count <= 0) {
            rs_info("mw sleep %dms for no msg", mw_sleep_);
            st_usleep(mw_sleep_ * 1000);
            continue;
        }

        if ((ret = rtmp_->SendAndFreeMessages(
                 msgs.msgs, count, response_->stream_id)) != ERROR_SUCCESS) {
            if (!is_client_gracefully_close(ret)) {
                rs_error("send messages to client failed. ret=%d", ret);
            }
            return ret;
        }
    }

    return ret;
}

int32_t Connection::Playing(Source* source)
{
    int ret = ERROR_SUCCESS;

    Consumer* consumer = nullptr;
    if ((ret = source->CreateConsumer(this, consumer)) != ERROR_SUCCESS) {
        rs_error("create consumer failed. ret=%d", ret);
        return ret;
    }

    rs_auto_free(Consumer, consumer);

    QueueRecvThread recv_thread(consumer, rtmp_, mw_sleep_);
    if ((ret = recv_thread.Start()) != ERROR_SUCCESS) {
        rs_error("start isolate recv thread failed. ret=%d", ret);
        return ret;
    }

    wakeable_ = consumer;
    ret       = do_playing(source, consumer, &recv_thread);
    wakeable_ = nullptr;

    recv_thread.Stop();

    if (!recv_thread.Empty()) {
        rs_warn("drop received %d messages", recv_thread.Size());
    }
    return ret;
}

int32_t Connection::ServiceCycle()
{
    int ret = ERROR_SUCCESS;

    if ((ret = rtmp_->SetWindowAckSize((int)RTMP_DEFAULT_WINDOW_ACK_SIZE)) !=
        ERROR_SUCCESS) {
        rs_error("set window ackowledgement size failed. ret=%d", ret);
        return ret;
    }

    if ((ret = rtmp_->SetPeerBandwidth((int)RTMP_DEFAULT_PEER_BAND_WIDTH,
                                       (int)PeerBandwidthType::DYNAMIC)) !=
        ERROR_SUCCESS) {
        rs_error("set peer bandwidth failed. ret=%d", ret);
        return ret;
    }

    std::string local_ip = Utils::GetLocalIP(st_netfd_fileno(client_stfd_));

    int chunk_size = _config->GetChunkSize(request_->vhost);
    if ((ret = rtmp_->SetChunkSize(chunk_size)) != ERROR_SUCCESS) {
        rs_error("set chunk size failed. ret=%d", ret);
        return ret;
    }

    if ((ret = rtmp_->ResponseConnectApp(request_, local_ip)) !=
        ERROR_SUCCESS) {
        rs_error("response connect app failed. ret=%d", ret);
        return ret;
    }

    // if ((ret = rtmp_->OnBWDone()) != ERROR_SUCCESS) {
    //     rs_error("on bandwidth done failed. ret=%d",ret);
    //     return ret;
    // }

    while (!disposed_) {
        ret = StreamServiceCycle();
        if (ret == ERROR_SUCCESS) {
            continue;
        }

        return ret;
    }
    return ret;
}

void    Connection::Resample() {}
int64_t Connection::GetSendBytesDelta()
{
    return 0;
}
int64_t Connection::GetRecvBytesDelta()
{
    return 0;
}
void Connection::CleanUp() {}

int Connection::process_publish_message(Source*        source,
                                        CommonMessage* msg,
                                        bool           is_edge)
{
    int ret = ERROR_SUCCESS;
    if (is_edge) {
        // TODO implement edge opt
    }

    if (msg->header.IsAudio()) {
        if ((ret = source->OnAudio(msg)) != ERROR_SUCCESS) {
            rs_error("source process audio message failed. ret=%d", ret);
            return ret;
        }
    }

    if (msg->header.IsVideo()) {
        if ((ret = source->OnVideo(msg)) != ERROR_SUCCESS) {
            rs_error("source process video message failed. ret=%d", ret);
            return ret;
        }
    }

    if (msg->header.IsAMF0Data() || msg->header.IsAMF3Data()) {
        Packet* packet = nullptr;
        if ((ret = rtmp_->DecodeMessage(msg, &packet)) != ERROR_SUCCESS) {
            rs_error("decode on_metadata message failed. ret=%d", ret);
            return ret;
        }
        rs_auto_free(Packet, packet);

        if (dynamic_cast<OnMetadataPacket*>(packet)) {
            OnMetadataPacket* pkt = dynamic_cast<OnMetadataPacket*>(packet);
            if ((ret = source->OnMetadata(msg, pkt)) != ERROR_SUCCESS) {
                rs_error("source process on_metadata message failed. ret=%d",
                         ret);
                return ret;
            }

            return ret;
        }
    }

    return ret;
}

int Connection::handle_publish_message(Source*        source,
                                       CommonMessage* msg,
                                       bool           is_fmle,
                                       bool           is_edge)
{
    int ret = ERROR_SUCCESS;

    if (msg->header.IsAMF0Command() || msg->header.IsAMF3Command()) {
        Packet* packet = nullptr;
        if ((ret = rtmp_->DecodeMessage(msg, &packet)) != ERROR_SUCCESS) {
            rs_error("FMLE decode unpublish message failed. ret=%d", ret);
            return ret;
        }

        rs_auto_free(Packet, packet);

        if (!is_fmle) {
            rs_trace("refresh flash publish finished");
            return ERROR_CONTROL_REPUBLISH;
        }

        if (dynamic_cast<FMLEStartPacket*>(packet)) {
            FMLEStartPacket* pkt = dynamic_cast<FMLEStartPacket*>(packet);
            if ((ret = rtmp_->FMLEUnPublish(response_->stream_id,
                                            pkt->transaction_id)) !=
                ERROR_SUCCESS) {
                return ret;
            }

            return ERROR_CONTROL_REPUBLISH;
        }
        return ret;
    }

    if ((ret = process_publish_message(source, msg, is_edge)) !=
        ERROR_SUCCESS) {
        rs_error("FMLE process publish message failed. ret=%d", ret);
        return ret;
    }

    return ret;
}

/**
 * @name: set_socket_option
 * @msg: 配置tcp NO_DELAY选项
 */
void Connection::set_socket_option()
{
    bool nvalue = _config->GetTCPNoDelay(request_->vhost);
    if (nvalue != tcp_nodelay_) {
        tcp_nodelay_ = nvalue;

        int       fd   = st_netfd_fileno(client_stfd_);
        socklen_t nb_v = sizeof(int);
        int       ov   = 0;

        getsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &ov, &nb_v);

        int nv = tcp_nodelay_;
        if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nv, nb_v) < 0) {
            rs_error("set socket TCP_NODELAY=%d failed", nv);
            return;
        }

        getsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nv, &nb_v);
        rs_trace("set socket TCP_NODELAY=%d success. %d => %d", tcp_nodelay_,
                 ov, nv);
    }
}

int Connection::acquire_publish(Source* source, bool is_edge)
{
    int ret = ERROR_SUCCESS;

    if (!source->CanPublish(is_edge)) {
        ret = ERROR_SYSTEM_STREAM_BUSY;
        rs_warn("stream %s is already publishing. ret=%d",
                request_->GetStreamUrl().c_str(), ret);
        return ret;
    }

    if (is_edge) {}
    else {
        if ((ret = source->OnPublish()) != ERROR_SUCCESS) {
            rs_error("notify publish failed. ret=%d", ret);
            return ret;
        }
    }

    return ret;
}

int Connection::do_publishing(Source* source, PublishRecvThread* recv_thread)
{
    int ret = ERROR_SUCCESS;

    if ((ret = recv_thread->Start()) != ERROR_SUCCESS) {
        rs_error("start isolate recv thread failed. ret=%d", ret);
        return ret;
    }

    int recv_thread_cid = recv_thread->GetCID();
    // marge isolate recv thread log
    recv_thread->SetCID(_context->GetID());

    publish_first_pkt_timeout_ =
        _config->GetPublishFirstPktTimeout(request_->vhost);
    publish_normal_pkt_timeout_ =
        _config->GetPublishNormalPktTimeout(request_->vhost);

    set_socket_option();

    bool mr       = _config->GetMREnabled(request_->vhost);
    int  mr_sleep = _config->GetMRSleepMS(request_->vhost);

    rs_trace("start publish mr=%d/%d, first_pkt_timeout=%d, "
             "normal_pkt_timeout=%d, rtcid=%d",
             mr, mr_sleep, publish_first_pkt_timeout_,
             publish_normal_pkt_timeout_, recv_thread_cid);

    int64_t nb_msgs = 0;

    while (!disposed_) {
        if (expired_) {
            ret = ERROR_USER_DISCONNECT;
            rs_error("connection expired. ret=%d", ret);
            return ret;
        }
        if (nb_msgs == 0) {
            recv_thread->Wait(publish_first_pkt_timeout_);
        }
        else {
            recv_thread->Wait(publish_normal_pkt_timeout_);
        }

        if ((ret = recv_thread->ErrorCode()) != ERROR_SUCCESS) {
            if (!is_system_control_error(ret) &&
                !is_client_gracefully_close(ret)) {
                rs_error("recv thread failed. ret=%d", ret);
            }
            return ret;
        }

        if (recv_thread->GetMsgNum() <= nb_msgs) {
            ret = ERROR_SOCKET_TIMEOUT;
            rs_warn("publish timeout %dms, nb_msgs=%lld, ret=%d",
                    nb_msgs ? publish_normal_pkt_timeout_ :
                              publish_first_pkt_timeout_,
                    nb_msgs, ret);
            break;
        }

        nb_msgs = recv_thread->GetMsgNum();
    }

    return ret;
}
}  // namespace rtmp