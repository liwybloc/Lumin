#include "executor.hpp"
#include "executils.hpp"
#include <iostream>
#include <optional>
#include <algorithm>

static void printValue(const Value &val);

template <typename ArrayType>
void printArray(const std::shared_ptr<ArrayType> &arr) {
    std::cout << "[";
    for (size_t i = 0; i < arr->elements.size(); ++i) {
        printValue(*arr->elements[i]);
        if (i + 1 < arr->elements.size()) std::cout << ", ";
    }
    std::cout << "]";
}

void printStruct(const std::shared_ptr<Struct> &_struct) {
    std::cout << _struct->name << "{";
    int index = 0;
    for (const auto &[name, value] : _struct->fields) {
        std::cout << name << ": ";
        printValue(value);
        if(++index < _struct->fields.size()) std::cout << ",";
    }
    std::cout << "}";
}

static void printValue(const Value &val) {
    std::visit(overloaded{
        [](int v) { std::cout << v; },
        [](bool b) { std::cout << (b ? "true" : "false"); },
        [](const std::string &s) { std::cout << s; },
        [](const std::shared_ptr<IntArray> &arr) { printArray(arr); },
        [](const std::shared_ptr<BoolArray> &arr) { printArray(arr); },
        [](const std::shared_ptr<StringArray> &arr) { printArray(arr); },
        [](const std::shared_ptr<Function> &) { std::cout << "[function]"; },
        [](const std::shared_ptr<Struct> &_struct) { printStruct(_struct); },
        [](std::nullptr_t) { std::cout << "nil"; },
        [](auto&) { std::cout << "[unknown]"; }
    }, val);
}

Executor::Executor(std::shared_ptr<ASTNode> root) : root(root) {
    globalEnv = std::make_shared<Environment>();

    auto printFunc = [](const std::vector<std::shared_ptr<Value>> &args, bool newline) -> std::shared_ptr<Value> {
        for (const auto &arg : args) printValue(*arg);
        if (newline) std::cout << std::endl;
        return std::make_shared<Value>(0);
    };

    globalEnv->set("print", std::make_shared<Function>(Function{
        [printFunc](const std::vector<std::shared_ptr<Value>> &args) { return printFunc(args, false); }
    }));
    globalEnv->set("println", std::make_shared<Function>(Function{
        [printFunc](const std::vector<std::shared_ptr<Value>> &args) { return printFunc(args, true); }
    }));
    globalEnv->set("printf", std::make_shared<Function>(Function{
        [this](const std::vector<std::shared_ptr<Value>> &args) -> std::shared_ptr<Value> {
            if (args.empty()) return std::make_shared<Value>(0);

            std::string format = getStringValue(*args[0]);
            size_t argIndex = 1;
            size_t pos = 0;

            while ((pos = format.find("{}", pos)) != std::string::npos && argIndex < args.size()) {
                std::string str = getStringValue(*args[argIndex]);
                format.replace(pos, 2, str);
                pos += str.size();
                argIndex++;
            }

            std::cout << format;
            return std::make_shared<Value>(0);
        }
    }));

    globalEnv->set("true", Value(true));
    globalEnv->set("false", Value(false));
    globalEnv->set("nil", Value(nullptr));
}

Value Executor::handleNDArrayAssignment(std::shared_ptr<ASTNode> node, std::shared_ptr<Environment> env) {
    std::vector<int> shape;
    for (size_t i = 0; i < node->children.size() - 1; ++i)
        shape.push_back(getIntValue(evaluateExpression(node->children[i], env)));

    int firstDim = shape[0]; 
    int totalElements = 1;
    for (size_t i = 1; i < shape.size(); ++i)
        totalElements *= shape[i];

    std::shared_ptr<ASTNode> rhsNode = node->children.back();
    auto resultArr = std::make_shared<IntArray>();

    for (int currentIndex = 0; currentIndex < totalElements; ++currentIndex) {
        env->selfRef = std::make_shared<Value>(currentIndex);
        for (int subIndex = 0; subIndex < firstDim; ++subIndex) {
            Value elementVal = evaluateExpression(rhsNode, env);

            int finalValue;
            if (std::holds_alternative<std::shared_ptr<IntArray>>(elementVal)) {
                auto rhsArr = std::get<std::shared_ptr<IntArray>>(elementVal);
                finalValue = rhsArr->elements.empty() ? 0 : *rhsArr->elements[subIndex % rhsArr->elements.size()];
            } else {
                finalValue = getIntValue(elementVal);
            }

            resultArr->elements.push_back(std::make_shared<int>(finalValue));
        }
    }

    env->set(node->strValue, Value(resultArr));
    return Value(resultArr);
}

void Executor::handleStructDeclaration(std::shared_ptr<ASTNode> node, std::shared_ptr<Environment> env) {
    std::string structName = node->strValue;
    std::shared_ptr<StructType> _struct = std::make_shared<StructType>(structName);

    for (const auto &child : node->children) {
        if(child->type == ASTNode::Type::PRIMITIVE_ASSIGNMENT) {
            _struct->fields.push_back(std::make_pair(child->strValue, child->primitiveValue));
        } else {
            _struct->fields.push_back(std::make_pair(child->strValue, child->children[0]->strValue));
        }
    }

    env->setType(structName, _struct);
}

Value Executor::handleStructAssignment(std::shared_ptr<ASTNode> node, std::shared_ptr<Environment> env) {
    std::string structName = node->children[0]->strValue;
    auto structValue = env->getType(structName);
    if(structValue == nullptr) throw std::runtime_error("Struct not found: " + structName);

    Struct _struct = Struct(structName, structValue);

    auto block = node->children[1];
    int positionalIndex = 0;

    for(auto &child : block->children) {
        if(child->type == ASTNode::Type::PRIMITIVE_ASSIGNMENT || child->type == ASTNode::Type::STRUCT_ASSIGNMENT) {
            std::string fieldName = child->strValue;
            
            auto it = std::find_if(structValue->fields.begin(), structValue->fields.end(),
                [&fieldName](const auto& pair){ return pair.first == fieldName; });

            if (it == structValue->fields.end()) {
                throw std::runtime_error("Struct " + structName + " does not have field: " + fieldName);
            }
            
            Value evaluatedValue;
            if(child->type == ASTNode::Type::PRIMITIVE_ASSIGNMENT) {
                evaluatedValue = evaluateExpression(child->children[0], env);
            } else { // STRUCT_ASSIGNMENT
                evaluatedValue = handleStructAssignment(child, env);
            }
            _struct.fields.push_back({fieldName, evaluatedValue});
            
        } else {
            if (positionalIndex >= structValue->fields.size())
                throw std::runtime_error("Too many positional values for struct: " + structName);

            const std::string &key = structValue->fields[positionalIndex].first;
            
            Value evaluatedValue = evaluateExpression(child, env);
        
            _struct.fields.push_back({key, evaluatedValue});
            
            positionalIndex++;
        }
    }

    Value val = Value(std::make_shared<Struct>(_struct));
    env->set(node->strValue, val);
    return val;
}
int Executor::getIntValue(const Value &val) {
    return extract<int>(
        val,
        [](int v) { return v; },
        [](bool b) { return b ? 1 : 0; }
    );
}

bool Executor::getBoolValue(const Value &val) {
    return extract<bool>(
        val,
        [](bool b) { return b; },
        [](int i) { return i != 0; },
        [](const std::string &s) { return !s.empty(); }
    );
}

std::string Executor::getStringValue(const Value &val) {
    return extract<std::string>(
        val,
        [](const std::string &s) { return s; },
        [](int i) { return std::to_string(i); },
        [](bool b) { return std::string(b ? "true" : "false"); }
    );
}

Value Executor::primitiveValue(const Primitive val) {
    switch (val) {
        case Primitive::INT: return 0;
        case Primitive::STRING: return "";
        case Primitive::BOOL: return false;
        default: throw std::runtime_error("Invalid primitive value");
    }
}

Value Executor::handleAssignment(
    std::shared_ptr<ASTNode> node,
    std::shared_ptr<Environment> env,
    bool modify
) {
    if (!node->children.empty() && node->children[0]->type == ASTNode::Type::READ) {
        auto readNode = node->children[0];
        Value parentVal = evaluateExpression(readNode->children[0], env);

        if (auto strPtr = std::get_if<std::shared_ptr<Struct>>(&parentVal)) {
            const std::string &prop = readNode->children[1]->strValue;
            auto it = std::find_if((*strPtr)->fields.begin(), (*strPtr)->fields.end(),
                                   [&prop](const auto &pair){ return pair.first == prop; });
            if (it == (*strPtr)->fields.end())
                throw std::runtime_error("Struct does not have field: " + prop);

            env->selfRef = std::make_shared<Value>(it->second);
            Value val = evaluateExpression(node->children[1], env);
            it->second = val;
            env->selfRef = nullptr;
            return val;
        }

        throw std::runtime_error("Left-hand side of assignment is not a struct or object");
    }

    env->selfRef = std::make_shared<Value>(env->get(node->strValue));
    Value val = node->children.empty() ? Value(0) : evaluateExpression(node->children[0], env);
    if (modify) env->modify(node->strValue, val);
    else env->set(node->strValue, val);
    env->selfRef = nullptr;
    return val;
}

template<typename T>
Value readOnArray(std::shared_ptr<T> arr, const std::string &property) {
    if (property == "length") return Value(static_cast<int>(arr->elements.size()));
    throw std::runtime_error("Unknown array property: " + property);
}

Value readOnStruct(const std::shared_ptr<Struct> &str, const std::string &property) {
    auto it = std::find_if(str->fields.begin(), str->fields.end(),
        [&property](const auto& pair){ return pair.first == property; });

    if (it == str->fields.end()) {
        throw std::runtime_error("Struct does not have field: " + property);
    }

    return it->second;
}

Value Executor::handleReadAssignment(
    std::shared_ptr<ASTNode> readNode,
    std::shared_ptr<Environment> env,
    std::shared_ptr<ASTNode> valNode
) {
    if (readNode->type != ASTNode::Type::READ)
        throw std::runtime_error("Expected READ node for member assignment");

    Value parentVal = evaluateExpression(readNode->children[0], env);
    const std::string &prop = readNode->children[1]->strValue;
    Value val = evaluateExpression(valNode, env);

    return std::visit(overloaded{
        [&](const std::shared_ptr<Struct> &str) -> Value {
            auto it = std::find_if(str->fields.begin(), str->fields.end(),
                [&prop](const auto &pair){ return pair.first == prop; });
            if (it == str->fields.end())
                throw std::runtime_error("Struct does not have field: " + prop);

            it->second = val;
            return val;
        },
        [&](const std::shared_ptr<IntArray> &arr) -> Value {
            if (prop == "length")
                throw std::runtime_error("Cannot assign to array length");
            throw std::runtime_error("Unsupported array property assignment: " + prop);
        },
        [&](auto&) -> Value {
            throw std::runtime_error("Left-hand side of assignment is not a struct or object");
        }
    }, parentVal);
}

Value Executor::evaluateReadProperty(const Value &target, const std::string &property) {
    return std::visit(overloaded{
        [&](const std::shared_ptr<IntArray> &arr) { return readOnArray(arr, property); },
        [&](const std::shared_ptr<BoolArray> &arr) { return readOnArray(arr, property); },
        [&](const std::shared_ptr<StringArray> &arr) { return readOnArray(arr, property); },
        [&](const std::shared_ptr<Struct> &str) { return readOnStruct(str, property); },
        [&](auto&) -> Value {
            throw std::runtime_error("Attempted READ on non-object");
        }
    }, target);
}