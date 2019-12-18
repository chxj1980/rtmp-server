#ifndef RS_THREAD_HPP
#define RS_THREAD_HPP

#include <common/core.hpp>

#include <st.h>

namespace internal
{
class IThreadHandler
{
public:
    IThreadHandler();
    virtual ~IThreadHandler();

public:
    virtual int OnBeforeCycle();
    virtual void OnThreadStart();
    virtual int Cycle();
    virtual int OnEndCycle();
    virtual void OnThreadStop();
};

class Thread
{
public:
    Thread(const char *name, IThreadHandler *thread_handler, int64_t interval_us, bool joinable);
    ~Thread();

public:
    virtual int Start();
    virtual void Stop();
    virtual bool CanLoop();
    virtual void StopLoop();

protected:
    virtual void Dispatch();
    virtual void Dispose();
    static void *Function(void *arg);

private:
    const char *name_;
    IThreadHandler *handler_;
    int64_t interval_us_;
    bool joinable_;
    st_thread_t st_;
    bool loop_;
    bool really_terminated_;
    bool disposed_;
    bool can_run_;
    int cid_;
};

} // namespace internal

#endif