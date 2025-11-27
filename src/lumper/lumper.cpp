#include "lumper.hpp"
#include <fstream>
#include <cstdint>
#include <vector>
#include <stdexcept>
#include <zstd.h>
#include <cstring>
#include <sstream>

constexpr char LUMP_MAGIC[4] = {'L','U','M','P'};
constexpr uint8_t LUMP_VERSION = 5;
constexpr uint64_t MAX_DSIZE = 1ULL << 30;
constexpr uint64_t MAX_CSIZE = 1ULL << 30;
constexpr uint32_t MAX_AST_DEPTH = 2000;
constexpr uint32_t MAX_STRING_LEN = 100 * 1024 * 1024;
constexpr uint8_t TYPE_MAX_VALUE = 31;

struct MemReader {
    const char* ptr;
    const char* end;

    MemReader(const char* data, size_t size) : ptr(data), end(data + size) {}

    uint8_t readByte() {
        if (ptr >= end) throw std::runtime_error("Unexpected EOF");
        return uint8_t(*ptr++);
    }

    uint32_t readVarint32() {
        uint32_t r = 0;
        int sh = 0;
        while (true) {
            uint8_t b = readByte();
            if (sh == 28 && (b & 0xF0) != 0) throw std::runtime_error("Varint32 overflow");
            r |= uint32_t(b & 0x7F) << sh;
            if (!(b & 0x80)) break;
            sh += 7;
        }
        return r;
    }

    uint64_t readVarint64() {
        uint64_t r = 0;
        int sh = 0;
        while (true) {
            uint8_t b = readByte();
            if (sh == 63 && (b & 0xFE) != 0) throw std::runtime_error("Varint64 overflow");
            r |= uint64_t(b & 0x7F) << sh;
            if (!(b & 0x80)) break;
            sh += 7;
        }
        return r;
    }

    void readExact(char* buf, size_t n) {
        if (ptr + n > end) throw std::runtime_error("Unexpected EOF reading data");
        std::memcpy(buf, ptr, n);
        ptr += n;
    }

    std::string readString(bool singleSize) {
        uint32_t len = singleSize ? 1 : readVarint32();
        if (len > MAX_STRING_LEN) throw std::runtime_error("String too large");
        std::string s(len, '\0');
        readExact(s.data(), len);
        return s;
    }
};

static bool shouldDetectLineNumber(ASTNode::Type node, ASTNode::Type parent) {
    if (node == ASTNode::Type::IMPORT_BLOCK || node == ASTNode::Type::BLOCK) return false;
    switch (parent) {
        case ASTNode::Type::PRAGMA:
        case ASTNode::Type::BLOCK:
        case ASTNode::Type::IF_STATEMENT:
        case ASTNode::Type::WHILE_STATEMENT:
        case ASTNode::Type::FOR_STATEMENT:
            return true;
        default:
            return false;
    }
}

static inline void writeByte(std::ostream &out, uint8_t v) {
    out.put(char(v));
    if (!out) throw std::runtime_error("Write error");
}

static void writeVarint(std::ostream &out, uint64_t v) {
    while (v > 0x7F) {
        out.put(char((v & 0x7F) | 0x80));
        v >>= 7;
    }
    out.put(char(v));
    if (!out) throw std::runtime_error("Write error");
}

static void writeString(std::ostream &out, const std::string &s, bool forceSize = false) {
    if (s.size() > MAX_STRING_LEN) throw std::runtime_error("String too large");
    if(s.size() != 1 || forceSize) writeVarint(out, static_cast<uint32_t>(s.size()));
    out.write(s.data(), s.size());
    if (!out) throw std::runtime_error("Write error");
}

static int parseInt(const char* data, size_t len) {
    int res = 0;
    for (size_t i = 0; i < len; ++i) {
        res = res * 10 + (data[i] - '0');
    }
    return res;
}

static void encodeNode(const std::shared_ptr<ASTNode> &node, std::ostream &out, std::shared_ptr<ASTNode> parent) {
    if (!node) throw std::runtime_error("Null AST node");
    uint32_t childCount = static_cast<uint32_t>(node->children.size());
    uint8_t tval = uint8_t(node->type);
    if (tval > TYPE_MAX_VALUE) throw std::runtime_error("ASTNode::Type out of range");

    uint32_t offset = 0;
    bool shouldWrite = parent && shouldDetectLineNumber(node->type, parent->type);
    if(shouldWrite) offset = 1;
    uint32_t actualChildCount = childCount - offset;

    uint8_t header = uint8_t(tval << 3);
    if(node->strValue.length() == 1) header |= 0b100;
    if (actualChildCount < 3) header |= uint8_t(actualChildCount);
    else header |= 0b11;
    writeByte(out, header);

    switch (node->type) {
        case ASTNode::Type::BINARY_OP:
        case ASTNode::Type::UNARY_OP:
            writeByte(out, uint8_t(node->binopValue));
            break;
        case ASTNode::Type::INTEGER:
        case ASTNode::Type::BOOL:
        case ASTNode::Type::STRING:
        case ASTNode::Type::SIZED_ARRAY_DECLARE:
        case ASTNode::Type::FUNCTION:
        case ASTNode::Type::PRIMITIVE_ASSIGNMENT:
            writeByte(out, uint8_t(node->primitiveValue));
            break;
        default:
            break;
    }

    switch (node->type) {
        case ASTNode::Type::FUNCTION:
            writeString(out, node->retType, true);
        case ASTNode::Type::INTEGER:
        case ASTNode::Type::STRING:
        case ASTNode::Type::IDENTIFIER:
        case ASTNode::Type::PRIMITIVE_ASSIGNMENT:
        case ASTNode::Type::ARRAY_ASSIGN:
        case ASTNode::Type::NDARRAY_ASSIGN:
        case ASTNode::Type::STRUCT_DECLARE:
        case ASTNode::Type::NEW_STRUCT:
        case ASTNode::Type::PRAGMA:
        case ASTNode::Type::BOOL:
        case ASTNode::Type::FOR_STATEMENT:
        case ASTNode::Type::CHAR:
            writeString(out, node->strValue);
            break;
        default:
            break;
    }

    if (shouldWrite) {
        auto child = node->children[0];
        if(child->type != ASTNode::Type::INTEGER)
            throw std::runtime_error("Expected integer node as first child when writing line number");
        auto val = child->strValue;
        writeVarint(out, parseInt(val.c_str(), val.size()));
    }

    if (actualChildCount >= 3) writeVarint(out, actualChildCount);
    for (uint32_t i = offset; i < childCount; i++) encodeNode(node->children[i], out, node);
}

static std::shared_ptr<ParsedASTNode> decodeNode(MemReader &r, uint32_t depth, int lkln, std::string pragma, std::shared_ptr<ParsedASTNode> parent) {
    if (depth > MAX_AST_DEPTH) throw std::runtime_error("AST depth exceeded safe limit");
    auto n = std::make_shared<ParsedASTNode>();

    uint8_t header = r.readByte();
    uint8_t tval = header >> 3;
    if (tval > TYPE_MAX_VALUE) throw std::runtime_error("Invalid node type");
    n->type = ASTNode::Type(tval);

    bool isOneChar = (header & 0b100) != 0;
    uint8_t childCount = header & 0b11;

    switch (n->type) {
        case ASTNode::Type::BINARY_OP:
        case ASTNode::Type::UNARY_OP:
            n->binopValue = BinaryOp(r.readByte());
            break;
        case ASTNode::Type::INTEGER:
        case ASTNode::Type::BOOL:
        case ASTNode::Type::STRING:
        case ASTNode::Type::SIZED_ARRAY_DECLARE:
        case ASTNode::Type::FUNCTION:
        case ASTNode::Type::PRIMITIVE_ASSIGNMENT:
            n->primitiveValue = Primitive(r.readByte());
            break;
        default: break;
    }

    switch (n->type) {
        case ASTNode::Type::INTEGER:
            n->strValue = r.readString(isOneChar);
            n->intValue = parseInt(n->strValue.c_str(), n->strValue.size());
            break;
        case ASTNode::Type::BOOL:
            n->strValue = r.readString(isOneChar);
            n->boolValue = (n->strValue == "1");
            break;
        case ASTNode::Type::FUNCTION:
            n->retType = r.readString(false);
        case ASTNode::Type::STRING:
        case ASTNode::Type::IDENTIFIER:
        case ASTNode::Type::PRIMITIVE_ASSIGNMENT:
        case ASTNode::Type::ARRAY_ASSIGN:
        case ASTNode::Type::NDARRAY_ASSIGN:
        case ASTNode::Type::STRUCT_DECLARE:
        case ASTNode::Type::NEW_STRUCT:
        case ASTNode::Type::FOR_STATEMENT:
        case ASTNode::Type::CHAR:
            n->strValue = r.readString(isOneChar);
            break;
        case ASTNode::Type::PRAGMA:
            n->strValue = r.readString(false);
            pragma = n->strValue;
            break;
        default: break;
    }

    bool shouldRead = parent && shouldDetectLineNumber(n->type, parent->type);
    if (shouldRead) {
        lkln = r.readVarint32();
    }
    n->lineNumber = lkln;

    uint32_t cc = (childCount < 3) ? childCount : r.readVarint32();
    n->children.reserve(cc);
    for (uint32_t i = 0; i < cc; ++i)
        n->children.push_back(decodeNode(r, depth + 1, lkln, pragma, n));

    n->fileName = pragma;
    return n;
}

Lumper::Lumper(const std::shared_ptr<ASTNode> &ast) : ast(ast) {}

void Lumper::lump(const std::string &loc) {
    if (!ast) throw std::runtime_error("Cannot lump a null AST root node");

    std::ostringstream uncompressed;
    writeVarint(uncompressed, static_cast<uint32_t>(ast->children.size()));
    for (const auto &c : ast->children) encodeNode(c, uncompressed, nullptr);

    const std::string inData = uncompressed.str();
    const size_t inSize = inData.size();
    if (inSize > MAX_DSIZE) throw std::runtime_error("Uncompressed data too large");

    const size_t maxCompressed = ZSTD_compressBound(inSize);
    std::vector<char> outBuf(maxCompressed);
    const size_t csize = ZSTD_compress(outBuf.data(), maxCompressed, inData.data(), inSize, 25);

    if (ZSTD_isError(csize)) throw std::runtime_error(std::string("ZSTD compression failed: ") + ZSTD_getErrorName(csize));
    if (csize == 0 || csize > MAX_CSIZE) throw std::runtime_error("Compressed size unreasonable");

    std::ofstream out(loc, std::ios::binary);
    out.write(LUMP_MAGIC, 4);
    writeByte(out, LUMP_VERSION);
    writeVarint(out, inSize);
    writeVarint(out, csize);
    out.write(outBuf.data(), csize);
}

std::shared_ptr<ParsedASTNode> Lumper::unlump(const std::string &loc) {
    std::ifstream in(loc, std::ios::binary | std::ios::ate);
    if (!in) return nullptr;
    const size_t fileSize = in.tellg();
    in.seekg(0);

    if (fileSize < 9) throw std::runtime_error("File too small");

    std::vector<char> fileData(fileSize);
    in.read(fileData.data(), fileSize);
    if (!in) throw std::runtime_error("Failed to read file");

    if (std::memcmp(fileData.data(), LUMP_MAGIC, 4) != 0) throw std::runtime_error("Invalid LUMP magic");
    if (uint8_t(fileData[4]) != LUMP_VERSION) throw std::runtime_error("Unsupported LUMP version");

    MemReader r(fileData.data() + 5, fileSize - 5);
    uint64_t dsize = r.readVarint64();
    uint64_t csize = r.readVarint64();
    if (dsize == 0 || dsize > MAX_DSIZE) throw std::runtime_error("Invalid decompressed size");
    if (csize == 0 || csize > MAX_CSIZE) throw std::runtime_error("Invalid compressed size");

    std::vector<char> cbuf(static_cast<size_t>(csize));
    r.readExact(cbuf.data(), static_cast<size_t>(csize));
    std::vector<char> dbuf(static_cast<size_t>(dsize));

    size_t ds = ZSTD_decompress(dbuf.data(), dsize, cbuf.data(), csize);
    if (ZSTD_isError(ds)) throw std::runtime_error(std::string("ZSTD decompression failed: ") + ZSTD_getErrorName(ds));
    if (ds != dsize) throw std::runtime_error("Decompressed size mismatch");

    MemReader reader(dbuf.data(), ds);
    auto root = std::make_shared<ParsedASTNode>();
    root->type = ASTNode::Type::PROGRAM;

    uint32_t cc = reader.readVarint32();
    if (cc > 10000000) throw std::runtime_error("Top-level child count unreasonable");
    root->children.reserve(cc);
    for (uint32_t i = 0; i < cc; ++i)
        root->children.push_back(decodeNode(reader, 0, 1, "ROOT", nullptr));

    return root;
}
