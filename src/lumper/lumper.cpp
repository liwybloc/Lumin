#include "lumper.hpp"
#include <sstream>
#include <fstream>
#include <cstdint>
#include <vector>
#include <stdexcept>
#include <zstd.h>
#include <cstring>

constexpr char LUMP_MAGIC[4] = {'L','U','M','P'};
constexpr uint8_t LUMP_VERSION = 5;
constexpr uint64_t MAX_DSIZE = 1ULL << 30; 
constexpr uint64_t MAX_CSIZE = 1ULL << 30; 
constexpr uint32_t MAX_AST_DEPTH = 2000;
constexpr uint32_t MAX_STRING_LEN = 100 * 1024 * 1024; 
constexpr uint8_t TYPE_MAX_VALUE = 31; 
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

static uint8_t readByte(std::istream &in) {
    int c = in.get();
    if (c == EOF) throw std::runtime_error("Unexpected EOF");
    return uint8_t(c);
}

static uint64_t readVarint64(std::istream &in) {
    uint64_t r = 0;
    int sh = 0;
    while (true) {
        uint8_t b = readByte(in);
        if (sh == 63) {
            if ((b & 0xFE) != 0) throw std::runtime_error("Varint64 overflow");
        } else if (sh > 63) throw std::runtime_error("Varint64 too long");
        r |= uint64_t(b & 0x7F) << sh;
        if (!(b & 0x80)) break;
        sh += 7;
    }
    return r;
}

static uint32_t readVarint(std::istream &in) {
    uint32_t r = 0;
    int sh = 0;
    while (true) {
        uint8_t b = readByte(in);
        if (sh == 28) {
            if ((b & 0xF0) != 0) throw std::runtime_error("Varint32 overflow");
        } else if (sh > 28) throw std::runtime_error("Varint32 too long");
        r |= uint32_t(b & 0x7F) << sh;
        if (!(b & 0x80)) break;
        sh += 7;
    }
    return r;
}

static void readExact(std::istream &in, char *buf, std::size_t n) {
    in.read(buf, n);
    if (!in || static_cast<std::size_t>(in.gcount()) != n) throw std::runtime_error("Unexpected EOF reading data");
}

static void writeString(std::ostream &out, const std::string &s) {
    if (s.size() > MAX_STRING_LEN) throw std::runtime_error("String too large");
    writeVarint(out, static_cast<uint32_t>(s.size()));
    out.write(s.data(), s.size());
    if (!out) throw std::runtime_error("Write error");
}

static std::string readString(std::istream &in) {
    uint32_t len = readVarint(in);
    if (len > MAX_STRING_LEN) throw std::runtime_error("String length unreasonable/too large");
    std::string s(len, '\0');
    readExact(in, s.data(), len);
    return s;
}

static void encodeNode(const std::shared_ptr<ASTNode> &node, std::ostream &out) {
    if (!node) throw std::runtime_error("Null AST node");
    uint32_t childCount = static_cast<uint32_t>(node->children.size());
    uint8_t tval = uint8_t(node->type);
    if (tval > TYPE_MAX_VALUE) throw std::runtime_error("ASTNode::Type out of range");
    uint8_t header = uint8_t(tval << 3);
    if (childCount < 7) header |= uint8_t(childCount);
    else header |= 0b111;
    writeByte(out, header);

    switch (node->type) {
        case ASTNode::Type::BINARY_OP:
        case ASTNode::Type::UNARY_OP:
            writeByte(out, uint8_t(node->binopValue));
            break;
        case ASTNode::Type::NUMBER:
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
            writeString(out, node->retType);
        case ASTNode::Type::NUMBER:
        case ASTNode::Type::STRING:
        case ASTNode::Type::IDENTIFIER:
        case ASTNode::Type::PRIMITIVE_ASSIGNMENT:
        case ASTNode::Type::ARRAY_ASSIGN:
        case ASTNode::Type::NDARRAY_ASSIGN:
        case ASTNode::Type::STRUCT_DECLARE:
        case ASTNode::Type::STRUCT_ASSIGNMENT:
        case ASTNode::Type::PRAGMA:
        case ASTNode::Type::BOOL:
        case ASTNode::Type::FOR_STATEMENT:
            writeString(out, node->strValue);
            break;
        default:
            break;
    }

    if (childCount >= 7) writeVarint(out, childCount);
    for (const auto &c : node->children) encodeNode(c, out);
}

static std::shared_ptr<ASTNode> decodeNode(std::istream &in, uint32_t depth) {
    if (depth > MAX_AST_DEPTH) throw std::runtime_error("AST depth exceeded safe limit");
    auto n = std::make_shared<ASTNode>();
    uint8_t header = readByte(in);
    uint8_t tval = header >> 3;
    if (tval > TYPE_MAX_VALUE) throw std::runtime_error("Invalid node type");
    n->type = ASTNode::Type(tval);
    uint8_t small = header & 0b111;

    switch (n->type) {
        case ASTNode::Type::BINARY_OP:
        case ASTNode::Type::UNARY_OP:
            n->binopValue = BinaryOp(readByte(in));
            break;
        case ASTNode::Type::NUMBER:
        case ASTNode::Type::BOOL:
        case ASTNode::Type::STRING:
        case ASTNode::Type::SIZED_ARRAY_DECLARE:
        case ASTNode::Type::FUNCTION:
        case ASTNode::Type::PRIMITIVE_ASSIGNMENT:
            n->primitiveValue = Primitive(readByte(in));
            break;
        default:
            break;
    }

    switch (n->type) {
        case ASTNode::Type::FUNCTION:
            n->retType = readString(in);
        case ASTNode::Type::NUMBER:
        case ASTNode::Type::STRING:
        case ASTNode::Type::IDENTIFIER:
        case ASTNode::Type::PRIMITIVE_ASSIGNMENT:
        case ASTNode::Type::ARRAY_ASSIGN:
        case ASTNode::Type::NDARRAY_ASSIGN:
        case ASTNode::Type::STRUCT_DECLARE:
        case ASTNode::Type::STRUCT_ASSIGNMENT:
        case ASTNode::Type::PRAGMA:
        case ASTNode::Type::BOOL:
        case ASTNode::Type::FOR_STATEMENT:
            n->strValue = readString(in);
            break;
        default:
            break;
    }

    uint32_t cc = (small < 7) ? small : readVarint(in);
    if (cc > 10000000) throw std::runtime_error("Child count unreasonable");
    n->children.reserve(cc);
    for (uint32_t i = 0; i < cc; ++i) n->children.push_back(decodeNode(in, depth + 1));
    return n;
}

Lumper::Lumper(const std::shared_ptr<ASTNode> &ast) : ast(ast) {}

void Lumper::lump(const std::string &loc) {
    if (!ast) throw std::runtime_error("Cannot lump a null AST root node");

    std::ostringstream uncompressed;

    writeVarint(uncompressed, static_cast<uint32_t>(ast->children.size()));
    for (const auto &c : ast->children) encodeNode(c, uncompressed);

    const std::string inData = uncompressed.str();
    const size_t inSize = inData.size();
    if (inSize > MAX_DSIZE) throw std::runtime_error("Uncompressed data too large");

    const size_t maxCompressed = ZSTD_compressBound(inSize);
    std::vector<char> outBuf(maxCompressed);
    const size_t csize = ZSTD_compress(outBuf.data(), maxCompressed, inData.data(), inSize, 3);

    if (ZSTD_isError(csize)) throw std::runtime_error(std::string("ZSTD compression failed: ") + ZSTD_getErrorName(csize));
    if (csize == 0 || csize > MAX_CSIZE) throw std::runtime_error("Compressed size unreasonable");

    std::ofstream out(loc, std::ios::binary);
    out.write(LUMP_MAGIC, 4);
    writeByte(out, LUMP_VERSION);
    writeVarint(out, inSize);
    writeVarint(out, csize);
    out.write(outBuf.data(), csize);
}

std::shared_ptr<ASTNode> Lumper::unlump(const std::string &loc) {
    std::ifstream in(loc, std::ios::binary);
    if (!in) return nullptr;

    char m[4];
    readExact(in, m, 4);
    if (std::memcmp(m, LUMP_MAGIC, 4) != 0) throw std::runtime_error("Invalid LUMP magic");

    uint8_t version = readByte(in);
    if (version != LUMP_VERSION) throw std::runtime_error("Unsupported LUMP version");

    uint64_t dsize = readVarint64(in);
    uint64_t csize = readVarint64(in);
    if (dsize == 0 || dsize > MAX_DSIZE) throw std::runtime_error("Invalid decompressed size");
    if (csize == 0 || csize > MAX_CSIZE) throw std::runtime_error("Invalid compressed size");

    std::vector<char> cbuf(static_cast<size_t>(csize));
    readExact(in, cbuf.data(), static_cast<size_t>(csize));
    std::vector<char> dbuf(static_cast<size_t>(dsize));

    size_t ds = ZSTD_decompress(dbuf.data(), dsize, cbuf.data(), csize);
    if (ZSTD_isError(ds)) throw std::runtime_error(std::string("ZSTD decompression failed: ") + ZSTD_getErrorName(ds));
    if (ds != dsize) throw std::runtime_error("Decompressed size mismatch");

    std::istringstream iss(std::string(dbuf.data(), ds));
    auto root = std::make_shared<ASTNode>();
    root->type = ASTNode::Type::PROGRAM;

    uint32_t cc = readVarint(iss);
    if (cc > 10000000) throw std::runtime_error("Top-level child count unreasonable");

    root->children.reserve(cc);
    for (uint32_t i = 0; i < cc; ++i) root->children.push_back(decodeNode(iss, 0));
    return root;
}
