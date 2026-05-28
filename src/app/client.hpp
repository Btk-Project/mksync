#pragma once

#include "preinclude.hpp"
#include "refl/formatter.hpp"
#include "core.hpp"
#include <ilias/task.hpp>
#include <ilias/net.hpp>
#include <map>

MKS_BEGIN

using ilias::IPEndpoint;
using ilias::TcpListener;
using ilias::TcpStream;

class Client {
public:
    Client() = default;
    Client(const Client &) = delete;
    ~Client();

    auto run(IPEndpoint server) -> IoTask<void>;
private:

};

MKS_END