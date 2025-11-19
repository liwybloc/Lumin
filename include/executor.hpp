#ifndef EXECUTOR_H
#define EXECUTOR_H

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

struct Function;
struct Struct;
struct ExportData;

template<typename T>
struct Array {
    using value_type = T;

    std::vector<std::shared_ptr<T>> elements;

    void add(const std::shared_ptr<T>& element) {
        elements.push_back(element);
    }
};

using IntArray    = Array<int>;
using BoolArray   = Array<bool>;
using StringArray = Array<std::string>;

using Type = std::variant<Primitive, std::string>;

using Value = std::variant<int, bool, std::nullptr_t, std::string,
                           std::shared_ptr<IntArray>,
                           std::shared_ptr<BoolArray>,
                           std::shared_ptr<StringArray>,
                           std::shared_ptr<Function>,
                           std::shared_ptr<Struct>,
                           std::shared_ptr<ExportData>>;

struct Function {
    std::function<std::shared_ptr<Value>(const std::vector<std::shared_ptr<Value>>&)> fn;
};

struct StructType {
    std::string name;
    StructType(const std::string &name) : name(name) {}
    std::vector<std::pair<std::string, Type>> fields; 
};

struct Struct {
    const std::string name;
    const std::shared_ptr<StructType> type;

    Struct(const std::string &name, std::shared_ptr<StructType> type) : name(name), type(type) {}

    std::vector<std::pair<std::string, Value>> fields;
    std::vector<std::pair<std::string, std::any>> hiddenFields;
private:
    std::unordered_map<std::string, size_t> fieldIndexMap;

public:
    void addField(const std::string &fieldName, const Value &value) {
        fieldIndexMap[fieldName] = fields.size();
        fields.emplace_back(fieldName, value);
    }

    void addHiddenField(const std::string &fieldName, const std::any &value) {
        hiddenFields.emplace_back(fieldName, value);
    }

    Value& getField(const std::string &fieldName) {
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

    void setField(const std::string &fieldName, const Value &value) {
        auto it = fieldIndexMap.find(fieldName);
        if (it == fieldIndexMap.end()) throw std::runtime_error("Field not found: " + fieldName);
        fields[it->second].second = value;
    }
};


struct ReturnValue {
    bool hasReturn;
    Value value;
    ReturnValue() : hasReturn(false), value(Value(nullptr)) {}
    ReturnValue(const Value &v) : hasReturn(true), value(v) {}
};

class Environment {
public:
    explicit Environment(std::shared_ptr<Environment> parent = nullptr) : parent(parent) {}

    std::stack<Value> selfRefStack;

    void set(const std::string &name, const Value &val) { variables[name] = val; }
    void setType(const std::string &name, const std::shared_ptr<StructType> &type) { structTypes[name] = type; }

    void pushSelfRef(const Value &val) { selfRefStack.push(val); }
    void popSelfRef() { 
        if (!selfRefStack.empty()) selfRefStack.pop(); 
        else throw std::runtime_error("Attempted to pop empty selfRef stack");
    }
    Value currentSelfRef() const { 
        if (!selfRefStack.empty()) return selfRefStack.top(); 
        throw std::runtime_error("selfRef stack is empty");
    }
    bool hasSelfRef() const { return !selfRefStack.empty(); }

    std::shared_ptr<StructType> getType(const std::string &name) {
        if (structTypes[name]) return structTypes[name];
        if (parent) return parent->getType(name);
        return nullptr;
    }

    void modify(const std::string &name, const Value &val) {
        if (variables.find(name) != variables.end()) variables[name] = val;
        else if (parent) return parent->modify(name, val);
        else variables[name] = val;
    }

    bool has(const std::string &name) const { return variables.find(name) != variables.end(); }

    Value get(const std::string &name) {
        if (variables.find(name) != variables.end()) return variables[name];
        if (parent) return parent->get(name);
        throw std::runtime_error("Undefined variable: " + name);
    }

private:
    std::unordered_map<std::string, Value> variables;
    std::unordered_map<std::string, std::shared_ptr<StructType>> structTypes;
    std::shared_ptr<Environment> parent;
};

using ENV = std::shared_ptr<Environment>;

struct ExportData {

    ExportData(const std::string &fileName) : fileName(fileName) {}

    std::unordered_map<std::string, std::pair<ENV, std::string>> exports;
    std::string fileName;

    void addExport(const std::string &name, ENV env) {
        exports[name] = { env, name };
    }

    Value getExportedValue(const std::string &name) {
        auto it = exports.find(name);
        if (it == exports.end()) throw std::runtime_error("Export not found: " + name);
        return it->second.first->get(it->second.second);
    }
};

class Executor {
public:
    explicit Executor(std::shared_ptr<ASTNode> root);
    Value run();

    template <typename T, typename... Cases>
    T extract(const Value &val, Cases &&...cases) {
        return std::visit(overloaded{
            std::forward<Cases>(cases)...,
            [](auto&) -> T { throw std::runtime_error("Type mismatch"); }
        }, val);
    }

    template <typename T>
    std::vector<int> getIndices(const std::shared_ptr<Array<T>> &arr,
                                const std::shared_ptr<ASTNode> &indicesNode,
                                std::shared_ptr<Environment> env) {
        std::vector<int> indices;
        int selfRefValue = 0;

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

    template <typename T, typename ArrayType>
    Value handleArrayAccess(const std::shared_ptr<ArrayType> &arr, std::shared_ptr<ASTNode> indicesNode, std::shared_ptr<Environment> env);

    template <typename T, typename ArrayType>
    void handleArrayAssignment(const std::shared_ptr<ArrayType> &arr,
                               std::shared_ptr<ASTNode> indicesNode,
                               std::shared_ptr<Environment> env,
                               std::shared_ptr<ASTNode> valNode);

    template <typename T, typename ArrayType>
    Value processArrayOperation(const std::shared_ptr<ArrayType> &arr,
                                std::shared_ptr<ASTNode> indicesNode,
                                std::shared_ptr<Environment> env,
                                std::optional<std::shared_ptr<ASTNode>> valNode);

    int getIntValue(const Value &val);
    bool getBoolValue(const Value &val);
    std::string getStringValue(const Value &val);

    void printStruct(const std::shared_ptr<Struct> &_struct);
    template<typename T, typename ArrayType>
    void printArray(const std::shared_ptr<ArrayType> &arr);
    template <typename T, typename ArrayType>
    Value arrayOperation(const std::shared_ptr<ArrayType> &arr, const std::vector<int> &indices);
    template <typename T, typename ArrayType>
    Value arrayOperation(const std::shared_ptr<ArrayType> &arr, const std::vector<int> &indices, std::shared_ptr<ASTNode> valNode, std::shared_ptr<Environment> env);
    void printValue(const Value &val);

private:
    std::shared_ptr<ASTNode> root;
    std::shared_ptr<Environment> globalEnv;

    std::unordered_map<std::string, std::shared_ptr<ExportData>> exportData; 
    std::unordered_map<std::string, std::shared_ptr<ASTNode>> pragmas;
    std::vector<std::string> handlingModules;

    Value handleNDArrayAssignment(std::shared_ptr<ASTNode> node, std::shared_ptr<Environment> env);
    void handleStructDeclaration(std::shared_ptr<ASTNode> node, std::shared_ptr<Environment> env);
    Value handleStructAssignment(std::shared_ptr<ASTNode> node, std::shared_ptr<Environment> env);

    void handleImports(std::vector<std::shared_ptr<ASTNode>> children, ENV env);

    void executePragma(std::shared_ptr<ASTNode> node, std::shared_ptr<Environment> env);

    void executePragmas(std::vector<std::shared_ptr<ASTNode>> children, std::shared_ptr<Environment> env);

    ReturnValue executeNode(std::shared_ptr<ASTNode> node, std::shared_ptr<Environment> env);
    ReturnValue executeBlock(const std::vector<std::shared_ptr<ASTNode>> &nodes, std::shared_ptr<Environment> env);
    std::shared_ptr<Function> createFunction(const std::vector<std::string> &params, std::shared_ptr<ASTNode> body, std::shared_ptr<Environment> closureEnv);
    Value handleReadAssignment(std::shared_ptr<ASTNode> node, std::shared_ptr<Environment> env, std::shared_ptr<ASTNode> valNode);
    Value evaluateReadProperty(const Value &target, const std::string &property);
    Value handleAssignment(std::shared_ptr<ASTNode> node, std::shared_ptr<Environment> env, bool modify);
    Value primitiveValue(const Primitive val);
    Value evaluateExpression(std::shared_ptr<ASTNode> node, std::shared_ptr<Environment> env);
};

#endif
