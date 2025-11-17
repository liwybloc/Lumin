#pragma once
#ifndef EXECUTILS_H
#define EXECUTILS_H

#include <memory>
#include <vector>
#include <variant>
#include <string>
#include <functional>
#include <stdexcept>
#include <iostream>
#include "executor.hpp"

template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };

template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

#endif