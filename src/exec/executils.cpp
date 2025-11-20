#include "executor.hpp"
#include "executils.hpp"
#include "outstream.hpp"
#include <iostream>
#include <optional>
#include <algorithm>

Executor::Executor(std::shared_ptr<ASTNode> root) : root(root) {
    globalEnv = std::make_shared<Environment>();

    globalEnv->set("nil", TypedValue());
}

TypedValue Executor::handleNDArrayAssignment(std::shared_ptr<ASTNode> node, std::shared_ptr<Environment> env) {
    int efficiency = std::stoi(node->children[0]->strValue);

    std::vector<int> shape;
    for (size_t i = 1; i < node->children.size() - 1; ++i)
        shape.push_back(getIntValue(evaluateExpression(node->children[i], env)));

    int totalElements = 1;
    for (auto dim : shape) totalElements *= dim;

    std::shared_ptr<ASTNode> rhsNode = node->children.back();
    auto resultArr = std::make_shared<IntArray>();

    std::vector<int> indices(shape.size(), 0);
    auto indexArr = std::make_shared<IntArray>();
    for (int idx : indices) indexArr->elements.push_back(std::make_shared<int>(idx));

    switch(efficiency) {
        case 0: {
            TypedValue elementVal = evaluateExpression(rhsNode, env);
            std::shared_ptr<IntArray> rhsArr;

            if (elementVal.type.match(BaseType::ArrayInt)) {
                rhsArr = elementVal.get<std::shared_ptr<IntArray>>();
            }

            for (int flatIndex = 0; flatIndex < totalElements; ++flatIndex) {
                int finalValue;
                if (rhsArr) {
                    finalValue = rhsArr->elements.empty() ? 0 : *rhsArr->elements[flatIndex % rhsArr->elements.size()];
                } else {
                    finalValue = getIntValue(elementVal);
                }

                resultArr->elements.push_back(std::make_shared<int>(finalValue));
            }
            break;
        }
        case 1: {
            for (int flatIndex = 0; flatIndex < totalElements; ++flatIndex) {
                env->pushSelfRef(flatIndex);

                TypedValue elementVal = evaluateExpression(rhsNode, env);
                int finalValue;
                if (elementVal.type.match(BaseType::ArrayInt)) {
                    auto rhsArr = elementVal.get<std::shared_ptr<IntArray>>();
                    finalValue = rhsArr->elements.empty() ? 0 : *rhsArr->elements[flatIndex % rhsArr->elements.size()];
                } else {
                    finalValue = getIntValue(elementVal);
                }

                resultArr->elements.push_back(std::make_shared<int>(finalValue));
                env->popSelfRef();
            }
            break;
        }
        case 2: {
            env->pushSelfRef(indexArr);
            for (int flatIndex = 0; flatIndex < totalElements; ++flatIndex) {
                TypedValue elementVal = evaluateExpression(rhsNode, env);
                int finalValue;
                if (elementVal.type.match(BaseType::ArrayInt)) {
                    auto rhsArr = elementVal.get<std::shared_ptr<IntArray>>();
                    finalValue = rhsArr->elements.empty() ? 0 : *rhsArr->elements[flatIndex % rhsArr->elements.size()];
                } else {
                    finalValue = getIntValue(elementVal);
                }

                resultArr->elements.push_back(std::make_shared<int>(finalValue));

                for (int d = static_cast<int>(shape.size()) - 1; d >= 0; --d) {
                    indices[d]++;
                    *indexArr->elements[d] = indices[d];
                    if (indices[d] < shape[d]) break;
                    indices[d] = 0;
                    *indexArr->elements[d] = 0;
                }
            }
            env->popSelfRef();
        }
    }

    env->set(node->strValue, TypedValue(resultArr));
    return TypedValue(resultArr);
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

TypedValue Executor::handleStructAssignment(std::shared_ptr<ASTNode> node, std::shared_ptr<Environment> env) {
    std::string structName = node->children[0]->strValue;
    auto structType = env->getType(structName);
    if (!structType) throw std::runtime_error("Struct not found: " + structName);

    auto rhsNode = node->children[1];

    if (rhsNode->type != ASTNode::Type::BLOCK) {
        TypedValue val = evaluateExpression(rhsNode, env);

        if (!val.type.match(BaseType::Struct) || val.get<std::shared_ptr<Struct>>()->name != structName) {
            throw std::runtime_error("RHS expression does not evaluate to a struct (or an incorrect one) for direct assignment: " + structName);
        }

        env->set(node->strValue, val);
        return val;
    }

    auto newStruct = std::make_shared<Struct>(structName, structType);
    int positionalIndex = 0;

    for (auto &child : rhsNode->children) {
        std::string fieldName;
        TypedValue evaluatedValue;

        if (child->type == ASTNode::Type::PRIMITIVE_ASSIGNMENT) {
            fieldName = child->strValue;
            evaluatedValue = evaluateExpression(child->children[0], env);
        } else if (child->type == ASTNode::Type::STRUCT_ASSIGNMENT) {
            fieldName = child->strValue;
            evaluatedValue = handleStructAssignment(child, env);
        } else {
            if (positionalIndex >= structType->fields.size())
                throw std::runtime_error("Too many positional values for struct: " + structName);

            fieldName = structType->fields[positionalIndex].first;
            evaluatedValue = evaluateExpression(child, env);
            positionalIndex++;
        }

        newStruct->addField(fieldName, evaluatedValue);
    }

    TypedValue val = TypedValue(newStruct);
    env->set(node->strValue, val);
    return val;
}

int Executor::getIntValue(const TypedValue &val) {
    if(!val.type.match(BaseType::Int)) throw std::runtime_error("Expected integer value");
    return val.get<int>();
}

bool Executor::getBoolValue(const TypedValue &val) {
    if(!val.type.match(BaseType::Bool)) throw std::runtime_error("Expected boolean value");
    return val.get<bool>();
}

std::string Executor::getStringValue(const TypedValue &val) {
    if(!val.type.match(BaseType::String)) throw std::runtime_error("Expected string value");
    return val.get<std::string>();
}

TypedValue Executor::primitiveValue(const Primitive val) {
    switch (val) {
        case Primitive::INT: return 0;
        case Primitive::STRING: return "";
        case Primitive::BOOL: return false;
        default: throw std::runtime_error("Invalid primitive value");
    }
}

TypedValue Executor::handleAssignment(
    std::shared_ptr<ASTNode> node,
    std::shared_ptr<Environment> env,
    Type type,
    bool modify
) {
    if (!node->children.empty() && node->children[0]->type == ASTNode::Type::READ) {
        auto readNode = node->children[0];
        TypedValue parentVal = evaluateExpression(readNode->children[0], env);

        if(!parentVal.type.match(BaseType::Struct))
            throw std::runtime_error("Left-hand side of assignment is not a struct or object");

        auto strPtr = parentVal.get<std::shared_ptr<Struct>>();

        const std::string &prop = readNode->children[1]->strValue;
        auto it = std::find_if(strPtr->fields.begin(), strPtr->fields.end(),
                                [&prop](const auto &pair){ return pair.first == prop; });
        if (it == strPtr->fields.end())
            throw std::runtime_error("Struct does not have field: " + prop);

        env->pushSelfRef(it->second);
        TypedValue val = evaluateExpression(node->children[1], env);
        if(!val.type.match(it->second.type)) throw std::runtime_error("Incompatible types for assignment");
        it->second = val;
        env->popSelfRef();
        return val;
    }

    if (modify) env->pushSelfRef(env->get(node->strValue));

    TypedValue val = node->children.empty() ? TypedValue(0) : evaluateExpression(node->children[0], env);
    if(!val.type.match(type)) throw std::runtime_error("Incompatible types for assignment");

    if (modify) env->modify(node->strValue, val);
    else env->set(node->strValue, val);

    if (modify) env->popSelfRef();
    return val;
}

template<typename T>
TypedValue readOnArray(std::shared_ptr<T> arr, const std::string &property) {
    if (property == "length") return TypedValue(static_cast<int>(arr->elements.size()));
    throw std::runtime_error("Unknown array property: " + property);
}

TypedValue readOnStruct(const std::shared_ptr<Struct> &str, const std::string &property) {
    auto it = std::find_if(str->fields.begin(), str->fields.end(),
        [&property](const auto& pair){ return pair.first == property; });

    if (it == str->fields.end()) {
        throw std::runtime_error("Struct does not have field: " + property);
    }

    return it->second;
}

TypedValue Executor::handleReadAssignment(
    std::shared_ptr<ASTNode> readNode,
    std::shared_ptr<Environment> env,
    std::shared_ptr<ASTNode> valNode
) {
    if (readNode->type != ASTNode::Type::READ)
        throw std::runtime_error("Expected READ node for member assignment");

    TypedValue parentVal = evaluateExpression(readNode->children[0], env);
    const std::string &prop = readNode->children[1]->strValue;
    TypedValue val = evaluateExpression(valNode, env);

    switch(val.type.kind) {
        case BaseType::Struct: {
            auto str = val.get<std::shared_ptr<Struct>>();
            auto it = std::find_if(str->fields.begin(), str->fields.end(),
                [&prop](const auto &pair){ return pair.first == prop; });
            if (it == str->fields.end())
                throw std::runtime_error("Struct does not have field: " + prop);

            it->second = val;
        }
        case BaseType::ArrayBool:
        case BaseType::ArrayString:
        case BaseType::ArrayInt: {
            if(prop == "length") throw std::runtime_error("Cannot modify array length");
            throw std::runtime_error("Cannot modify array elements");
        }
    }

    return val;
}

TypedValue Executor::evaluateReadProperty(const TypedValue &target, const std::string &property) {
    switch(target.type.kind) {
        case BaseType::Struct: {
            auto str = target.get<std::shared_ptr<Struct>>();
            return readOnStruct(str, property);
        }
        case BaseType::ArrayInt: {
            auto arr = target.get<std::shared_ptr<IntArray>>();
            return readOnArray(arr, property);
        }
        case BaseType::ArrayBool: {
            auto arr = target.get<std::shared_ptr<BoolArray>>();
            return readOnArray(arr, property);
        }
        case BaseType::ArrayString: {
            auto arr = target.get<std::shared_ptr<StringArray>>();
            return readOnArray(arr, property);
        }
        case BaseType::ExportData: {
            auto exp = target.get<std::shared_ptr<ExportData>>();
            return exp->getExportedValue(property);
        }
        default:
            throw std::runtime_error("Attempted READ on non-object");
    }
}