#ifndef EXECUTOR_HPP
#define EXECUTOR_HPP

#include "parser.hpp"
#include "executils.hpp"
#include <stack>
#include <unordered_map>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <stdexcept>
#include <functional>
#include <optional>
#include <vector>
#include <any>
#include <algorithm>
#include <cstddef>

enum class BaseType { Int, Bool, String, Array, Function, Struct, ExportData, NIL };

struct StructType;
struct Struct;
struct Function;
struct ExportData;
struct TypedValue;
struct Type;
struct Array;

struct Type {
    BaseType kind;
    std::string customName;
    std::shared_ptr<Type> elementType;

    Type() : kind(BaseType::NIL) {}
    Type(BaseType base) : kind(base) {}
    Type(const std::string &name) : kind(BaseType::Struct), customName(name) {}
    Type(Primitive prim) {
        switch (prim) {
            case Primitive::INT: kind = BaseType::Int; break;
            case Primitive::BOOL: kind = BaseType::Bool; break;
            case Primitive::STRING: kind = BaseType::String; break;
            default: throw std::runtime_error("Invalid primitive type - " + std::to_string(static_cast<int>(prim)));
        }
    }

    bool match(BaseType base) const {
        return kind == base;
    }

    bool match(const Type &other) const {
        if (kind != other.kind) return false;
        if (kind == BaseType::Struct) return customName == other.customName;
        if (kind == BaseType::Array) {
            if (!elementType || !other.elementType) return false;
            return elementType->match(*other.elementType);
        }
        return true;
    }

    Type array() const {
        Type t(BaseType::Array);
        t.elementType = std::make_shared<Type>(*this);
        return t;
    }

    std::string toString() const {
        switch(kind) {
            case BaseType::Int: return "int";
            case BaseType::Bool: return "bool";
            case BaseType::String: return "string";
            case BaseType::Array: return "array<" + (elementType ? elementType->toString() : std::string("?")) + ">";
            case BaseType::Function: return "function";
            case BaseType::Struct: return "struct: " + customName;
            case BaseType::ExportData: return "exportData";
            case BaseType::NIL: return "nil";
        }
        return "unknown";
    }
};

struct Array {
    Type elementType;
    std::vector<TypedValue> elements;

    void add(const TypedValue &v);
};
using PArray = std::shared_ptr<Array>;
using PFunction = std::shared_ptr<Function>;
using PStruct = std::shared_ptr<Struct>;
using PExportData = std::shared_ptr<ExportData>;

using Value = std::variant<int, bool, std::nullptr_t, std::string,
                           PArray, PFunction, PStruct, PExportData>;

struct TypedValue {
    Value value;
    Type type;

    TypedValue(PExportData ed) : value(ed), type(Type(BaseType::ExportData)) {}
    TypedValue(PFunction fn) : value(fn), type(Type(BaseType::Function)) {}
    TypedValue(Value value, Type type) : value(value), type(type) {}
    TypedValue(int value) : value(value), type(Type(Primitive::INT)) {}
    TypedValue(bool value) : value(value), type(Type(Primitive::BOOL)) {}
    TypedValue(const std::string &value) : value(value), type(Type(Primitive::STRING)) {}
    TypedValue(const PArray arr, Type t) : value(arr), type(t) {}
    TypedValue(const PStruct fn) : value(fn), type(Type(BaseType::Struct)) {}
    explicit TypedValue() : value(nullptr), type(Type(BaseType::NIL)) {}

    template <typename T>
    T get() const {
        return std::get<T>(value);
    }

    template <typename T>
    std::shared_ptr<T> point() const {
        return get<std::shared_ptr<T>>();
    }
};

struct TypedIdentifier {
    std::string ident;
    Type type;

    TypedIdentifier(std::string &ident, Type type) : ident(ident), type(type) {}
};

struct Parameter {
    std::string ident;
    Type type;
    bool vararg;

    Parameter(std::string &ident, Type type) : ident(ident), type(type) {}
};

struct Function {
    std::function<std::shared_ptr<TypedValue>(const std::vector<std::shared_ptr<TypedValue>>&)> fn;
};

struct _FunctionData {
    std::vector<Parameter> params;
    Type retType;
    std::shared_ptr<ASTNode> body;
};
using FunctionData = std::shared_ptr<_FunctionData>;

struct Struct {
    const std::string name;
    const std::shared_ptr<StructType> type;

    Struct(const std::string &name, std::shared_ptr<StructType> type) : name(name), type(type) {}

    std::vector<std::pair<std::string, TypedValue>> fields;
    std::vector<std::pair<std::string, std::any>> hiddenFields;
private:
    std::unordered_map<std::string, size_t> fieldIndexMap;

public:
    void addField(const std::string &fieldName, const TypedValue &value) {
        fieldIndexMap[fieldName] = fields.size();
        fields.emplace_back(fieldName, value);
    }

    void addHiddenField(const std::string &fieldName, const std::any &value) {
        hiddenFields.emplace_back(fieldName, value);
    }

    TypedValue& getField(const std::string &fieldName) {
        auto it = fieldIndexMap.find(fieldName);
        if (it == fieldIndexMap.end()) throw std::runtime_error("Field not found: " + fieldName);
        return fields[it->second].second;
    }

    std::any& getHiddenField(const std::string &fieldName) {
        auto it = std::find_if(hiddenFields.begin(), hiddenFields.end(),
                               [&fieldName](const auto &pair){ return pair.first == fieldName; });
        if (it == hiddenFields.end()) throw std::runtime_error("Hidden field not found: " + fieldName);
        return it->second;
    }

    void setField(const std::string &fieldName, const TypedValue &value) {
        auto it = fieldIndexMap.find(fieldName);
        if (it == fieldIndexMap.end()) throw std::runtime_error("Field not found: " + fieldName);
        fields[it->second].second = value;
    }
};

struct StructType {
    std::string name;
    StructType(const std::string &name) : name(name) {}
    std::vector<std::pair<std::string, Type>> fields;
    
    bool match(Type type) const {
        return type.kind == BaseType::Struct && type.customName == name;
    }
};

struct ReturnValue {
    bool hasReturn;
    TypedValue value;
    ReturnValue() : hasReturn(false), value(TypedValue()) {}
    ReturnValue(const TypedValue &v) : hasReturn(true), value(v) {}
};

class Environment;
class Executor;
using NativeFunc = std::function<ReturnValue(std::shared_ptr<Environment>, Executor*, std::unordered_map<std::string, TypedValue>)>;

class Environment {
public:
    explicit Environment(std::shared_ptr<Environment> parent = nullptr) : parent(parent) {}

    std::stack<TypedValue> selfRefStack;
    std::unordered_map<std::string, NativeFunc> nativeInqueries;
    void registerNative(const std::string &name, NativeFunc func) {
        if(parent != nullptr) throw std::runtime_error("Cannot set native functions on a non-root environment");
        nativeInqueries[name] = func;
    }

    void set(const std::string &name, const TypedValue &val) { variables[name] = val; }
    void setType(const std::string &name, const std::shared_ptr<StructType> &type) { structTypes[name] = type; }

    void pushSelfRef(const TypedValue &val) { selfRefStack.push(val); }
    void popSelfRef() {
        if (!selfRefStack.empty()) selfRefStack.pop();
        else throw std::runtime_error("Attempted to pop empty selfRef stack");
    }
    TypedValue currentSelfRef() const {
        if (!selfRefStack.empty()) return selfRefStack.top();
        throw std::runtime_error("selfRef stack is empty");
    }
    bool hasSelfRef() const { return !selfRefStack.empty(); }

    std::shared_ptr<StructType> getType(const std::string &name) {
        if (structTypes[name]) return structTypes[name];
        if (parent) return parent->getType(name);
        return nullptr;
    }

    void modify(const std::string &name, const TypedValue &val) {
        if (variables.find(name) != variables.end()) variables[name] = val;
        else if (parent) return parent->modify(name, val);
        else variables[name] = val;
    }

    bool has(const std::string &name) const { return variables.find(name) != variables.end(); }

    TypedValue get(const std::string &name) {
        if (variables.find(name) != variables.end()) return variables[name];
        if (parent) return parent->get(name);
        throw std::runtime_error("Undefined variable: " + name);
    }

    std::shared_ptr<Environment> parent;

private:
    std::unordered_map<std::string, TypedValue> variables;
    std::unordered_map<std::string, std::shared_ptr<StructType>> structTypes;
};

using ENV = std::shared_ptr<Environment>;

struct ExportData {
    ExportData(const std::string &fileName) : fileName(fileName) {}

    std::unordered_map<std::string, std::pair<ENV, std::string>> exports;
    std::string fileName;

    void addExport(const std::string &name, ENV env) {
        exports[name] = { env, name };
    }

    TypedValue getExportedValue(const std::string &name) {
        auto it = exports.find(name);
        if (it == exports.end()) throw std::runtime_error("Export not found: " + name);
        return it->second.first->get(it->second.second);
    }
};

class Executor {
public:
    explicit Executor(std::shared_ptr<ASTNode> root);
    void printArray(std::ostream *out, const std::shared_ptr<Array> &arr);
    void printStruct(std::ostream *out, const std::shared_ptr<Struct> &st);
    void printValue(std::ostream *out, const TypedValue &val);
    TypedValue run();

    template <typename T, typename... Cases>
    T extract(const Value &val, Cases &&...cases) {
        return std::visit(overloaded{
            std::forward<Cases>(cases)...,
            [](auto&) -> T { throw std::runtime_error("Type mismatch"); }
        }, val);
    }

    std::vector<int> getIndices(const std::shared_ptr<Array> &arr,
                                const std::shared_ptr<ASTNode> &indicesNode,
                                ENV env) {
        std::vector<int> indices;

        for (const auto &idxNode : indicesNode->children) {
            if (idxNode->type == ASTNode::Type::RANGE) {
                int start = getIntValue(evaluateExpression(idxNode->children[0], env));
                int end   = getIntValue(evaluateExpression(idxNode->children[1], env));
                for (int i = start; i <= end; ++i) {
                    env->pushSelfRef(i);
                    indices.push_back(i);
                    env->popSelfRef();
                }
            } else {
                int val = getIntValue(evaluateExpression(idxNode, env));
                env->pushSelfRef(val);
                indices.push_back(val);
                env->popSelfRef();
            }
        }

        return indices;
    }

    TypedValue handleArrayAccess(const std::shared_ptr<Array> &arr, std::shared_ptr<ASTNode> indicesNode, ENV env);

    void handleArrayAssignment(const std::shared_ptr<Array> &arr,
                               std::shared_ptr<ASTNode> indicesNode,
                               ENV env,
                               std::shared_ptr<ASTNode> valNode);

    TypedValue processArrayOperation(const std::shared_ptr<Array> &arr,
                                std::shared_ptr<ASTNode> indicesNode,
                                ENV env,
                                std::optional<std::shared_ptr<ASTNode>> valNode);

    int getIntValue(const TypedValue &val);
    bool getBoolValue(const TypedValue &val);
    std::string getStringValue(const TypedValue &val);

    TypedValue arrayOperation(const std::shared_ptr<Array> &arr, const std::vector<int> &indices);
    TypedValue arrayOperation(const std::shared_ptr<Array> &arr, const std::vector<int> &indices, std::shared_ptr<ASTNode> valNode, ENV env);
    TypedValue evaluateExpression(std::shared_ptr<ASTNode> node, ENV env);

private:
    std::shared_ptr<ASTNode> root;
    ENV globalEnv;

    std::unordered_map<std::string, PExportData> exportData; 
    std::unordered_map<std::string, std::shared_ptr<ASTNode>> pragmas;
    std::vector<std::string> handlingModules;

    std::shared_ptr<Function> createNativeFunction(std::string name, FunctionData funcData, ENV env);

    TypedValue handleNDArrayAssignment(std::shared_ptr<ASTNode> node, ENV env);
    void handleStructDeclaration(std::shared_ptr<ASTNode> node, ENV env);
    TypedValue handleStructAssignment(std::shared_ptr<ASTNode> node, ENV env);

    void handleImports(std::vector<std::shared_ptr<ASTNode>> children, ENV env);

    void executePragma(std::shared_ptr<ASTNode> node, ENV env);

    void executePragmas(std::vector<std::shared_ptr<ASTNode>> children, ENV env);

    FunctionData executeFunctionDefinition(std::shared_ptr<ASTNode> node, ENV env);

    ReturnValue executeNode(std::shared_ptr<ASTNode> node, ENV env, bool extraBit = false);

    ReturnValue executeBlock(const std::vector<std::shared_ptr<ASTNode>> &nodes, ENV env);
    TypedValue handleReadAssignment(std::shared_ptr<ASTNode> node, ENV env, std::shared_ptr<ASTNode> valNode);
    TypedValue evaluateReadProperty(const TypedValue &target, const std::string &property);
    TypedValue primitiveValue(const Primitive val);
    TypedValue handleAssignment(std::shared_ptr<ASTNode> node, ENV env, Primitive primVal, bool modify);
    std::shared_ptr<Function> createFunction(FunctionData funcData, ENV closureEnv);
};

#endif
