#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "parser.hpp"
#include "executils.hpp"
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

struct Function;
struct Struct;

template<typename T>
struct Array {
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
                           std::shared_ptr<Struct>>;

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

    std::shared_ptr<Value> selfRef = nullptr;

    void set(const std::string &name, const Value &val) { variables[name] = val; }
    void setType(const std::string &name, const std::shared_ptr<StructType> &type) { structTypes[name] = type; }

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

class Executor {
public:
    explicit Executor(std::shared_ptr<ASTNode> root);
    Value run();

    void handleImport(const std::string &moduleName, std::shared_ptr<Environment> env);

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
            int val;
            if (idxNode->type == ASTNode::Type::RANGE) {
                int start = getIntValue(evaluateExpression(idxNode->children[0], env));
                env->selfRef = std::make_shared<Value>(start);
                int end   = getIntValue(evaluateExpression(idxNode->children[1], env));
                for (int i = start; i <= end; ++i) {
                    selfRefValue = i;
                    env->selfRef = std::make_shared<Value>(selfRefValue);
                    indices.push_back(i);
                }
            } else {
                val = getIntValue(evaluateExpression(idxNode, env));
                selfRefValue = val;
                env->selfRef = std::make_shared<Value>(selfRefValue);
                indices.push_back(val);
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
    template <typename ArrayType>
    void printArray(const std::shared_ptr<ArrayType> &arr);
    void printValue(const Value &val);

private:
    std::shared_ptr<ASTNode> root;
    std::shared_ptr<Environment> globalEnv;

    Value handleNDArrayAssignment(std::shared_ptr<ASTNode> node, std::shared_ptr<Environment> env);
    void handleStructDeclaration(std::shared_ptr<ASTNode> node, std::shared_ptr<Environment> env);
    Value handleStructAssignment(std::shared_ptr<ASTNode> node, std::shared_ptr<Environment> env);

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
