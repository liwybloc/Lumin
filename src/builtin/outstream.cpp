#include "executor.hpp"
#include "executils.hpp"
#include <iostream>

void addOutstream(std::shared_ptr<Environment> globalEnv, Executor* executor) {
    auto printFunc = [executor](const std::vector<std::shared_ptr<TypedValue>> &args, bool newline) -> std::shared_ptr<TypedValue> {
        for (const auto &arg : args) executor->printValue(&std::cout, *arg);
        if (newline) std::cout << std::endl;
        return std::make_shared<TypedValue>(0);
    };

    globalEnv->set("print", {std::make_shared<Function>(Function{
        [printFunc](const std::vector<std::shared_ptr<TypedValue>> &args) { return printFunc(args, false); }
    })});
    globalEnv->set("println", {std::make_shared<Function>(Function{
        [printFunc](const std::vector<std::shared_ptr<TypedValue>> &args) { return printFunc(args, true); }
    })});
    globalEnv->set("printf", {std::make_shared<Function>(Function{
        [executor](const std::vector<std::shared_ptr<TypedValue>> &args) -> std::shared_ptr<TypedValue> {
            if (args.empty()) return std::make_shared<TypedValue>(0);

            std::string format = executor->getStringValue(*args[0]);
            size_t argIndex = 1;
            size_t pos = 0;

            std::stringstream stream;

            while ((pos = format.find("{}", pos)) != std::string::npos && argIndex < args.size()) {
                std::string before = format.substr(0, pos);
                stream << before;
                format = format.substr(pos + 2);

                executor->printValue(&stream, *args[argIndex]);
                argIndex++;
                pos = 0;
            }

            stream << format;
            std::cout << stream.str();

            return std::make_shared<TypedValue>(0);
        }
    })});
}