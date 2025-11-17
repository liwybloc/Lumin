#pragma once
#ifndef FILESTREAM_H
#define FILESTREAM_H

#include <memory>
#include <vector>
#include "executor.hpp"

void addFilestream(std::shared_ptr<Environment> globalEnv, Executor* executor);

#endif