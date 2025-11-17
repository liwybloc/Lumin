#include "executor.hpp"
#include "executils.hpp"
#include <iostream>

void addOutstream(std::shared_ptr<Environment> globalEnv, Executor* executor) {
    auto printFunc = [executor](const std::vector<std::shared_ptr<Value>> &args, bool newline) -> std::shared_ptr<Value> {
        for (const auto &arg : args) executor->printValue(*arg);
        if (newline) std::cout << std::endl;
        return std::make_shared<Value>(0);
    };

    globalEnv->set("print", std::make_shared<Function>(Function{
        [printFunc](const std::vector<std::shared_ptr<Value>> &args) { return printFunc(args, false); }
    }));
    globalEnv->set("println", std::make_shared<Function>(Function{
        [printFunc](const std::vector<std::shared_ptr<Value>> &args) { return printFunc(args, true); }
    }));
    globalEnv->set("printf", std::make_shared<Function>(Function{
        [executor](const std::vector<std::shared_ptr<Value>> &args) -> std::shared_ptr<Value> {
            if (args.empty()) return std::make_shared<Value>(0);

            std::string format = executor->getStringValue(*args[0]);
            size_t argIndex = 1;
            size_t pos = 0;

            while ((pos = format.find("{}", pos)) != std::string::npos && argIndex < args.size()) {
                std::string str = executor->getStringValue(*args[argIndex]);
                format.replace(pos, 2, str);
                pos += str.size();
                argIndex++;
            }

            std::cout << format;
            return std::make_shared<Value>(0);
        }
    }));
}