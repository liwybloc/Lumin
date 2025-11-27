#include "netstream.hpp"

void addNetstream(std::shared_ptr<Environment> globalEnv, Executor* executor) {

    StructType fileType{"Response"};
    fileType.fields.push_back({"url", Primitive::STRING});
    fileType.fields.push_back({"res_size", Primitive::INT});
    fileType.fields.push_back({"is_open", Primitive::BOOL});

    auto sharedFT = std::make_shared<StructType>(fileType);
    globalEnv->setType("File", sharedFT);

    globalEnv->set("fetch", {std::make_shared<Function>(Function{
        [executor](const std::vector<std::shared_ptr<TypedValue>> &args, std::shared_ptr<ParsedASTNode> callNode) -> std::shared_ptr<TypedValue> {
            // Implement fetching data from a network source
            // Return the fetched data as a string
            return std::make_shared<TypedValue>("Fetched data");
        }
    })});

}