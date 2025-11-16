#include "executor.hpp"
#include "executils.hpp"
#include <iostream>
#include <optional>

Value Executor::run() {
    executeNode(root, globalEnv);

    if (globalEnv->has("main")) {
        auto mainFunc = globalEnv->get("main");
        if (!std::holds_alternative<std::shared_ptr<Function>>(mainFunc))
            throw std::runtime_error("main is not a function");
        return *std::get<std::shared_ptr<Function>>(mainFunc)->fn({});
    }

    return Value(0);
}

ReturnValue Executor::executeNode(std::shared_ptr<ASTNode> node, std::shared_ptr<Environment> env) {
    switch (node->type) {
        case ASTNode::Type::PROGRAM:
            return executeBlock(node->children, env);
        case ASTNode::Type::BLOCK:
            return executeBlock(node->children, std::make_shared<Environment>(env));
        case ASTNode::Type::STRUCT_DECLARE: {
            handleStructDeclaration(node, env);
            return ReturnValue();
        }
        case ASTNode::Type::EXPRESSION_STATEMENT:
            evaluateExpression(node->children[0], env);
            return ReturnValue();
        case ASTNode::Type::PRIMITIVE_ASSIGNMENT: {
            handleAssignment(node, env, false);
            return ReturnValue();
        }
        case ASTNode::Type::STRUCT_ASSIGNMENT: {
            handleStructAssignment(node, env);
            return ReturnValue();
        }
        case ASTNode::Type::RETURN_STATEMENT:
            return ReturnValue(node->children.empty() ? Value(nullptr) : evaluateExpression(node->children[0], env));
        case ASTNode::Type::IF_STATEMENT:
            if (getBoolValue(evaluateExpression(node->children[0], env)))
                return executeNode(node->children[1], env);
            if (node->children.size() > 2)
                return executeNode(node->children[2], env);
            return ReturnValue();
        case ASTNode::Type::WHILE_STATEMENT:
            while (getBoolValue(evaluateExpression(node->children[0], env))) {
                ReturnValue r = executeNode(node->children[1], env);
                if (r.hasReturn) return r;
            }
            return ReturnValue();
        case ASTNode::Type::FUNCTION: {
            std::vector<std::string> params;
            for (size_t i = 0; i < node->children.size() - 1; ++i) {
                params.push_back(node->children[i]->strValue);
            }
            auto func = createFunction(params, node->children.back(), env);
            env->set(node->strValue, func);
            return ReturnValue();
        }
        case ASTNode::Type::FOR_STATEMENT: {
            executeNode(node->children[0], env);
            while (getBoolValue(evaluateExpression(node->children[1], env))) {
                ReturnValue ret = executeNode(node->children[3], env);
                if (ret.hasReturn) return ret;
                evaluateExpression(node->children[2], env);
            }
            return ReturnValue();
        }
        default:
            return ReturnValue(evaluateExpression(node, env));
    }
}

ReturnValue Executor::executeBlock(const std::vector<std::shared_ptr<ASTNode>> &nodes, std::shared_ptr<Environment> env) {
    for (const auto &child : nodes) {
        ReturnValue r = executeNode(child, env);
        if (r.hasReturn) return r;
    }
    return ReturnValue();
}

template <typename T>
T& getArrayElement(const std::shared_ptr<Array<T>> &arr, int index) {
    if (index < 0 || index >= static_cast<int>(arr->elements.size()))
        throw std::runtime_error("Array index out of bounds");
    return *arr->elements.at(index);
}
template <typename T, typename ArrayType>
Value Executor::handleArrayAccess(const std::shared_ptr<ArrayType> &arr, std::shared_ptr<ASTNode> indicesNode, std::shared_ptr<Environment> env) {
    auto indices = getIndices(arr, indicesNode, env); 
    
    if(indices.size() == 1)
        return Value(getArrayElement<T>(arr, indices[0]));
    
    auto result = std::make_shared<ArrayType>();
    for(int idx : indices)
        result->add(std::make_shared<T>(getArrayElement<T>(arr, idx)));
    return Value(result);
}
template <typename T, typename ArrayType>
void Executor::handleArrayAssignment(
    const std::shared_ptr<ArrayType> &arr,
    std::shared_ptr<ASTNode> indicesNode,
    std::shared_ptr<Environment> env,
    std::shared_ptr<ASTNode> valNode
) {
    auto indices = getIndices(arr, indicesNode, env);

    auto getValue = [this](const Value &v) -> T {
        if constexpr (std::is_same_v<T, int>) return this->getIntValue(v);
        if constexpr (std::is_same_v<T, bool>) return this->getBoolValue(v);
        if constexpr (std::is_same_v<T, std::string>) return std::get<std::string>(v);
        throw std::runtime_error("Unsupported array element type in assignment helper.");
    };

    for (int idx : indices) {
        T currentValue = getArrayElement<T>(arr, idx);

        env->selfRef = std::make_shared<Value>(Value(currentValue));

        getArrayElement<T>(arr, idx) = getValue(evaluateExpression(valNode, env));

        env->selfRef = nullptr;
    }
}

template<typename T, typename ArrayType>
Value Executor::processArrayOperation(
        const std::shared_ptr<ArrayType> &arr,
        std::shared_ptr<ASTNode> indicesNode,
        std::shared_ptr<Environment> env,
        std::optional<std::shared_ptr<ASTNode>> valNode) {

    const auto indices = getIndices(arr, indicesNode, env);

    if (!valNode) {
        if (indices.size() == 1) return Value(getArrayElement<T>(arr, indices[0]));
        auto result = std::make_shared<ArrayType>();
        for (const int idx : indices) result->add(std::make_shared<T>(getArrayElement<T>(arr, idx)));
        return Value(result);
    }

    Value val = evaluateExpression(*valNode, env);

    std::vector<T> valuesToAssign;

    if constexpr (std::is_same_v<T, int>) {
        if (std::holds_alternative<std::shared_ptr<IntArray>>(val)) {
            auto valArr = std::get<std::shared_ptr<IntArray>>(val);
            for (const auto &el : valArr->elements) valuesToAssign.push_back(*el);
        } else {
            valuesToAssign.push_back(getIntValue(val));
        }
    } else if constexpr (std::is_same_v<T, bool>) {
        if (std::holds_alternative<std::shared_ptr<BoolArray>>(val)) {
            auto valArr = std::get<std::shared_ptr<BoolArray>>(val);
            for (const auto &el : valArr->elements) valuesToAssign.push_back(*el);
        } else {
            valuesToAssign.push_back(getBoolValue(val));
        }
    } else if constexpr (std::is_same_v<T, std::string>) {
        if (std::holds_alternative<std::shared_ptr<StringArray>>(val)) {
            auto valArr = std::get<std::shared_ptr<StringArray>>(val);
            for (const auto &el : valArr->elements) valuesToAssign.push_back(*el);
        } else {
            valuesToAssign.push_back(std::get<std::string>(val));
        }
    }

    for (size_t i = 0; i < indices.size(); ++i) {
        int idx = indices[i];
        T valueToSet = i < valuesToAssign.size() ? valuesToAssign[i] : valuesToAssign.back();
        env->selfRef = std::make_shared<Value>(getArrayElement<T>(arr, idx));
        getArrayElement<T>(arr, idx) = valueToSet;
        env->selfRef = nullptr;
    }

    return Value(arr);
}

std::shared_ptr<Function> Executor::createFunction(
        const std::vector<std::string> &params,
        std::shared_ptr<ASTNode> body,
        std::shared_ptr<Environment> closureEnv) {

    const auto wrapValue = [](const Value &v) {
        if (std::holds_alternative<std::shared_ptr<Function>>(v))
            return std::make_shared<Value>(std::get<std::shared_ptr<Function>>(v));
        return std::make_shared<Value>(v);
    };

    return std::make_shared<Function>(Function{
        [this, params, body, closureEnv, wrapValue](const std::vector<std::shared_ptr<Value>> &args) {
            auto local = std::make_shared<Environment>(closureEnv);
            for (size_t i = 0; i < params.size() && i < args.size(); ++i) {
                local->set(params[i], *args[i]);
            }
            ReturnValue r = executeNode(body, local);
            if (!r.hasReturn) return std::make_shared<Value>(nullptr);
            return wrapValue(r.value);
        }
    });
}

Value Executor::evaluateExpression(std::shared_ptr<ASTNode> node, std::shared_ptr<Environment> env) {
    switch (node->type) {
        case ASTNode::Type::NUMBER: return Value(std::stoi(node->strValue));
        case ASTNode::Type::STRING: return Value(node->strValue);
        case ASTNode::Type::IDENTIFIER: return env->get(node->strValue);
        case ASTNode::Type::PRIMITIVE_ASSIGNMENT: {
            return handleAssignment(node, env, true);
        }
        case ASTNode::Type::STRUCT_ASSIGNMENT: {
            return handleStructAssignment(node, env);
        }
        case ASTNode::Type::NDARRAY_ASSIGN: {
            return handleNDArrayAssignment(node, env);
        }
        case ASTNode::Type::SIZED_ARRAY_DECLARE: {
            Value primVal = primitiveValue(node->primitiveValue);
            int size = getIntValue(evaluateExpression(node->children[0], env));

            auto arr = std::make_shared<IntArray>();
            int val = getIntValue(primVal);
            for(int i=0;i<size;i++) {
                arr->elements.push_back(std::make_shared<int>(val));
            }
            return arr;
        }
        case ASTNode::Type::READ: {
            auto target = evaluateExpression(node->children[0], env);
            auto reading = node->children[1]->strValue;
            return evaluateReadProperty(target, reading);
        }
        case ASTNode::Type::SELF_REFERENCE:
            if (env->selfRef) return *env->selfRef;
            return Value(nullptr);
        case ASTNode::Type::ARRAY_ACCESS: {
            Value arrVal = evaluateExpression(node->children[0], env);
            auto idx = node->children[1];
            return std::visit(overloaded{
                [this, &idx, &env](std::shared_ptr<IntArray> &arr) {
                    return processArrayOperation<int, IntArray>(arr, idx, env, std::nullopt);
                },
                [this, &idx, &env](std::shared_ptr<BoolArray> &arr) {
                    return processArrayOperation<bool, BoolArray>(arr, idx, env, std::nullopt);
                },
                [this, &idx, &env](std::shared_ptr<StringArray> &arr) {
                    return processArrayOperation<std::string, StringArray>(arr, idx, env, std::nullopt);
                },
                [](auto&) -> Value { throw std::runtime_error("Attempted array access on non-array"); }
            }, arrVal);
        }
        case ASTNode::Type::ARRAY_ASSIGN: {
            Value arrVal = evaluateExpression(node->children[0], env);
            auto idx = node->children[1];
            auto val = node->children[2];
            std::visit(overloaded{
                [this, &idx, &env, &val](std::shared_ptr<IntArray> &arr) {
                    processArrayOperation<int, IntArray>(arr, idx, env, val);
                },
                [this, &idx, &env, &val](std::shared_ptr<BoolArray> &arr) {
                    processArrayOperation<bool, BoolArray>(arr, idx, env, val);
                },
                [this, &idx, &env, &val](std::shared_ptr<StringArray> &arr) {
                    processArrayOperation<std::string, StringArray>(arr, idx, env, val);
                },
                [](auto&) { throw std::runtime_error("Attempted array assignment on non-array"); }
            }, arrVal);
            return Value(arrVal);
        }
        case ASTNode::Type::ARRAY_LITERAL: {
            auto arr = std::make_shared<IntArray>();
            for (const auto &child : node->children) {
                if (child->type == ASTNode::Type::RANGE) {
                    int start = getIntValue(evaluateExpression(child->children[0], env));
                    env->selfRef = std::make_shared<Value>(start);
                    int end   = getIntValue(evaluateExpression(child->children[1], env));
                    for (int i = start; i <= end; ++i)
                        arr->add(std::make_shared<int>(i));
                } else {
                    arr->add(std::make_shared<int>(getIntValue(evaluateExpression(child, env))));
                }
            }
            return Value(arr);
        }
        case ASTNode::Type::CALL: {
            auto funcVal = evaluateExpression(node->children[0], env);
            std::vector<std::shared_ptr<Value>> args;
            for (auto &argNode : node->children[1]->children)
                args.push_back(std::make_shared<Value>(evaluateExpression(argNode, env)));
            if (!std::holds_alternative<std::shared_ptr<Function>>(funcVal))
                throw std::runtime_error("Attempted to call a non-function");
            return *std::get<std::shared_ptr<Function>>(funcVal)->fn(args);
        }
        case ASTNode::Type::BINARY_OP: {
            Value leftVal = evaluateExpression(node->children[0], env);
            Value rightVal = evaluateExpression(node->children[1], env);
            if (node->binopValue == PLUS && std::holds_alternative<std::string>(leftVal))
                return Value(std::get<std::string>(leftVal) + std::get<std::string>(rightVal));
            int left = getIntValue(leftVal), right = getIntValue(rightVal);
            switch(node->binopValue) {
                case PLUS: return Value(left + right);
                case MINUS: return Value(left - right);
                case MULTIPLY: return Value(left * right);
                case DIVIDE: return Value(left / right);
                case MODULUS: return Value(left % right);
                case COMPARISON: return Value(left == right);
                case LESS: return Value(left < right);
                case GREATER: return Value(left > right);
                case LESS_EQUAL: return Value(left <= right);
                case GREATER_EQUAL: return Value(left >= right);
                default: throw std::runtime_error("Unsupported binary operation");
            }
        }
        case ASTNode::Type::UNARY_OP: {
            int val = getIntValue(evaluateExpression(node->children[0], env));
            switch(node->binopValue) {
                case MINUS: return Value(-val);
                case BITWISE_NOT: return Value(~val);
                case NOT: return Value(!val);
                default: throw std::runtime_error("Unsupported unary operation");
            }
        }
        default: throw std::runtime_error("Unsupported expression type: " + std::to_string(static_cast<int>(node->type)));
    }
}