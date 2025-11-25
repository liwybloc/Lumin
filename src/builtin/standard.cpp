#include "standard.hpp"

void implStandard(std::shared_ptr<Environment> globalEnv, Executor* executor) {

    globalEnv->set("nil", TypedValue());

}