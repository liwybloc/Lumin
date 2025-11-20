#include "filestream.hpp"
#include "executor.hpp"
#include "executils.hpp"
#include <fstream>
#include <memory>
#include <string>

void addFilestream(std::shared_ptr<Environment> globalEnv, Executor* executor) {
    StructType fileType{"File"};
    fileType.fields.push_back({"filename", Primitive::STRING});
    fileType.fields.push_back({"size", Primitive::INT});
    fileType.fields.push_back({"is_open", Primitive::BOOL});

    auto sharedFT = std::make_shared<StructType>(fileType);
    globalEnv->setType("File", sharedFT);

    globalEnv->set("fopen", std::make_shared<Function>(Function{
        [executor, sharedFT](const std::vector<std::shared_ptr<TypedValue>> &args) -> std::shared_ptr<TypedValue> {
            if (args.size() < 2) throw std::runtime_error("fopen requires filename and mode");

            std::string filename = executor->getStringValue(*args[0]);
            std::string mode = executor->getStringValue(*args[1]);

            std::ios_base::openmode openMode = std::ios::binary;
            if (mode == "r") openMode = std::ios::in | std::ios::binary;
            else if (mode == "w") openMode = std::ios::out | std::ios::binary | std::ios::trunc;
            else if (mode == "a") openMode = std::ios::out | std::ios::binary | std::ios::app;
            else throw std::runtime_error("Invalid file mode: " + mode);

            auto file = std::make_shared<std::fstream>(filename, openMode);
            if (!file->is_open()) throw std::runtime_error("Failed to open file: " + filename);

            auto fileStruct = std::make_shared<Struct>("File", sharedFT);
            fileStruct->addField("filename", filename);
            fileStruct->addField("size", 0);
            fileStruct->addField("is_open", true);

            fileStruct->addHiddenField("stream", file);

            return std::make_shared<TypedValue>(fileStruct);
        }
    }));

    globalEnv->set("fclose", std::make_shared<Function>(Function{
        [](const std::vector<std::shared_ptr<TypedValue>> &args) -> std::shared_ptr<TypedValue> {
            if (args.empty()) throw std::runtime_error("fclose requires a File struct");
            auto file = args[0]->get<std::shared_ptr<Struct>>();

            auto stream = std::any_cast<std::shared_ptr<std::fstream>>(file->getHiddenField("stream"));
            if (stream && stream->is_open()) stream->close();

            file->setField("is_open", false);
            return std::make_shared<TypedValue>(0);
        }
    }));

    globalEnv->set("fwrite", std::make_shared<Function>(Function{
        [executor](const std::vector<std::shared_ptr<TypedValue>> &args) -> std::shared_ptr<TypedValue> {
            if (args.size() < 2) throw std::runtime_error("fwrite requires a File struct and string");
            auto file = args[0]->get<std::shared_ptr<Struct>>();
            auto stream = std::any_cast<std::shared_ptr<std::fstream>>(file->getHiddenField("stream"));
            if (!stream || !stream->is_open())
                throw std::runtime_error("File is not open");

            std::string data = executor->getStringValue(*args[1]);
            (*stream) << data;
            stream->flush();

            stream->seekp(0, std::ios::end);
            file->setField("size", static_cast<int>(stream->tellp()));

            return std::make_shared<TypedValue>(0);
        }
    }));

    globalEnv->set("fread", std::make_shared<Function>(Function{
        [sharedFT](const std::vector<std::shared_ptr<TypedValue>> &args) -> std::shared_ptr<TypedValue> {
            if (args.empty()) throw std::runtime_error("fread requires a File struct");

            if(!sharedFT->match(args[0]->type)) throw std::runtime_error("fread expects a File struct");

            auto file = args[0]->get<std::shared_ptr<Struct>>();
            auto stream = std::any_cast<std::shared_ptr<std::fstream>>(file->getHiddenField("stream"));
            if (!stream || !stream->is_open())
                throw std::runtime_error("File is not open");

            stream->seekg(0, std::ios::beg);
            if (args.size() >= 2) {
                if(!args[1]->type.match(Primitive::INT)) throw std::runtime_error("fread expects a int");
                int n = args[1]->get<int>();
                std::string buf(n, '\0');
                stream->read(buf.data(), n);
                buf.resize(stream->gcount()); 
                return std::make_shared<TypedValue>(buf);
            } else {
                std::string content((std::istreambuf_iterator<char>(*stream)), std::istreambuf_iterator<char>());
                return std::make_shared<TypedValue>(content);
            }
        }
    }));
}
