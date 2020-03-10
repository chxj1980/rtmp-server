/*
 * @Author: linmin
 * @Date: 2020-03-10 10:29:30
 * @LastEditTime: 2020-03-10 16:40:32
 * @LastEditors: Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: \rtmp_server\protocol\rtmp\jitter.cpp
 */
#include <protocol/rtmp/jitter.hpp>
#include <protocol/rtmp/message.hpp>
#include <protocol/rtmp/defines.hpp>
#include <common/error.hpp>
#include <common/log.hpp>
#include <common/utils.hpp>

namespace rtmp
{

Jitter::Jitter()
{
    last_pkt_time_ = 0;
    last_pkt_correct_time_ = -1;
}

Jitter::~Jitter()
{
}

int Jitter::Correct(SharedPtrMessage *msg, JitterAlgorithm ag)
{
    int ret = ERROR_SUCCESS;
    if (ag != JitterAlgorithm::FULL)
    {
        if (ag == JitterAlgorithm::OFF)
        {
            return ret;
        }
        if (ag == JitterAlgorithm::ZERO)
        {
            //ensure timestamp start at zero
            if (last_pkt_correct_time_ == -1)
            {
                last_pkt_correct_time_ = msg->timestamp;
            }
            msg->timestamp -= last_pkt_correct_time_;
            return ret;
        }

        return ret;
    }

    if (!msg->IsAV())
    {
        msg->timestamp = 0;
        return ret;
    }

    int64_t time = msg->timestamp;
    int64_t delta = time - last_pkt_time_;

    if (delta < RTMP_MAX_JITTER_MS_NEG || delta > RTMP_MAX_JITTER_MS)
    {
        delta = RTMP_DEFAULT_FRAME_TIME_MS;
    }

    last_pkt_correct_time_ = rs_max(0, last_pkt_correct_time_ + delta);

    msg->timestamp = last_pkt_correct_time_;
    last_pkt_time_ = time;

    return ret;
}

int Jitter::GetTime()
{
    return last_pkt_correct_time_;
}

} // namespace rtmp