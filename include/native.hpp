#include <string>
#include <memory>
#include "executor.hpp"

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

using RegisterNative = void(*)(Environment&, const std::string&, NativeFunc);
using PluginInit = void(*)(Environment&, RegisterNative);

static PluginInit loadPlugin(const std::string& path) {
#if defined(_WIN32)
    const HMODULE h = LoadLibraryA(path.c_str());
    if (!h) return nullptr;
    const auto init = reinterpret_cast<PluginInit>(
        GetProcAddress(h, "initPlugin")
    );
#else
    void* h = dlopen(path.c_str(), RTLD_NOW);
    if (!h) return nullptr;
    const auto init = reinterpret_cast<PluginInit>(
        dlsym(h, "initPlugin")
    );
#endif
    if (!init) return nullptr;
    return init;
}

static void regImpl(Environment& env, const std::string& name, NativeFunc f) {
    env.registerNative(name, f);
}

void linkNative(const std::string path, const std::shared_ptr<Environment> env) {
    const PluginInit init = loadPlugin(path);
    if (!init) return;
    init(*env, regImpl);
}
