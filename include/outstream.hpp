#pragma once
#ifndef OUTSTREAM_H
#define OUTSTREAM_H

#include <memory>
#include <vector>
#include "executor.hpp"

void addOutstream(std::shared_ptr<Environment> globalEnv, Executor* executor);

#endif