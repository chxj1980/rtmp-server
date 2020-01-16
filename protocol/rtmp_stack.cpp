#include <protocol/rtmp_stack.hpp>
#include <protocol/rtmp_consts.hpp>

namespace rtmp
{

static void vhost_resolve(std::string &vhost, std::string &app, std::string &param)
{
    size_t pos = std::string::npos;
    if ((pos = app.find("?")) != std::string::npos)
    {
        param = app.substr(pos);
        rs_info("param:%s", param.c_str());
    }

    app = Utils::StringReplace(app, ",", "?");
    app = Utils::StringReplace(app, "...", "?");
    app = Utils::StringReplace(app, "&&", "?");
    app = Utils::StringReplace(app, "=", "?");
    if (Utils::StringEndsWith(app, "/_definst_"))
    {
        app = Utils::StringEraseLastSubstr(app, "/_definst_");
    }

    if ((pos = app.find("?")) != std::string::npos)
    {
        std::string query = app.substr(pos + 1);
        app = app.substr(0, pos);

        if ((pos = query.find("vhost?")) != std::string::npos)
        {
            query = query.substr(pos + 6);
            if (!query.empty())
            {
                vhost = query;
                if ((pos = vhost.find("?")) != std::string::npos)
                {
                    vhost = vhost.substr(0, pos);
                }
            }
        }
    }
}

static int chunk_header_c0(int perfer_cid,
                           uint32_t timestamp,
                           int32_t payload_length,
                           int8_t message_type,
                           int32_t stream_id,
                           char *buf)
{
    char *pp = nullptr;
    char *p = buf;

    *p++ = 0x00 | (0x3f & perfer_cid);
    if (timestamp < RTMP_EXTENDED_TIMESTAMP)
    {
        pp = (char *)&timestamp;
        *p++ = pp[2];
        *p++ = pp[1];
        *p++ = pp[0];
    }
    else
    {
        *p++ = 0xff;
        *p++ = 0xff;
        *p++ = 0xff;
    }

    pp = (char *)&payload_length;
    *p++ = pp[2];
    *p++ = pp[1];
    *p++ = pp[0];

    *p++ = message_type;

    pp = (char *)&stream_id;
    *p++ = pp[3];
    *p++ = pp[2];
    *p++ = pp[1];
    *p++ = pp[0];

    if (timestamp >= RTMP_EXTENDED_TIMESTAMP)
    {
        pp = (char *)&timestamp;
        *p++ = pp[3];
        *p++ = pp[2];
        *p++ = pp[1];
        *p++ = pp[0];
    }

    return p - buf;
}

static int chunk_header_c3(int perfer_cid, uint32_t timestamp, char *buf)
{
    char *pp = nullptr;
    char *p = buf;

    *p++ = 0xC0 | (0x3f & perfer_cid);

    if (timestamp >= RTMP_EXTENDED_TIMESTAMP)
    {
        pp = (char *)&timestamp;
        *p++ = pp[3];
        *p++ = pp[2];
        *p++ = pp[1];
        *p++ = pp[0];
    }

    return p - buf;
}

void DiscoveryTcUrl(const std::string &tc_url,
                    std::string &schema,
                    std::string &host,
                    std::string &vhost,
                    std::string &app,
                    std::string &stream,
                    std::string &port,
                    std::string &param)
{
    size_t pos = std::string::npos;
    std::string url = tc_url;

    if ((pos = url.find("://")) != std::string::npos)
    {
        schema = url.substr(0, pos);
        url = url.substr(pos + 3);
    }

    if ((pos = url.find("/")) != std::string::npos)
    {
        host = url.substr(0, pos);
        url = url.substr(pos + 1);

        if ((pos = host.find(":")) != std::string::npos)
        {
            port = host.substr(pos + 1);
            host = host.substr(0, pos);
        }
        else
        {
            port = RTMP_DEFAULT_PORT;
        }
    }

    app = url;
    vhost = host;

    vhost_resolve(vhost, app, param);
    vhost_resolve(vhost, stream, param);

    if (param == RTMP_DEFAULT_VHOST_PARAM)
    {
        param = "";
    }
}

IMessageHandler::IMessageHandler()
{
}

IMessageHandler::~IMessageHandler()
{
}

MessageHeader::MessageHeader() : timestamp_delta(0),
                                 payload_length(0),
                                 message_type(0),
                                 stream_id(0),
                                 timestamp(0),
                                 perfer_cid(0)
{
}

MessageHeader::~MessageHeader()
{
}

bool MessageHeader::IsAudio()
{
    if (message_type == RTMP_MSG_AUDIO_MESSAGE)
        return true;
    return false;
}
bool MessageHeader::IsVideo()
{

    if (message_type == RTMP_MSG_VIDEO_MESSAGE)
        return true;
    return false;
}
bool MessageHeader::IsAMF0Command()
{
    if (message_type == RTMP_MSG_AMF0_COMMAND)
        return true;
    return false;
}
bool MessageHeader::IsAMF0Data()
{
    if (message_type == RTMP_MSG_AMF0_DATA)
        return true;
    return false;
}
bool MessageHeader::IsAMF3Command()
{
    if (message_type == RTMP_MSG_AMF3_COMMAND)
        return true;
    return false;
}
bool MessageHeader::IsAMF3Data()
{
    if (message_type == RTMP_MSG_AMF3_DATA)
        return true;
    return false;
}
bool MessageHeader::IsWindowAckledgementSize()
{
    if (message_type == RTMP_MSG_WINDOW_ACK_SIZE)
        return true;
    return false;
}

bool MessageHeader::IsAckledgement()
{
    if (message_type == RTMP_MSG_ACK)
        return true;
    return false;
}
bool MessageHeader::IsSetChunkSize()
{
    if (message_type == RTMP_MSG_SET_CHUNK_SIZE)
        return true;
    return false;
}
bool MessageHeader::IsUserControlMessage()
{
    if (message_type == RTMP_MSG_USER_CONTROL_MESSAGE)
        return true;
    return false;
}
bool MessageHeader::IsSetPeerBandWidth()
{
    if (message_type == RTMP_MSG_SET_PEER_BANDWIDTH)
        return true;
    return false;
}
bool MessageHeader::IsAggregate()
{
    if (message_type == RTMP_MSG_AGGREGATE)
        return true;
    return false;
}
void MessageHeader::InitializeAMF0Script(int32_t size, int32_t stream)
{
    message_type = RTMP_MSG_AMF0_DATA;
    payload_length = size;
    timestamp_delta = 0;
    timestamp = 0;
    stream_id = stream;
    perfer_cid = RTMP_CID_OVER_CONNECTION2;
}
void MessageHeader::InitializeVideo(int32_t size, uint32_t time, int32_t stream)
{
    message_type = RTMP_MSG_VIDEO_MESSAGE;
    payload_length = size;
    timestamp_delta = time;
    timestamp = time;
    stream_id = stream;
    perfer_cid = RTMP_CID_VIDEO;
}
void MessageHeader::InitializeAudio(int32_t size, uint32_t time, int32_t stream)
{
    message_type = RTMP_MSG_AUDIO_MESSAGE;
    payload_length = size;
    timestamp_delta = time;
    timestamp = time;
    stream_id = stream;
    perfer_cid = RTMP_CID_AUDIO;
}

CommonMessage::CommonMessage() : size(0),
                                 payload(nullptr)
{
}

CommonMessage::~CommonMessage()
{
    rs_freep(payload);
}

void CommonMessage::CreatePayload(int32_t size)
{
    rs_freep(payload);
    payload = new char[size];
    rs_verbose("create payload for rtmp message,size=%d", size);
}

ChunkStream::ChunkStream(int cid) : cid(cid),
                                    fmt(0),
                                    msg(nullptr),
                                    extended_timestamp(false),
                                    msg_count(0)

{
}

ChunkStream::~ChunkStream()
{
}

HandshakeBytes::HandshakeBytes() : c0c1(nullptr),
                                   s0s1s2(nullptr),
                                   c2(nullptr)
{
}

HandshakeBytes::~HandshakeBytes()
{
    rs_freepa(c0c1);
    rs_freepa(s0s1s2);
    rs_freepa(c2);
}

int32_t HandshakeBytes::ReadC0C1(IProtocolReaderWriter *rw)
{
    int32_t ret = ERROR_SUCCESS;

    if (c0c1)
    {
        return ret;
    }

    ssize_t nread;
    c0c1 = new char[1537];

    if ((ret = rw->ReadFully(c0c1, 1537, &nread)) != ERROR_SUCCESS)
    {
        rs_error("read c0c1 failed,ret=%d", ret);
        return ret;
    }

    rs_verbose("read c0c1 success");
    return ret;
}

int32_t HandshakeBytes::ReadS0S1S2(IProtocolReaderWriter *rw)
{
    int32_t ret = ERROR_SUCCESS;

    if (s0s1s2)
    {
        return ret;
    }

    ssize_t nread;
    s0s1s2 = new char[3073];
    if ((ret = rw->ReadFully(s0s1s2, 3073, &nread)) != ERROR_SUCCESS)
    {
        rs_error("read s0s1s2 failed,ret=%d", ret);
        return ret;
    }

    rs_verbose("read s0s1s2 success");
    return ret;
}

int32_t HandshakeBytes::ReadC2(IProtocolReaderWriter *rw)
{
    int32_t ret = ERROR_SUCCESS;

    if (c2)
    {
        return ret;
    }

    ssize_t nread;
    c2 = new char[1536];
    if ((ret = rw->ReadFully(c2, 1536, &nread)) != ERROR_SUCCESS)
    {
        rs_error("read c2 failed,ret=%d", ret);
        return ret;
    }

    rs_verbose("read c2 success");
    return ret;
}

int32_t HandshakeBytes::CreateC0C1()
{
    int32_t ret = ERROR_SUCCESS;

    if (c0c1)
    {
        return ret;
    }

    c0c1 = new char[1537];
    Utils::RandomGenerate(c0c1, 1537);

    BufferManager manager;
    if ((ret = manager.Initialize(c0c1, 9)) != ERROR_SUCCESS)
    {
        return ret;
    }

    //c0
    manager.Write1Bytes(0x03);
    //c1
    manager.Write4Bytes((int32_t)::time(nullptr));
    manager.Write4Bytes(0x00);

    return ret;
}

int32_t HandshakeBytes::CreateS0S1S2(const char *c1)
{
    int32_t ret = ERROR_SUCCESS;

    if (s0s1s2)
    {
        return ret;
    }

    s0s1s2 = new char[3073];
    Utils::RandomGenerate(s0s1s2, 3073);

    BufferManager manager;
    if ((ret = manager.Initialize(s0s1s2, 9)) != ERROR_SUCCESS)
    {
        return ret;
    }
    //s0
    manager.Write1Bytes(0x03);
    //s1
    manager.Write4Bytes((int32_t)::time(NULL));
    // s1 time2 copy from c1
    if (c0c1)
    {
        manager.WriteBytes(c0c1 + 1, 4);
    }

    //s2
    // if c1 specified, copy c1 to s2.
    if (c1)
    {
        memcpy(s0s1s2 + 1537, c1, 1536);
    }

    return ret;
}

int32_t HandshakeBytes::CreateC2()
{
    int32_t ret = ERROR_SUCCESS;

    if (c2)
    {
        return ret;
    }

    c2 = new char[1536];
    Utils::RandomGenerate(c2, 1536);

    BufferManager manager;
    if ((ret = manager.Initialize(c2, 8)) != ERROR_SUCCESS)
    {
        return ret;
    }

    manager.Write4Bytes((int32_t)::time(nullptr));
    if (s0s1s2)
    {
        manager.WriteBytes(s0s1s2 + 1, 4);
    }

    return ret;
}

SimpleHandshake::SimpleHandshake()
{
}

SimpleHandshake::~SimpleHandshake()
{
}

int32_t SimpleHandshake::HandshakeWithClient(HandshakeBytes *handshake_bytes, IProtocolReaderWriter *rw)
{
    int32_t ret = ERROR_SUCCESS;

    ssize_t nwrite;

    if ((ret = handshake_bytes->ReadC0C1(rw)) != ERROR_SUCCESS)
    {
        return ret;
    }

    if (handshake_bytes->c0c1[0] != 0x03)
    {
        ret = ERROR_RTMP_PLAIN_REQUIRED;
        rs_error("check c0 failed,only support rtmp plain text,ret=%d", ret);
        return ret;
    }

    rs_verbose("check c0 success");

    if ((ret = handshake_bytes->CreateS0S1S2(handshake_bytes->c0c1 + 1)) != ERROR_SUCCESS)
    {
        return ret;
    }

    if ((ret = rw->Write(handshake_bytes->s0s1s2, 3073, &nwrite)) != ERROR_SUCCESS)
    {
        rs_error("simple handshake send s0s1s2 failed,ret=%d", ret);
        return ret;
    }

    rs_verbose("simple handshake send s0s1s2 success");

    if ((ret = handshake_bytes->ReadC2(rw)) != ERROR_SUCCESS)
    {
        return ret;
    }

    rs_verbose("simple handshake success");
    return ret;
}

Request::Request()
{
}

Request::~Request()
{
}

void Request::Strip()
{
    host = Utils::StringRemove(host, "/ \n\r\t");
    vhost = Utils::StringRemove(vhost, "/ \n\r\t");
    app = Utils::StringRemove(app, " \n\r\t");
    stream = Utils::StringRemove(stream, " \n\r\t");

    app = Utils::StringTrimEnd(app, "/");
    stream = Utils::StringTrimEnd(stream, "/");

    app = Utils::StringTrimStart(app, "/");
    stream = Utils::StringTrimStart(stream, "/");
}

Packet::Packet()
{
}

Packet::~Packet()
{
}

int Packet::GetPreferCID()
{
    return 0;
}

int Packet::GetMessageType()
{
    return 0;
}

int Packet::GetSize()
{
    return 0;
}

int Packet::Encode(int &psize, char *&ppayload)
{
    int ret = ERROR_SUCCESS;

    int size = GetSize();
    char *payload = nullptr;

    BufferManager manager;
    if (size > 0)
    {
        payload = new char[size];

        if ((ret = manager.Initialize(payload, size)) != ERROR_SUCCESS)
        {
            rs_error("initialize buffer manager failed,ret=%d", ret);
            rs_freepa(payload);
            return ret;
        }
    }

    if ((ret = EncodePacket(&manager)) != ERROR_SUCCESS)
    {
        rs_error("encode the packet failed,ret=%d", ret);
        rs_freep(payload);
        return ret;
    }

    psize = size;
    ppayload = payload;
    rs_verbose("encode the packet success,size=%d", size);
    return ret;
}

int Packet::Decode(BufferManager *manager)
{
    return ERROR_SUCCESS;
}

int Packet::EncodePacket(BufferManager *manager)
{
    return ERROR_SUCCESS;
}

SetChunkSizePacket::SetChunkSizePacket() : chunk_size(RTMP_PROTOCOL_CHUNK_SIZE)
{
}

SetChunkSizePacket::~SetChunkSizePacket()
{
}

int SetChunkSizePacket::Decode(BufferManager *manager)
{
    int ret = ERROR_SUCCESS;

    if (!manager->Require(4))
    {
        ret = ERROR_RTMP_MESSAGE_DECODE;
        rs_error("decode chunk size failed,ret=%d", ret);
        return ret;
    }

    chunk_size = manager->Read4Bytes();
    rs_verbose("decode chunk size success,chunk_size=%d", chunk_size);
    return ret;
}

int SetChunkSizePacket::GetPreferCID()
{
    return RTMP_CID_PROTOCOL_CONTROL;
}

int SetChunkSizePacket::GetMessageType()
{
    return RTMP_MSG_SET_CHUNK_SIZE;
}

int SetChunkSizePacket::GetSize()
{
    return 4;
}

int SetChunkSizePacket::EncodePacket(BufferManager *manager)
{
    int ret = ERROR_SUCCESS;
    if (!manager->Require(4))
    {
        ret = ERROR_RTMP_MESSAGE_ENCODE;
        rs_error("encode setchunk packet failed,ret=%d", ret);
        return ret;
    }
    manager->Write4Bytes(chunk_size);
    rs_verbose("encode setchunk packet success,chunk_size=%d", chunk_size);
    return ret;
}

ConnectAppPacket::ConnectAppPacket() : command_name(RTMP_AMF0_COMMAND_CONNECT),
                                       transaction_id(1),
                                       args(nullptr)
{
    command_object = AMF0Any::Object();
}

ConnectAppPacket::~ConnectAppPacket()
{
    rs_freep(command_object);
    rs_freep(args);
}

int ConnectAppPacket::Decode(BufferManager *manager)
{
    int ret = ERROR_SUCCESS;

    if ((ret = AMF0ReadString(manager, command_name)) != ERROR_SUCCESS)
    {
        rs_error("amf0 decode connect command_name failed,ret=%d", ret);
        return ret;
    }

    if (command_name.empty() || command_name != RTMP_AMF0_COMMAND_CONNECT)
    {
        ret = ERROR_RTMP_AMF0_DECODE;
        rs_error("amf0 decode connect command_name failed,command_name=%s,ret=%d", command_name, ret);
        return ret;
    }

    if ((ret = AMF0ReadNumber(manager, transaction_id)) != ERROR_SUCCESS)
    {
        rs_error("amf0 decode connect transaction_id failed,ret=%d", ret);
        return ret;
    }

    if (transaction_id != 1.0)
    {
        //because some client don't send transaction_id=1.0, only warn them
        rs_warn("amf0 decode connect transaction_id incorrect,transaction_id:%.1f,required:%.1f", 1.0, transaction_id);
    }

    if ((ret = command_object->Read(manager)) != ERROR_SUCCESS)
    {
        rs_error("amf0 decode connect command_object failed,ret=%d", ret);
        return ret;
    }

    if (!manager->Empty())
    {
        rs_freep(args);

        AMF0Any *p = nullptr;

        if ((ret = AMF0ReadAny(manager, &p)) != ERROR_SUCCESS)
        {
            rs_error("amf0 decode connect args failed,ret=%d", ret);
            return ret;
        }

        if (!p->IsObject())
        {
            rs_warn("drop connect args,marker=%#x", p->marker);
            rs_freep(p);
        }
        else
        {
            args = p->ToObject();
        }
    }

    rs_info("amf0 decode connect request success");

    return ret;
}

int ConnectAppPacket::GetPreferCID()
{
    return RTMP_CID_OVER_CONNECTION;
}
int ConnectAppPacket::GetMessageType()
{
    return RTMP_MSG_AMF0_COMMAND;
}

int ConnectAppPacket::GetSize()
{
    int size = 0;
    size += AMF0_LEN_STR(command_name);
    size += AMF0_LEN_NUMBER;
    size += AMF0_LEN_OBJECT(command_object);
    if (args)
    {
        size += AMF0_LEN_OBJECT(args);
    }
    return size;
}

int ConnectAppPacket::EncodePacket(BufferManager *manager)
{
    int ret = ERROR_SUCCESS;

    if ((ret = AMF0WriteString(manager, command_name)) != ERROR_SUCCESS)
    {
        rs_error("amf0 encode connect command_name failed,ret=%d", ret);
        return ret;
    }

    if ((ret = AMF0WriteNumber(manager, transaction_id)) != ERROR_SUCCESS)
    {
        rs_error("amf0 encode connect transaction_id failed,ret=%d");
        return ret;
    }

    if ((ret = command_object->Write(manager)) != ERROR_SUCCESS)
    {
        rs_error("amf0 encode connect command_object failed,ret=%d", ret);
        return ret;
    }

    if (args && (ret = args->Write(manager)) != ERROR_SUCCESS)
    {
        rs_error("amf0 encode connect args failed,ret=%d", ret);
        return ret;
    }

    return ret;
}

ConnectAppResPacket::ConnectAppResPacket() : command_name(RTMP_AMF0_COMMAND_RESULT),
                                             transaction_id(1),
                                             props(AMF0Any::Object()),
                                             info(AMF0Any::Object())
{
}

ConnectAppResPacket::~ConnectAppResPacket()
{
    rs_freep(info);
    rs_freep(props);
}

int ConnectAppResPacket::GetPreferCID()
{
    return RTMP_CID_PROTOCOL_CONTROL;
}

int ConnectAppResPacket::GetMessageType()
{
    return RTMP_MSG_AMF0_COMMAND;
}

int ConnectAppResPacket::Decode(BufferManager *manager)
{
    int ret = ERROR_SUCCESS;

    if ((ret = AMF0ReadString(manager, command_name)) != ERROR_SUCCESS)
    {
        rs_error("amf0 decode connect command_name failed,ret=%d", ret);
        return ret;
    }

    if (command_name.empty() || command_name != RTMP_AMF0_COMMAND_RESULT)
    {
        ret = ERROR_RTMP_AMF0_DECODE;
        rs_error("amf0 decode connect command_name failed,command_name=%s,ret=%d", command_name.c_str(), ret);
        return ret;
    }

    if ((ret = AMF0ReadNumber(manager, transaction_id)) != ERROR_SUCCESS)
    {
        rs_error("amf0 decode connect transaction failed,ret=%d", ret);
        return ret;
    }

    if (transaction_id != 1.0)
    {
        ret = ERROR_RTMP_AMF0_DECODE;
        rs_error("amf0 decode connect transaction failed,transaction_id=%.1f,ret=%d", transaction_id, ret);
        return ret;
    }
    {
        AMF0Any *p = nullptr;

        if ((ret = AMF0ReadAny(manager, &p)) != ERROR_SUCCESS)
        {
            rs_error("amf0 decode connect properties failed,ret=%d", ret);
            return ret;
        }

        if (!p->IsObject())
        {
            rs_warn("ignore decode connect properties,marker=%#x", p->marker);
            rs_freep(p);
        }
        else
        {
            rs_freep(props);
            props = p->ToObject();
        }
    }

    if ((ret = info->Read(manager)) != ERROR_SUCCESS)
    {
        rs_error("amf0 decode connect info failed,ret=%d", ret);
        return ret;
    }

    return ret;
}

int ConnectAppResPacket::GetSize()
{
    int size = 0;
    size += AMF0_LEN_STR(command_name);
    size += AMF0_LEN_NUMBER;
    size += AMF0_LEN_OBJECT(props);
    size += AMF0_LEN_OBJECT(info);
    return size;
}

int ConnectAppResPacket::EncodePacket(BufferManager *manager)
{
    int ret = ERROR_SUCCESS;

    if ((ret = AMF0WriteString(manager, command_name)) != ERROR_SUCCESS)
    {
        rs_error("amf0 encode connect.response.command_name failed,ret=%d", ret);
        return ret;
    }

    if ((ret = AMF0WriteNumber(manager, transaction_id)) != ERROR_SUCCESS)
    {
        rs_error("amf0 encode connect.response.transaction_id failed,ret=%d", ret);
        return ret;
    }

    if ((ret = props->Write(manager)) != ERROR_SUCCESS)
    {
        rs_error("amf0 encode connect.response.props failed,ret=%d", ret);
        return ret;
    }

    if ((ret = info->Write(manager)) != ERROR_SUCCESS)
    {
        rs_error("amf0 encode connect.response.info failed,ret=%d", ret);
        return ret;
    }

    return ret;
}

SetWindowAckSizePacket::SetWindowAckSizePacket() : ackowledgement_window_size(0)
{
}

SetWindowAckSizePacket::~SetWindowAckSizePacket()
{
}

int SetWindowAckSizePacket::Decode(BufferManager *manager)
{
    int ret = ERROR_SUCCESS;
    if (!manager->Require(4))
    {
        ret = ERROR_RTMP_MESSAGE_DECODE;
        rs_error("decode set_ackowledgement_window_size packet failed,ret=%d", ret);
        return ret;
    }

    ackowledgement_window_size = manager->Read4Bytes();

    return ret;
}

int SetWindowAckSizePacket::GetPreferCID()
{
    return RTMP_CID_PROTOCOL_CONTROL;
}

int SetWindowAckSizePacket::GetMessageType()
{
    return RTMP_MSG_WINDOW_ACK_SIZE;
}

int SetWindowAckSizePacket::GetSize()
{
    return 4;
}

int SetWindowAckSizePacket::EncodePacket(BufferManager *manager)
{
    int ret = ERROR_SUCCESS;

    if (!manager->Require(4))
    {
        ret = ERROR_RTMP_MESSAGE_ENCODE;
        rs_error("encode set_ackowledgement_window_size packet failed,ret=%d", ret);
        return ret;
    }

    manager->Write4Bytes(ackowledgement_window_size);

    return ret;
}

SetPeerBandwidthPacket::SetPeerBandwidthPacket() : bandwidth(0),
                                                   type((int8_t)PeerBandwidthType::DYNAMIC)
{
}

SetPeerBandwidthPacket::~SetPeerBandwidthPacket()
{
}

int SetPeerBandwidthPacket::GetPreferCID()
{
    return RTMP_CID_PROTOCOL_CONTROL;
}

int SetPeerBandwidthPacket::GetMessageType()
{
    return RTMP_MSG_SET_PEER_BANDWIDTH;
}

int SetPeerBandwidthPacket::GetSize()
{
    return 5;
}

int SetPeerBandwidthPacket::EncodePacket(BufferManager *manager)
{
    int ret = ERROR_SUCCESS;

    if (!manager->Require(5))
    {
        ret = ERROR_RTMP_MESSAGE_ENCODE;
        rs_error("encode set_peer_bandwidth_packet failed,ret=%d", ret);
        return ret;
    }

    manager->Write4Bytes(bandwidth);
    manager->Write1Bytes(type);

    return ret;
}

FMLEStartPacket::FMLEStartPacket() : command_name(RTMP_AMF0_COMMAND_RELEASE_STREAM),
                                     transaction_id(0),
                                     stream_name("")
{
}

FMLEStartPacket::~FMLEStartPacket()
{
}

int FMLEStartPacket::GetPreferCID()
{
    return RTMP_CID_OVER_CONNECTION;
}
int FMLEStartPacket::GetMessageType()
{
    return RTMP_MSG_AMF0_COMMAND;
}
int FMLEStartPacket::Decode(BufferManager *manager)
{
    int ret = ERROR_SUCCESS;

    if ((ret = AMF0ReadString(manager, command_name)) != ERROR_SUCCESS)
    {
        ret = ERROR_RTMP_AMF0_DECODE;
        rs_error("amf0 decode FMLE start packet command_name failed,ret=%d", ret);
        return ret;
    }

    if (command_name.empty() || (command_name != RTMP_AMF0_COMMAND_RELEASE_STREAM &&
                                 command_name != RTMP_AMF0_COMMAND_FC_PBLISH &&
                                 command_name != RTMP_AMF0_COMMAND_UNPUBLISH))
    {
        ret = ERROR_RTMP_AMF0_DECODE;
        rs_error("amf0 decode FMLE start packet check command_name failed,ret=%d", ret);
        return ret;
    }

    if ((ret = AMF0ReadNumber(manager, transaction_id)) != ERROR_SUCCESS)
    {
        ret = ERROR_RTMP_AMF0_DECODE;
        rs_error("amf0 decode FMLE start packet transaction_id failed,ret=%d", ret);
        return ret;
    }

    if ((ret = AMF0ReadNull(manager)) != ERROR_SUCCESS)
    {
        ret = ERROR_RTMP_AMF0_DECODE;
        rs_error("amf0 decode FMLE start packet null failed,ret=%d", ret);
        return ret;
    }

    if ((ret = AMF0ReadString(manager, stream_name)) != ERROR_SUCCESS)
    {
        ret = ERROR_RTMP_AMF0_DECODE;
        rs_error("amf0 decode FMLE start packet stream_name failed,ret=%d", ret);
        return ret;
    }

    return ret;
}

int FMLEStartPacket::EncodePacket(BufferManager *manager)
{
    int ret = ERROR_SUCCESS;

    if ((ret = AMF0WriteString(manager, command_name)) != ERROR_SUCCESS)
    {
        ret = ERROR_RTMP_AMF0_ENCODE;
        rs_error("amf0 encode FMLE start packet command_name failed,ret=%d", ret);
        return ret;
    }

    if ((ret = AMF0WriteNumber(manager, transaction_id)) != ERROR_SUCCESS)
    {
        ret = ERROR_RTMP_AMF0_ENCODE;
        rs_error("amf0 encode FMLE start packet transaction_id failed,ret=%d", ret);
        return ret;
    }

    if ((ret = AMF0WriteNull(manager)) != ERROR_SUCCESS)
    {
        ret = ERROR_RTMP_AMF0_ENCODE;
        rs_error("amf0 encode FMLE start packet null failed,ret=%d", ret);
        return ret;
    }
    if ((ret = AMF0WriteString(manager, stream_name)) != ERROR_SUCCESS)
    {
        ret = ERROR_RTMP_AMF0_ENCODE;
        rs_error("amf0 encode FMLE start packet stream_name failed,ret=%d", ret);
        return ret;
    }

    return ret;
}

int FMLEStartPacket::GetSize()
{
    int size = 0;
    size += AMF0_LEN_STR(command_name);
    size += AMF0_LEN_NUMBER;
    size += AMF0_LEN_NULL;
    size += AMF0_LEN_STR(stream_name);
    return size;
}

FMLEStartResPacket::FMLEStartResPacket(double trans_id) : transaction_id(trans_id),
                                                          command_name(RTMP_AMF0_COMMAND_RESULT),
                                                          stream_name("")
{
    command_object = AMF0Any::Null();
    args = AMF0Any::Undefined();
}

FMLEStartResPacket::~FMLEStartResPacket()
{
    rs_freep(command_object);
    rs_freep(args);
}

int FMLEStartResPacket::GetPreferCID()
{
    return RTMP_CID_OVER_CONNECTION;
}

int FMLEStartResPacket::GetMessageType()
{
    return RTMP_MSG_AMF0_COMMAND;
}

int FMLEStartResPacket::Decode(BufferManager *manager)
{
    int ret = ERROR_SUCCESS;

    if ((ret = AMF0ReadString(manager, command_name)) != ERROR_SUCCESS)
    {
        ret = ERROR_RTMP_AMF0_DECODE;
        rs_error("amf0 decode FMLE start response packet command_name failed,ret=%d", ret);
        return ret;
    }

    if (command_name.empty() || command_name != RTMP_AMF0_COMMAND_RESULT)
    {
        ret = ERROR_RTMP_AMF0_DECODE;
        rs_error("amf0 decode FMLE start response packet command_name check failed,ret=%d", ret);
        return ret;
    }

    if ((ret = AMF0ReadNumber(manager, transaction_id)) != ERROR_SUCCESS)
    {
        ret = ERROR_RTMP_AMF0_DECODE;
        rs_error("amf0 decode FMLE start response packet transaction_id failed,ret=%d", ret);
        return ret;
    }

    if ((ret = AMF0ReadNull(manager)) != ERROR_SUCCESS)
    {
        ret = ERROR_RTMP_AMF0_DECODE;
        rs_error("amf0 decode FMLE start response packet null failed,ret=%d", ret);
        return ret;
    }

    if ((ret = AMF0ReadUndefined(manager)) != ERROR_SUCCESS)
    {
        ret = ERROR_RTMP_AMF0_DECODE;
        rs_error("amf0 decode FMLE start response packet undefined failed,ret=%d", ret);
        return ret;
    }

    return ret;
}

int FMLEStartResPacket::GetSize()
{
    int size = 0;
    size += AMF0_LEN_STR(command_name);
    size += AMF0_LEN_NUMBER;
    size += AMF0_LEN_NULL;
    size += AMF0_LEN_UNDEFINED;
    return size;
}

int FMLEStartResPacket::EncodePacket(BufferManager *manager)
{
    int ret = ERROR_SUCCESS;

    if ((ret = AMF0WriteString(manager, command_name)) != ERROR_SUCCESS)
    {
        ret = ERROR_RTMP_AMF0_ENCODE;
        rs_error("amf0 encode FMLE start response packet command_name failed,ret=%d", ret);
        return ret;
    }

    if ((ret = AMF0WriteNumber(manager, transaction_id)) != ERROR_SUCCESS)
    {
        ret = ERROR_RTMP_AMF0_ENCODE;
        rs_error("amf0 encode FMLE start response packet transactoin_id failed,ret=%d", ret);
        return ret;
    }

    if ((ret = AMF0WriteNull(manager)) != ERROR_SUCCESS)
    {
        ret = ERROR_RTMP_AMF0_ENCODE;
        rs_error("amf0 encode FMLE start response packet null failed,ret=%d", ret);
        return ret;
    }

    if ((ret = AMF0WriteUndefined(manager)) != ERROR_SUCCESS)
    {
        ret = ERROR_RTMP_AMF0_ENCODE;
        rs_error("amf0 encode FMLE start response packet undefined failed,ret=%d", ret);
        return ret;
    }

    return ret;
}

AckWindowSize::AckWindowSize() : window(0),
                                 sequence_number(0),
                                 recv_bytes(0)
{
}

AckWindowSize::~AckWindowSize()
{
}

Protocol::Protocol(IProtocolReaderWriter *rw) : rw_(rw),
                                                in_chunk_size_(RTMP_PROTOCOL_CHUNK_SIZE),
                                                out_chunk_size_(RTMP_PROTOCOL_CHUNK_SIZE)
{
    in_buffer_ = new FastBuffer;
    cs_cache_ = new ChunkStream *[RTMP_CHUNK_STREAM_CHCAHE];
    for (int cid = 0; cid < RTMP_CHUNK_STREAM_CHCAHE; cid++)
    {
        ChunkStream *cs = new ChunkStream(cid);
        cs->header.perfer_cid = cid;
        cs_cache_[cid] = cs;
    }
}

Protocol::~Protocol()
{
    {
        std::map<int, ChunkStream *>::iterator it;
        for (it = chunk_streams_.begin(); it != chunk_streams_.end();)
        {
            ChunkStream *cs = it->second;
            rs_freep(cs);
            it = chunk_streams_.erase(it);
        }
    }
    {
        std::vector<Packet *>::iterator it;
        for (it = manual_response_queue_.begin(); it != manual_response_queue_.end();)
        {
            Packet *packet = *it;
            rs_freep(packet);
            it = manual_response_queue_.erase(it);
        }
    }

    rs_freep(in_buffer_);

    for (int cid = 0; cid < RTMP_CHUNK_STREAM_CHCAHE; cid++)
    {
        ChunkStream *cs = cs_cache_[cid];
        rs_freep(cs);
    }
    rs_freepa(cs_cache_);
}

void Protocol::SetSendTimeout(int64_t timeout_us)
{
    rw_->SetSendTimeout(timeout_us);
}

void Protocol::SetRecvTimeout(int64_t timeout_us)
{
    rw_->SetRecvTimeout(timeout_us);
}

int Protocol::ReadBasicHeader(char &fmt, int &cid)
{
    int ret = ERROR_SUCCESS;

    if ((ret = in_buffer_->Grow(rw_, 1)) != ERROR_SUCCESS)
    {
        if (!IsClientGracefullyClose(ret))
        {
            rs_error("read 1 bytes basic header failed,ret=%d", ret);
        }
        return ret;
    }
    // 0 1 2 3 4 5 6 7
    // +-+-+-+-+-+-+-+-+
    // |fmt|   cs id   |
    // +-+-+-+-+-+-+-+-+
    fmt = in_buffer_->Read1Bytes();
    cid = fmt & 0x3f;
    fmt = (fmt >> 6) & 0x03;

    // Value 0 indicates the ID in the range of
    // 64–319 (the second byte + 64). Value 1 indicates the ID in the range
    // of 64–65599 ((the third byte)*256 + the second byte + 64). Value 2
    // indicates its low-level protocol message. There are no additional
    // bytes for stream IDs. Values in the range of 3–63 represent the
    // complete stream ID. There are no additional bytes used to represent
    // it.

    //2-63
    if (cid > 1)
    {
        rs_verbose("basic header parsed,fmt=%d,cid=%d", fmt, cid);
        return ret;
    }

    //64-319
    if (cid == 0)
    {
        if ((ret = in_buffer_->Grow(rw_, 1)) != ERROR_SUCCESS)
        {
            if (!IsClientGracefullyClose(ret))
            {
                rs_error("read 2 bytes basic header failed,ret=%d", ret);
            }
            return ret;
        }

        cid = 64;
        cid += (uint8_t)in_buffer_->Read1Bytes();
        rs_verbose("basic header parsed,fmt=%d,cid=%d", fmt, cid);
    }
    //64–65599
    else if (cid == 1)
    {
        if ((ret = in_buffer_->Grow(rw_, 2)) != ERROR_SUCCESS)
        {
            if (!IsClientGracefullyClose(ret))
            {
                rs_error("read 3 bytes basic header failed,ret=%d", ret);
            }
            return ret;
        }

        cid = 64;
        cid += (uint8_t)in_buffer_->Read1Bytes();
        cid += ((uint8_t)in_buffer_->Read1Bytes()) * 256;
        rs_verbose("basic header parsed,fmt=%d,cid=%d", fmt, cid);
    }
    else
    {
        ret = ERROR_RTMP_BASIC_HEADER;
        rs_error("invaild cid,impossible basic header");
    }

    return ret;
}

/**
 * when fmt = 0,message header contain [timestamp delta][payload length][message type][stream id],11 bytes
 * when fmt = 1,message header contain [timestamp delta][payload length][message type],7 bytes
 * when fmt = 2,message header contain [timestamp delta],3 bytes 
 * when fmt = 3,message header is null,0 bytes
 */
int Protocol::ReadMessageHeader(ChunkStream *cs, char fmt)
{
    int ret = ERROR_SUCCESS;

    bool is_first_msg_of_chunk = !cs->msg;
    if (cs->msg_count == 0 && fmt != RTMP_FMT_TYPE0)
    {
        //for librtmp,if ping,it will send a fresh stream with fmt 1
        // 0x42             where: fmt=1, cid=2, protocol contorl user-control message
        // 0x00 0x00 0x00   where: timestamp=0
        // 0x00 0x00 0x06   where: payload_length=6
        // 0x04             where: message_type=4(protocol control user-control message)
        // 0x00 0x06            where: event Ping(0x06)
        // 0x00 0x00 0x0d 0x0f  where: event data 4bytes ping timestamp.
        if (cs->cid == RTMP_CID_PROTOCOL_CONTROL && fmt == RTMP_FMT_TYPE1)
        {
            rs_warn("accept cid=2,fmt=1 to make librtmp work");
        }
        else
        {
            ret = ERROR_RTMP_CHUNK_START;
            rs_error("chunk stream is fresh,fmt mush be %d,actual is %d,cid=%d,ret=%d", RTMP_FMT_TYPE0, fmt, cs->cid, ret);
            return ret;
        }
    }

    if (cs->msg && fmt == RTMP_FMT_TYPE0)
    {
        ret = ERROR_RTMP_CHUNK_START;
        rs_error("chunk stream exists,fmt could not be %d,actual is %d,cid=%d,ret=%d", RTMP_FMT_TYPE0, fmt, cs->cid, ret);
        return ret;
    }

    if (!cs->msg)
    {
        cs->msg = new CommonMessage;
        rs_verbose("create message for new chunk,fmt=%d,cid=%d", fmt, cs->cid);
    }

    static const char mh_sizes[] = {11, 7, 3, 0};
    int mh_size = mh_sizes[(int)fmt];
    rs_verbose("calc chunk message header size,fmt=%d,mh_size=%d", fmt, mh_size);

    if (mh_size > 0 && (ret = in_buffer_->Grow(rw_, mh_size)) != ERROR_SUCCESS)
    {
        if (!IsClientGracefullyClose(ret))
        {
            rs_error("read %d bytes message header failed,ret=%d", mh_size, ret);
        }
        return ret;
    }

    char *ptr = in_buffer_->ReadSlice(mh_size);
    BufferManager manager;
    if ((ret = manager.Initialize(ptr, mh_size)) != ERROR_SUCCESS)
    {
        rs_error("initialize buffer manager failed,ret=%d", ret);
        return ret;
    }

    if (fmt <= RTMP_FMT_TYPE2)
    {
        cs->header.timestamp_delta = manager.Read3Bytes();
        cs->extended_timestamp = (cs->header.timestamp_delta >= RTMP_EXTENDED_TIMESTAMP);

        if (!cs->extended_timestamp)
        {
            if (fmt == RTMP_FMT_TYPE0)
            {
                cs->header.timestamp = cs->header.timestamp_delta;
            }
            else
            {
                cs->header.timestamp += cs->header.timestamp_delta;
            }
        }
        rs_verbose("chunk message timestamp=%lld", cs->header.timestamp);

        if (fmt <= RTMP_FMT_TYPE1)
        {
            int32_t payload_length = manager.Read3Bytes();

            if (!is_first_msg_of_chunk && cs->header.payload_length != payload_length)
            {
                ret = ERROR_RTMP_CHUNK_START;
                rs_error("msg exists in chunk cache,size=%d,could not change to %d,ret=%d", cs->header.payload_length, payload_length, ret);
                return ret;
            }

            cs->header.payload_length = payload_length;
            cs->header.message_type = manager.Read1Bytes();
            if (fmt <= RTMP_FMT_TYPE0)
            {
                cs->header.stream_id = manager.Read4Bytes();
                rs_verbose("header read completed,fmt=%d,mh_size=%d,ext_time=%d,time=%lld,payload=%d,####type=%d,sid=%d",
                           fmt, mh_size, cs->extended_timestamp, cs->header.timestamp, cs->header.payload_length,
                           cs->header.message_type, cs->header.stream_id);
            }
            else
            {
                rs_verbose("header read completed,fmt=%d,mh_size=%d,ext_time=%d,time=%lld,payload=%d,type=%d",
                           fmt, mh_size, cs->extended_timestamp, cs->header.timestamp, cs->header.payload_length,
                           cs->header.message_type);
            }
        }
        else
        {
            rs_verbose("header read completed,fmt=%d,mh_size=%d,ext_time=%d,time=%lld",
                       fmt, mh_size, cs->extended_timestamp, cs->header.timestamp);
        }
    }
    else
    {
        if (is_first_msg_of_chunk && !cs->extended_timestamp)
        {
            cs->header.timestamp += cs->header.timestamp_delta;
        }

        rs_verbose("header read completed,fmt=%d,size=%d,ext_time=%d", fmt, mh_size, cs->extended_timestamp);
    }

    if (cs->extended_timestamp)
    {
        mh_size += 4;
        rs_verbose("read header ext time,fmt=%d,ext_time=%d,mh_size=%d", fmt, cs->extended_timestamp, mh_size);
        if ((ret = in_buffer_->Grow(rw_, 4)) != ERROR_SUCCESS)
        {
            if (!IsClientGracefullyClose(ret))
            {
                rs_error("read %d bytes message header failed,required_size=%d,ret=%d", mh_size, 4, ret);
            }
            return ret;
        }

        ptr = in_buffer_->ReadSlice(4);
        manager.Initialize(ptr, 4);

        uint32_t timestamp = manager.Read4Bytes();
        timestamp &= 0x7fffffff;

        uint32_t chunk_timestamp = (uint32_t)cs->header.timestamp;

        /**
        * example 1:
        * (first_packet,without extended ts,ts = 0) --> (second_packet,with extended ts,exts=40) is ok
        * example 2:
        * (first_packet,without extended ts,ts = 0) --> (second_packet,without extended ts,ts=40) --> (third_packet,with extended ts,exts=40)
        */
        if (!is_first_msg_of_chunk && chunk_timestamp > 0 && chunk_timestamp != timestamp)
        {
            mh_size -= 4;
            in_buffer_->Skip(-4);
            rs_warn("no 4 bytes extended timestamp in the continue chunk");
        }
        else
        {
            cs->header.timestamp = timestamp;
        }
        rs_verbose("header read extended timestamp completed,time=%lld", cs->header.timestamp);
    }

    cs->header.timestamp &= 0x7ffffff;
    cs->msg->header = cs->header;
    cs->msg_count++;

    return ret;
}

int Protocol::ReadMessagePayload(ChunkStream *cs, CommonMessage **pmsg)
{
    int ret = ERROR_SUCCESS;

    if (cs->header.payload_length <= 0)
    {
        rs_warn("get an empty rtmp messge(type=%d,size=%d,time=%lld,sid=%d)", cs->header.message_type, cs->header.payload_length, cs->header.timestamp, cs->header.stream_id);
        *pmsg = cs->msg;
        cs->msg = nullptr;
        return ret;
    }

    int payload_size = cs->header.payload_length - cs->msg->size;
    payload_size = rs_min(cs->header.payload_length, in_chunk_size_);

    rs_verbose("chunk payload size is %d,message_size=%d,recveived_size=%d,in_chunk_size=%d", payload_size, cs->header.payload_length, cs->msg->size, in_chunk_size_);

    if (!cs->msg->payload)
    {

        cs->msg->CreatePayload(payload_size);
    }

    if ((ret = in_buffer_->Grow(rw_, payload_size)) != ERROR_SUCCESS)
    {
        if (!IsClientGracefullyClose(ret))
        {
            rs_error("read payload failed,required_size=%d,ret=%d", payload_size, ret);
        }
        return ret;
    }

    memcpy(cs->msg->payload + cs->msg->size, in_buffer_->ReadSlice(payload_size), payload_size);
    cs->msg->size += payload_size;

    rs_verbose("chunk payload read completed,payload size=%d", payload_size);
    if (cs->header.payload_length == cs->msg->size)
    {
        //got entire rtmp message
        *pmsg = cs->msg;
        cs->msg = nullptr;
        rs_verbose("got entire rtmp message(type=%d,size=%d,time=%lld,sid=%d)", cs->header.message_type, cs->header.payload_length, cs->header.timestamp, cs->header.stream_id);
        return ret;
    }
    rs_verbose("got part of rtmp message(type=%d,size=%d,time=%lld,size=%d),partial size=%d", cs->header.message_type, cs->header.payload_length, cs->header.timestamp, cs->header.stream_id, cs->msg->size);
    return ret;
}

int Protocol::RecvInterlacedMessage(CommonMessage **pmsg)
{
    int ret = ERROR_SUCCESS;
    char fmt = 0;
    int cid = 0;
    if ((ret = ReadBasicHeader(fmt, cid)) != ERROR_SUCCESS)
    {
        if (!IsClientGracefullyClose(ret))
        {
            rs_error("read basic header failed,ret=%d", ret);
        }
        return ret;
    }

    rs_verbose("read basic header success,fmt=%d,cid=%d", fmt, cid);

    ChunkStream *cs = nullptr;
    if (cid < RTMP_CHUNK_STREAM_CHCAHE)
    {
        rs_verbose("cs-cache hint,cid=%d", cid);
        cs = cs_cache_[cid];
        rs_verbose("cache chunk stream:fmt=%d,cid=%d,size=%d,msg(type=%d,size=%d,time=%lld,sid=%d)",
                   fmt, cid, (cs->msg ? cs->msg->size : 0),
                   cs->header.message_type, cs->header.payload_length,
                   cs->header.timestamp, cs->header.stream_id);
    }
    else
    {
        if (chunk_streams_.find(cid) == chunk_streams_.end())
        {
            cs = new ChunkStream(cid);
            cs->header.perfer_cid = cid;
            chunk_streams_[cid] = cs;
            rs_verbose("cache new chunk stream:fmt=%d,cid=%d", fmt, cid);
        }
        else
        {
            cs = chunk_streams_[cid];
            rs_verbose("cache chunk stream:fmt=%d,cid=%d,size=%d,msg(type=%d,size=%d,time=%lld,sid=%d)",
                       fmt, cid, (cs->msg ? cs->msg->size : 0),
                       cs->header.message_type, cs->header.payload_length,
                       cs->header.timestamp, cs->header.stream_id);
        }
    }

    if ((ret = ReadMessageHeader(cs, fmt)) != ERROR_SUCCESS)
    {
        if (!IsClientGracefullyClose(ret))
        {
            rs_error("read message header failed,ret=%d", ret);
        }
        return ret;
    }

    rs_verbose("read message header success,fmt=%d.ext_time=%d,size=%d,message(type=%d,size=%d,time=%lld,sid=%d)",
               fmt, cs->extended_timestamp, (cs->msg ? cs->msg->size : 0), cs->header.message_type, cs->header.payload_length, cs->header.timestamp, cs->header.stream_id);

    CommonMessage *msg = nullptr;
    if ((ret = ReadMessagePayload(cs, &msg)) != ERROR_SUCCESS)
    {
        if (!IsClientGracefullyClose(ret))
        {
            rs_error("read message payload failed,ret=%d", ret);
        }
        return ret;
    }

    if (!msg)
    {
        return ret;
    }

    *pmsg = msg;
    return ret;
}

int Protocol::RecvMessage(CommonMessage **pmsg)
{
    int ret = ERROR_SUCCESS;
    *pmsg = nullptr;

    while (true)
    {
        CommonMessage *msg = nullptr;
        if ((ret = RecvInterlacedMessage(&msg)) != ERROR_SUCCESS)
        {
            if (!IsClientGracefullyClose(ret))
            {
                rs_error("recv interlaced message failed,ret=%d", ret);
            }
            rs_freep(msg);
            return ret;
        }

        if (!msg)
        {
            continue;
        }

        rs_verbose("entire message receviced");

        if (msg->size <= 0 || msg->header.payload_length <= 0)
        {
            //empty mesage
            rs_warn("got empty message");
            rs_freep(msg);
            continue;
        }

        if ((ret = OnRecvMessage(msg)) != ERROR_SUCCESS)
        {
            rs_error("hook the received message failed,ret=%d", ret);
            rs_freep(msg);
            continue;
        }
        rs_verbose("got a message,cid=%d,type=%d,size=%d,time=%lld",
                   msg->header.perfer_cid,
                   msg->header.message_type,
                   msg->header.payload_length,
                   msg->header.timestamp);
        *pmsg = msg;
        break;
    }

    return ret;
}

int Protocol::ResponseAckMessage()
{
    int ret = ERROR_SUCCESS;
    if (in_ack_size_.window <= 0)
    {
        return ret;
    }

    return ret;
}

int Protocol::DoDecodeMessage(MessageHeader &header, BufferManager *manager, Packet **ppacket)
{
    int ret = ERROR_SUCCESS;

    Packet *packet = nullptr;

    //no support amf3. If command message is amf3,direct return error.
    if (header.IsAMF3Command() || header.IsAMF3Data())
    {
        ret = ERROR_RTMP_AMF3_NO_SUPPORT;
        rs_error("decode amf3 command message failed,no support amf3,ret=%d", ret);
        return ret;
    }
    else if (header.IsAMF0Command() || header.IsAMF0Data())
    {
        rs_verbose("start to decode amf0 command message");
        std::string command;
        if ((ret = AMF0ReadString(manager, command)) != ERROR_SUCCESS)
        {
            rs_error("decode amf0 command name failed,ret=%d", ret);
            return ret;
        }

        rs_verbose("decode amf0 command name success,command_name=%s", command.c_str());

        // double transactionId = 0.0;
        // if ((ret = AMF0ReadNumber(manager, &transactionId)) != ERROR_SUCCESS)
        // {

        // }

        //reset buffer manager. start to decode amf0 packet
        manager->Skip(-1 * manager->Pos());

        if (command == RTMP_AMF0_COMMAND_CONNECT)
        {
            rs_verbose("decode amf0 command message(connect)");
            *ppacket = packet = new ConnectAppPacket;
            return packet->Decode(manager);
        }
        else if (command == RTMP_AMF0_COMMAND_RELEASE_STREAM)
        {
            rs_verbose("decode amf0 command message(releaseStream)");
            *ppacket = packet = new FMLEStartPacket;
            return packet->Decode(manager);
        }

        // return ret;
    }
    else if (header.IsSetChunkSize())
    {
        rs_verbose("start to decode set chunk size message");
        *ppacket = packet = new SetChunkSizePacket;
        return packet->Decode(manager);
    }

    return ret;
}

int Protocol::OnSendPacket(MessageHeader *header, Packet *packet)
{
    int ret = ERROR_SUCCESS;

    switch (header->message_type)
    {
    case RTMP_MSG_WINDOW_ACK_SIZE:
        SetWindowAckSizePacket *pkt = dynamic_cast<SetWindowAckSizePacket *>(packet);
        out_ack_size_.window = (uint32_t)pkt->ackowledgement_window_size;
        break;
    }
    return ret;
}

int Protocol::DoSendAndFreePacket(Packet *packet, int stream_id)
{
    int ret = ERROR_SUCCESS;

    rs_auto_free(Packet, packet);

    int size = 0;
    char *payload = nullptr;

    if ((ret = packet->Encode(size, payload)) != ERROR_SUCCESS)
    {
        rs_error("encode rtmp packet to bytes failed,ret=%d", ret);
        return ret;
    }

    if (size <= 0 || payload == nullptr)
    {
        rs_warn("packet is empty,ignore empty message");
        return ret;
    }

    MessageHeader header;
    header.payload_length = size;
    header.message_type = packet->GetMessageType();
    header.perfer_cid = packet->GetPreferCID();
    header.stream_id = stream_id;

    ret = DoSimpleSend(&header, payload, size);
    rs_freep(payload);
    if (ret == ERROR_SUCCESS)
    {
        ret = OnSendPacket(&header, packet);
    }

    return ret;
}

int Protocol::DoSimpleSend(MessageHeader *header, char *payload, int size)
{
    int ret = ERROR_SUCCESS;

    char *p = payload;
    char *end = payload + size;

    char c0c3[RTMP_FMT0_HEADER_SIZE];

    while (p < end)
    {
        int nbh = 0;
        if (p == payload)
        {
            nbh = chunk_header_c0(header->perfer_cid,
                                  header->timestamp,
                                  header->payload_length,
                                  header->message_type,
                                  header->stream_id,
                                  c0c3);
        }
        else
        {
            nbh = chunk_header_c3(header->perfer_cid, header->timestamp, c0c3);
        }

        iovec iovs[2];
        iovs[0].iov_base = c0c3;
        iovs[0].iov_len = nbh;

        int payload_size = rs_min(end - p, out_chunk_size_);
        iovs[1].iov_base = p;
        iovs[1].iov_len = payload_size;
        p += payload_size;

        if ((ret = rw_->WriteEv(iovs, 2, nullptr)) != ERROR_SUCCESS)
        {
            if (!IsClientGracefullyClose(ret))
            {
                rs_error("send packet with writev failed,ret=%d", ret);
            }
            return ret;
        }
    }

    return ret;
}

int Protocol::DecodeMessage(CommonMessage *msg, Packet **ppacket)
{
    int ret = ERROR_SUCCESS;
    *ppacket = nullptr;

    BufferManager manager;
    if ((ret = manager.Initialize(msg->payload, msg->size)) != ERROR_SUCCESS)
    {
        rs_error("initialize buffer manager failed,ret=%d", ret);
        return ret;
    }

    Packet *packet = nullptr;
    if ((ret = DoDecodeMessage(msg->header, &manager, &packet)) != ERROR_SUCCESS)
    {
        rs_error("do decode message failed,ret=%d", ret);
        return ret;
    }

    *ppacket = packet;

    return ret;
}

int Protocol::ManualResponseFlush()
{
    int ret = ERROR_SUCCESS;

    std::vector<Packet *>::iterator it;
    for (it = manual_response_queue_.begin(); it != manual_response_queue_.end(); it++)
    {
        Packet *packet = *it;

        it = manual_response_queue_.erase(it);
        if ((ret = DoSendAndFreePacket(packet, 0)) != ERROR_SUCCESS)
        {
            return ret;
        }
    }

    return ret;
}

int Protocol::SendAndFreePacket(Packet *packet, int stream_id)
{
    int ret = ERROR_SUCCESS;

    if ((ret = DoSendAndFreePacket(packet, stream_id)) != ERROR_SUCCESS)
    {
        return ret;
    }

    if ((ret = ManualResponseFlush()) != ERROR_SUCCESS)
    {
        return ret;
    }

    return ret;
}

int Protocol::OnRecvMessage(CommonMessage *msg)
{
    int ret = ERROR_SUCCESS;

    if ((ret = ResponseAckMessage()) != ERROR_SUCCESS)
    {
        return ret;
    }

    //only handle rtmp control message,but not rtmp command message
    Packet *packet = nullptr;
    switch (msg->header.message_type)
    {
    case RTMP_MSG_SET_CHUNK_SIZE:
    case RTMP_MSG_USER_CONTROL_MESSAGE:
    case RTMP_MSG_WINDOW_ACK_SIZE:
        if ((ret = DecodeMessage(msg, &packet)) != ERROR_SUCCESS)
        {
            rs_error("decode packet from message payload failed,ret=%d", ret);
            return ret;
        }
        rs_verbose("decode packet from message payload success");
        break;
    default:
        //If recv amf0/amf3 message,just return
        return ret;
    }

    rs_auto_free(Packet, packet);

    switch (msg->header.message_type)
    {
    case RTMP_MSG_SET_CHUNK_SIZE:
    {
        SetChunkSizePacket *pkt = dynamic_cast<SetChunkSizePacket *>(packet);
        if (pkt->chunk_size < RTMP_MIN_CHUNK_SIZE || pkt->chunk_size > RTMP_MAX_CHUNK_SIZE)
        {
            rs_warn("accept chunk size:%d", pkt->chunk_size);
            if (pkt->chunk_size < RTMP_MIN_CHUNK_SIZE)
            {
                ret = ERROR_RTMP_CHUNK_START;
                rs_error("chunk size should be %d+,value=%d,ret=%d", RTMP_MIN_CHUNK_SIZE, pkt->chunk_size, ret);
                return ret;
            }
        }

        in_chunk_size_ = pkt->chunk_size;
        rs_verbose("in_chunk_size=%d", pkt->chunk_size);
        break;
    }
    }

    return ret;
}

} // namespace rtmp
