#pragma once

#include "config/config.hpp"
#include "config/pch.hpp"

MKS_BEGIN

using ::ilias::IoResult;
using ::ilias::IoTask;
using ::ilias::Task;
using ::ilias::Result;
using ::ilias::Err;

// Overloads
template <typename ...Ts>
struct Overloads : Ts... { using Ts::operator()...; };

MKS_END