#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <functional>
#include "lexer.hpp"
#include "parser.hpp"
#include "lumper.hpp"
#include "executor.hpp"

struct ParsedData {
    std::shared_ptr<ASTNode> ast;
    std::string lumpPath;
};

ParsedData parseAndLumpIfNeeded(const std::string& filename, const std::string& source, bool forceLump) {
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens, filename);
    auto ast = parser.parseProgram();

    #ifdef DEBUG
        std::ofstream debug("astdebug.txt");
        debug << astToString(ast);
        debug.close();
    #endif

    std::string lumpPath = filename.substr(0, filename.size() - 4) + ".lmp";
    if (forceLump) {
        Lumper lumper{ast};
        lumper.lump(lumpPath);
    }
    return {ast, lumpPath};
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " [options] <file>\n";
        return 1;
    }

    bool runLumper = false, exec = false;
    std::string expFileName;
    std::string filename;

    std::unordered_map<std::string, std::function<void(int&, char**)>> commands;

    commands["--lmp"] = [&](int& i, char**) {
        expFileName = ".lum";
        runLumper = true;
    };
    commands["--run"] = [&](int& i, char**) {
        expFileName = "";
        exec = true;
    };

    int i = 1;
    while (i < argc) {
        std::string arg = argv[i];
        auto it = commands.find(arg);

        if (it != commands.end()) {
            it->second(i, argv);
            i++;
            continue;
        }

        filename = arg;
        break;
    }

    if (filename.empty()) {
        std::cerr << "Missing input file.\n";
        return 1;
    }

    std::ifstream file(filename);
    if (!file) {
        std::cerr << "Path: " << filename << "\n";
        perror("Failed to open file");
        return 1;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    if (runLumper) {
        parseAndLumpIfNeeded(filename, source, true);
        return 0;
    } else if (exec) {
        std::string ext = filename.substr(filename.size() - 4);
        std::string lumpLoc;

        std::shared_ptr<ASTNode> ast;
        if (ext == ".lum") {
            auto parsed = parseAndLumpIfNeeded(filename, source, true);
            ast = parsed.ast;
            lumpLoc = parsed.lumpPath;
        } else if (ext == ".lmp") {
            lumpLoc = filename;
        } else {
            std::cerr << "Invalid file extension. Expected .lum or .lmp.\n";
            return 1;
        }

        Lumper lumper{ast};
        auto decoded = lumper.unlump(lumpLoc);
        if (!decoded) {
            std::cerr << "Failed to unlump file.\n";
            return 1;
        }

        return Executor(decoded).run().get<int>();
    }

    return 0;
}
