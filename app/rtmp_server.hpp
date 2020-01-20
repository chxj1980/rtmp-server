#ifndef RS_RTMP_SERVER_HPP
#define RS_RTMP_SERVER_HPP

#include <common/core.hpp>
#include <protocol/rtmp_stack.hpp>

class RTMPServer
{
public:
    RTMPServer(IProtocolReaderWriter *rw);
    virtual ~RTMPServer();

public:
    virtual int32_t Handshake();
    virtual void SetSendTimeout(int64_t timeout_us);
    virtual void SetRecvTimeout(int64_t timeout_us);
    virtual int ConnectApp(rtmp::Request *req);
    virtual int SetWindowAckSize(int ackowledgement_window_size);
    virtual int SetPeerBandwidth(int bandwidth, int type);
    virtual int SetChunkSize(int chunk_size);
    virtual int ResponseConnectApp(rtmp::Request *req, const std::string &local_ip);
    virtual int IdentifyClient(int stream_id, rtmp::ConnType &type, std::string &stream_name, double &duration);
    virtual int StartFmlePublish(int stream_id);

protected:
    virtual int IdentifyFmlePublishClient(rtmp::FMLEStartPacket *pkt, rtmp::ConnType &type, std::string &stream_id);

private:
    IProtocolReaderWriter *rw_;
    rtmp::HandshakeBytes *handshake_bytes_;
    rtmp::Protocol *protocol_;
};

#endif