#include "lumper.hpp"
#include <sstream>
#include <fstream>
#include <cstdint>
#include <cstring>
#include <vector>
#include <map>
#include <stdexcept>
#include <algorithm>
#include <functional>
#include <zconf.h>
#include <zlib.h>

constexpr char LUMP_MAGIC[4] = {'L','U','M','P'};
constexpr uint8_t LUMP_VERSION = 4;

static inline void writeByte(std::ostream &out, uint8_t v) {
    out.put(char(v));
}

static void writeVarint(std::ostream &out, uint32_t v) {
    while (v > 0x7F) {
        out.put(char((v & 0x7F) | 0x80));
        v >>= 7;
    }
    out.put(char(v));
}

static uint8_t readByte(std::istream &in) {
    int c = in.get();
    if (c == EOF) throw std::runtime_error("Unexpected EOF");
    return uint8_t(c);
}

static uint32_t readVarint(std::istream &in) {
    uint32_t r = 0;
    int sh = 0;
    while (true) {
        uint8_t b = readByte(in);
        if (sh >= 28 && (b & 0x7F) != 0) {
            throw std::runtime_error("Varint overflow");
        }
        r |= uint32_t(b & 0x7F) << sh;
        if (!(b & 0x80)) break;
        sh += 7;
    }
    return r;
}

static std::string readString(std::istream &in) {
    uint32_t len = readVarint(in);
    if (len == 0) return "";
    std::string s(len, '\0');
    in.read(s.data(), len);
    if (in.gcount() != len) throw std::runtime_error("Unexpected EOF reading string data");
    return s;
}

struct LumpContext {
    std::map<std::string, uint32_t> stringMap;
    std::vector<std::string> strings;
    std::vector<std::string> stringTable;

    LumpContext() {
        strings.emplace_back(""); 
        stringMap[""] = 0;
    }

    uint32_t getIndex(const std::string &s) {
        auto it = stringMap.find(s);
        if (it != stringMap.end()) {
            return it->second;
        }
        uint32_t index = strings.size();
        strings.push_back(s);
        stringMap[s] = index;
        return index;
    }
    
    void writeTable(std::ostream &out) const {
        writeVarint(out, strings.size()); 
        for (const auto &s : strings) {
            writeVarint(out, s.size());
            out.write(s.data(), s.size());
        }
    }

    void readTable(std::istream &in) {
        uint32_t count = readVarint(in);
        stringTable.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            stringTable.push_back(readString(in));
        }
    }

    const std::string& getString(uint32_t index) const {
        if (index >= stringTable.size()) {
            throw std::runtime_error("Invalid string table index");
        }
        return stringTable[index];
    }
};

// Header Layout (8 bits):
// Bits 7-3 : node type (5 bits)
// Bits 2-1 : children count flag (2 bits)
// Bit    0 : data byte flag (1 bit)

static void encodeNode(const std::shared_ptr<ASTNode> &node, std::ostream &out, LumpContext &ctx) {
    uint8_t header = uint8_t(node->type) << 3;

    uint32_t childCount = node->children.size();
    if (childCount == 0) header |= 0b00 << 1;
    else if (childCount == 1) header |= 0b01 << 1;
    else if (childCount == 2) header |= 0b10 << 1;
    else header |= 0b11 << 1;

    bool hasDataByte = false;
    uint8_t dataByte = 0;

    switch (node->type) {
        case ASTNode::Type::BINARY_OP:
        case ASTNode::Type::UNARY_OP:
            hasDataByte = true;
            dataByte = uint8_t(node->binopValue);
            break;
        case ASTNode::Type::NUMBER:
        case ASTNode::Type::BOOL:
        case ASTNode::Type::STRING:
        case ASTNode::Type::SIZED_ARRAY_DECLARE:
            hasDataByte = true;
            dataByte = uint8_t(node->primitiveValue);
            break;
        default:
            break;
    }

    if (hasDataByte) header |= 0b1;

    writeByte(out, header);

    if (hasDataByte) writeByte(out, dataByte);

    switch (node->type) {
        case ASTNode::Type::FUNCTION:
            writeVarint(out, ctx.getIndex(node->retType));
            writeVarint(out, ctx.getIndex(node->strValue));
            break;
        case ASTNode::Type::NUMBER:
        case ASTNode::Type::STRING:
        case ASTNode::Type::IDENTIFIER:
        case ASTNode::Type::PRIMITIVE_ASSIGNMENT:
        case ASTNode::Type::ARRAY_ASSIGN:
        case ASTNode::Type::NDARRAY_ASSIGN:
        case ASTNode::Type::STRUCT_DECLARE:
        case ASTNode::Type::STRUCT_ASSIGNMENT:
        case ASTNode::Type::IMPORT:
            writeVarint(out, ctx.getIndex(node->strValue));
            break;
        default:
            break;
    }

    if (childCount >= 3) writeVarint(out, childCount);

    for (const auto &c : node->children) encodeNode(c, out, ctx);
}

static std::shared_ptr<ASTNode> decodeNode(std::istream &in, const LumpContext &ctx) {
    auto n = std::make_shared<ASTNode>();

    uint8_t header = readByte(in);

    n->type = ASTNode::Type(header >> 3);        // Bits 7-3: type
    uint8_t countFlag = (header >> 1) & 0b11;    // Bits 2-1: children count
    bool hasDataByte = header & 0b1;             // Bit    0: data byte flag

    if (hasDataByte) {
        uint8_t dataByte = readByte(in);
        switch (n->type) {
            case ASTNode::Type::BINARY_OP:
            case ASTNode::Type::UNARY_OP:
                n->binopValue = BinaryOp(dataByte);
                break;
            case ASTNode::Type::NUMBER:
            case ASTNode::Type::BOOL:
            case ASTNode::Type::STRING:
            case ASTNode::Type::SIZED_ARRAY_DECLARE:
                n->primitiveValue = Primitive(dataByte);
                break;
            default:
                break;
        }
    }

    switch (n->type) {
        case ASTNode::Type::FUNCTION:
            n->retType = ctx.getString(readVarint(in));
            [[fallthrough]];
        case ASTNode::Type::NUMBER:
        case ASTNode::Type::STRING:
        case ASTNode::Type::IDENTIFIER:
        case ASTNode::Type::PRIMITIVE_ASSIGNMENT:
        case ASTNode::Type::ARRAY_ASSIGN:
        case ASTNode::Type::NDARRAY_ASSIGN:
        case ASTNode::Type::STRUCT_DECLARE:
        case ASTNode::Type::STRUCT_ASSIGNMENT:
        case ASTNode::Type::IMPORT:
            n->strValue = ctx.getString(readVarint(in));
            break;
        default:
            break;
    }

    uint32_t cc;
    if (countFlag == 0b00) cc = 0;
    else if (countFlag == 0b01) cc = 1;
    else if (countFlag == 0b10) cc = 2;
    else cc = readVarint(in);

    n->children.reserve(cc);
    for (uint32_t i = 0; i < cc; ++i) n->children.push_back(decodeNode(in, ctx));

    return n;
}

Lumper::Lumper(const std::shared_ptr<ASTNode> &ast) : ast(ast) {}

void Lumper::lump(std::string &loc) {
    std::ostringstream uncompressed;

    LumpContext ctx;
    std::function<void(const std::shared_ptr<ASTNode>&)> collectStrings;
    collectStrings = [&](const std::shared_ptr<ASTNode> &node) {
        switch (node->type) {
            case ASTNode::Type::FUNCTION:
                ctx.getIndex(node->retType);
                [[fallthrough]];
            case ASTNode::Type::NUMBER:
            case ASTNode::Type::STRING:
            case ASTNode::Type::IDENTIFIER:
            case ASTNode::Type::PRIMITIVE_ASSIGNMENT:
            case ASTNode::Type::ARRAY_ASSIGN:
            case ASTNode::Type::NDARRAY_ASSIGN:
            case ASTNode::Type::STRUCT_ASSIGNMENT:
            case ASTNode::Type::STRUCT_DECLARE:
            case ASTNode::Type::SIZED_ARRAY_DECLARE:
            case ASTNode::Type::IMPORT:
                ctx.getIndex(node->strValue);
                break;
            default: break;
        }
        for (const auto &c : node->children) collectStrings(c);
    };

    collectStrings(ast);
    ctx.writeTable(uncompressed);

    writeVarint(uncompressed, ast->children.size());
    for (const auto &c : ast->children) encodeNode(c, uncompressed, ctx);

    std::string inData = uncompressed.str();
    uLongf compressedSize = compressBound(inData.size());
    std::vector<unsigned char> outBuffer(compressedSize);

    if (compress(outBuffer.data(), &compressedSize,
                 reinterpret_cast<const Bytef*>(inData.data()),
                 inData.size()) != Z_OK) {
        throw std::runtime_error("Compression failed");
    }

    std::ofstream out(loc, std::ios::binary);
    if (!out) throw std::runtime_error("Cannot open file for writing");

    out.write(LUMP_MAGIC, 4);
    writeByte(out, LUMP_VERSION);

    uint64_t origSize = inData.size();
    out.write(reinterpret_cast<char*>(&origSize), sizeof(origSize));

    out.write(reinterpret_cast<char*>(outBuffer.data()), compressedSize);
}

std::shared_ptr<ASTNode> Lumper::unlump(const std::string &loc) {
    std::ifstream in(loc, std::ios::binary);
    if (!in) return nullptr;

    char m[4];
    in.read(m, 4);
    if (std::memcmp(m, LUMP_MAGIC, 4) != 0)
        throw std::runtime_error("Invalid LUMP file magic");

    uint8_t version = readByte(in);
    if (version != LUMP_VERSION)
        throw std::runtime_error("Unsupported LUMP version");

    unsigned long decompressedSize;
    in.read(reinterpret_cast<char*>(&decompressedSize), sizeof(decompressedSize));

    in.seekg(0, std::ios::end);
    size_t fileSize = in.tellg();
    size_t compressedSize = fileSize - (4 + 1 + sizeof(decompressedSize));
    in.seekg(4 + 1 + sizeof(decompressedSize), std::ios::beg);

    std::vector<unsigned char> compressed(compressedSize);
    in.read(reinterpret_cast<char*>(compressed.data()), compressedSize);

    std::vector<unsigned char> decompressed(decompressedSize);
    if (uncompress(decompressed.data(), &decompressedSize,
                   compressed.data(), compressedSize) != Z_OK) {
        throw std::runtime_error("Decompression failed");
    }

    std::istringstream iss(std::string(reinterpret_cast<char*>(decompressed.data()), decompressedSize));

    LumpContext ctx;
    ctx.readTable(iss);

    auto root = std::make_shared<ASTNode>();
    root->type = ASTNode::Type::PROGRAM;

    uint32_t cc = readVarint(iss);
    root->children.reserve(cc);
    for (uint32_t i = 0; i < cc; ++i)
        root->children.push_back(decodeNode(iss, ctx));

    return root;
}