#pragma once

#include <ilias/io.hpp>
#include <ilias/net.hpp>

#include "../proto/codec.hpp"

namespace mksync::transport {

using BufferedTcpStream = ilias::BufStream<ilias::TcpStream>;

auto writeFrame(BufferedTcpStream &stream, const proto::Frame &frame) -> ilias::IoTask<void>;
auto readFrame(BufferedTcpStream &stream) -> ilias::IoTask<proto::Frame>;

auto makeHello(std::string deviceName, uint32_t screenCount = 0, uint32_t version = 1) -> proto::Frame;
auto makeHelloAck(uint32_t screenCount = 0, uint32_t version = 1) -> proto::Frame;
auto makePing(uint64_t timestampUs) -> proto::Frame;
auto makePong(uint64_t timestampUs) -> proto::Frame;

} // namespace mksync::transport

