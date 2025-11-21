#include <executor.hpp>
#include <native.hpp>

#if defined(_WIN32)
__declspec(dllexport)
#endif
ReturnValue printFunc(std::shared_ptr<Environment> env, Executor* exec, std::unordered_map<std::string, TypedValue> args) {
    return ReturnValue();
}

void initPlugin(Environment& env, RegisterNative reg) {
    reg(env, "_print", [](std::shared_ptr<Environment> env, Executor* exec, std::unordered_map<std::string, TypedValue> args) -> ReturnValue {
        std::cout << args["content"].get<std::string>();
        return ReturnValue();
    });

}