#pragma once

#include <functional>

#include "base_socket.h"

namespace taosocks {

namespace {

using namespace base_socket;

struct ReadIOContext : BaseIOContext
{
    ReadIOContext()
        : BaseIOContext(OpType::Read)
    {
        wsabuf.buf = (char*)buf;
        wsabuf.len = _countof(buf);
    }

    WSARet Read(SOCKET fd)
    {
        DWORD dwFlags = 0;

        WSAIntRet R = ::WSARecv(
            fd,
            &wsabuf, 1,
            nullptr,
            &dwFlags,
            &overlapped,
            nullptr
        );

        return R;
    }

    WSABUF wsabuf;
    unsigned char buf[65536];
};

struct WriteIOContext : BaseIOContext
{
    WriteIOContext()
        : BaseIOContext(OpType::Write)
    {
        wsabuf.buf = nullptr;
    }
    ~WriteIOContext()
    {
        if(wsabuf.buf != nullptr) {
            delete[] wsabuf.buf;
            wsabuf.buf = nullptr;
        }
    }

    WSARet Write(SOCKET fd, const unsigned char* data, size_t size)
    {
        DWORD dwFlags = 0;

        wsabuf.buf = new char[size];
        ::memcpy(wsabuf.buf, data, size);
        //wsabuf.buf = (char*)data;
        wsabuf.len = size;

        WSAIntRet R = ::WSASend(
            fd,
            &wsabuf, 1,
            nullptr,
            dwFlags,
            &overlapped,
            nullptr
        );

        return R;
    }

    WSABUF wsabuf;
};

struct ConnectIOContext : BaseIOContext
{
    ConnectIOContext()
        : BaseIOContext(OpType::Connect)
    { }

    WSARet Connect(SOCKET fd, const sockaddr_in& addr)
    {
        WSABoolRet R = WSA::ConnectEx(
            fd, 
            (sockaddr*)&addr, sizeof(addr), 
            nullptr, 0,
            nullptr,
            &overlapped
        );

        return R;
    }
};


}


class ClientSocket: public BaseSocket
{
private:
    struct Flags
    {
        enum Value
        {
            Closed  = 1 << 0,
        };
    };

    struct ConnectDispatchData : BaseDispatchData
    {
        ConnectDispatchData()
            : BaseDispatchData(OpType::Connect)
        { }
    };

    struct ReadDispatchData : BaseDispatchData
    {
        ReadDispatchData()
            : BaseDispatchData(OpType::Read)
        { }

        unsigned char* data;
        size_t size;
    };

    struct WriteDispatchData : BaseDispatchData
    {
        WriteDispatchData()
            : BaseDispatchData(OpType::Write)
        { }

        size_t size;
    };

    struct CloseDispatchData : BaseDispatchData
    {
        CloseDispatchData()
            : BaseDispatchData(OpType::Close)
        { }
    };

public:
    ClientSocket(threading::Dispatcher& disp)
        : BaseSocket(disp)
        , flags(0)
    {

    }

    ClientSocket(Dispatcher& disp, SOCKET fd, const SOCKADDR_IN& local, const SOCKADDR_IN& remote)
        : BaseSocket(disp, fd)
        , flags(0)
        , _local(local)
        , _remote(remote)
    {

    }

    typedef std::function<void(ClientSocket*, unsigned char*, size_t)> OnReadT;
    typedef std::function<void(ClientSocket*, size_t)> OnWrittenT;
    typedef std::function<void(ClientSocket*)> OnClosedT;
    typedef std::function<void(ClientSocket*)> OnConnectedT;

private:
    SOCKADDR_IN _local;
    SOCKADDR_IN _remote;
    OnReadT _onRead;
    OnWrittenT _onWritten;
    OnClosedT _onClosed;
    OnConnectedT _onConnected;
    DWORD flags;


public:
    void OnRead(OnReadT onRead);
    void OnWritten(OnWrittenT onWritten);
    void OnClosed(OnClosedT onClose);
    void OnConnected(OnConnectedT onConnected);

    WSARet Connect(in_addr & addr, unsigned short port);
    WSARet Read();
    WSARet Write(const char* data, size_t size, void* tag);
    WSARet Write(const char* data, void* tag);
    WSARet Write(const unsigned char* data, size_t size, void* tag);
    void Close();

private:
    void _OnRead(ReadIOContext& io);
    WSARet _OnWritten(WriteIOContext& io);
    WSARet _OnConnected(ConnectIOContext& io);


public:
    virtual void OnDispatch(BaseDispatchData& data) override;

    virtual void OnTask(BaseIOContext& bio) override;
    virtual int GetDescriptor() override;
};


}