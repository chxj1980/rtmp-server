#ifndef RS_CONNECTION_HPP
#define RS_CONNECTION_HPP

#include <common/core.hpp>
#include <common/thread.hpp>
#include <protocol/statistics.hpp>

#include <string>

class Connection;

class IConnectionManager
{
public:
    IConnectionManager();
    virtual ~IConnectionManager();

public:
    virtual void Remove(Connection *conn) = 0;
};

class Connection : public virtual IKbpsDelta,
                   public virtual internal::IThreadHandler
{
public:
    Connection(IConnectionManager *conn_manager, st_netfd_t client_stfd);
    virtual ~Connection();

public:
    virtual void Dispose();
    virtual int Start();
    virtual int Cycle() override;
    virtual void OnThreadStop() override;
    virtual int GetID();
    virtual void SetExpire();

protected:
    virtual int DoCycle() = 0;

protected:
    IConnectionManager *conn_manager_;
    st_netfd_t client_stfd_;
    std::string client_ip_;
    bool disposed_;
    bool expired_;

private:
    int id_;
    internal::Thread *thread_;
};
#endif