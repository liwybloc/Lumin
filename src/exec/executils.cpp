#include "executor.hpp"
#include "executils.hpp"
#include "outstream.hpp"
#include <iostream>
#include <optional>
#include <algorithm>
#include <fstream>

Executor::Executor(std::shared_ptr<ASTNode> root) : root(root) {
    globalEnv = std::make_shared<Environment>();

    globalEnv->set("nil", TypedValue());

    std::ofstream debugFile("astdebug2.txt");
    if (debugFile.is_open()) {
        debugFile << astToString(root).c_str();
        debugFile.close();
    } else {
        std::cerr << "Failed to open astdebug2.txt for writing\n";
    }
}

TypedValue Executor::handleNDArrayAssignment(std::shared_ptr<ASTNode> node, ENV env) {
    int efficiency = std::stoi(node->children[0]->strValue);

    std::vector<int> shape;
    for (size_t i = 1; i < node->children.size() - 1; ++i)
        shape.push_back(getIntValue(evaluateExpression(node->children[i], env)));

    int totalElements = 1;
    for (auto dim : shape) totalElements *= dim;

    std::shared_ptr<ASTNode> rhsNode = node->children.back();
    auto resultArr = std::make_shared<Array>();
    resultArr->elementType = Type(Primitive::INT);

    std::vector<int> indices(shape.size(), 0);

    switch(efficiency) {
        case 0: {
            TypedValue elementVal = evaluateExpression(rhsNode, env);
            std::shared_ptr<Array> rhsArr;

            if (elementVal.type.match(BaseType::Array)) {
                rhsArr = elementVal.get<std::shared_ptr<Array>>();
            }

            for (int flatIndex = 0; flatIndex < totalElements; ++flatIndex) {
                TypedValue finalVal;
                if (rhsArr) {
                    finalVal = rhsArr->elements.empty() 
                        ? TypedValue(0) 
                        : rhsArr->elements[flatIndex % rhsArr->elements.size()];
                } else {
                    finalVal = elementVal;
                }

                resultArr->elements.push_back(finalVal);
            }
            break;
        }
        case 1: {
            for (int flatIndex = 0; flatIndex < totalElements; ++flatIndex) {
                env->pushSelfRef(TypedValue(flatIndex));
                TypedValue elementVal = evaluateExpression(rhsNode, env);
                TypedValue finalVal;

                if (elementVal.type.match(BaseType::Array)) {
                    auto rhsArr = elementVal.get<std::shared_ptr<Array>>();
                    finalVal = rhsArr->elements.empty() 
                        ? TypedValue(0) 
                        : rhsArr->elements[flatIndex % rhsArr->elements.size()];
                } else {
                    finalVal = elementVal;
                }

                resultArr->elements.push_back(finalVal);
                env->popSelfRef();
            }
            break;
        }
        case 2: {
            auto indexArr = std::make_shared<Array>();
            indexArr->elementType = Type(Primitive::INT);
            for (auto idx : indices)
                indexArr->elements.push_back(TypedValue(idx));

            env->pushSelfRef(TypedValue(indexArr, Type(BaseType::Int)));
            for (int flatIndex = 0; flatIndex < totalElements; ++flatIndex) {
                TypedValue elementVal = evaluateExpression(rhsNode, env);
                TypedValue finalVal;

                if (elementVal.type.match(BaseType::Array)) {
                    auto rhsArr = elementVal.get<std::shared_ptr<Array>>();
                    finalVal = rhsArr->elements.empty() 
                        ? TypedValue(0) 
                        : rhsArr->elements[flatIndex % rhsArr->elements.size()];
                } else {
                    finalVal = elementVal;
                }

                resultArr->elements.push_back(finalVal);

                for (int d = static_cast<int>(shape.size()) - 1; d >= 0; --d) {
                    indices[d]++;
                    indexArr->elements[d] = TypedValue(indices[d]);
                    if (indices[d] < shape[d]) break;
                    indices[d] = 0;
                    indexArr->elements[d] = TypedValue(0);
                }
            }
            env->popSelfRef();
            break;
        }
    }

    env->set(node->strValue, TypedValue(resultArr, resultArr->elementType.array()));
    return TypedValue(resultArr, resultArr->elementType.array());
}


void Executor::handleStructDeclaration(std::shared_ptr<ASTNode> node, ENV env) {
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

TypedValue Executor::handleStructAssignment(std::shared_ptr<ASTNode> node, ENV env) {
    const std::string varName = node->strValue;
    const std::string structName = node->children[0]->strValue;

    auto structType = env->getType(structName);
    if (!structType)
        throw std::runtime_error("Unknown struct type: " + structName);

    auto structDef = std::static_pointer_cast<StructType>(structType);
    auto instance = std::make_shared<Struct>(structName, structType);

    if (node->children.size() - 1 != structDef->fields.size())
        throw std::runtime_error("Struct assignment has incorrect number of arguments");

    for (size_t i = 0; i < structDef->fields.size(); ++i) {
        auto &field = structDef->fields[i];
        auto argNode = node->children[i + 1];
        TypedValue val;

        if (argNode->type == ASTNode::Type::PRIMITIVE_ASSIGNMENT) {
            const std::string fieldName = argNode->strValue;
            TypedValue inner = evaluateExpression(argNode->children[0], env);
            if (!inner.type.match(field.second))
                throw std::runtime_error("Type mismatch for field: " + fieldName);
            val = inner;
        } else {
            TypedValue literal = evaluateExpression(argNode, env);
            if (!literal.type.match(field.second))
                throw std::runtime_error("Type mismatch for field at index " + std::to_string(i));
            val = literal;
        }

        instance->fields.emplace_back(field.first, val);
    }

    TypedValue finalVal(instance, Type(structName));
    env->set(varName, finalVal);
    return finalVal;
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
    ENV env,
    Primitive primVal,
    bool modify
) {

    TypedValue val;

    auto inferArrayType = [this](const std::shared_ptr<ASTNode> &arrayNode, ENV env) -> Type {
        if (arrayNode->children.empty()) return Type(BaseType::Array); // empty array defaults to array<nil>
        TypedValue firstVal = evaluateExpression(arrayNode->children[0], env);
        Type elemType = firstVal.type;

        for (size_t i = 1; i < arrayNode->children.size(); ++i) {
            TypedValue nextVal = evaluateExpression(arrayNode->children[i], env);
            if (!nextVal.type.match(elemType))
                throw std::runtime_error(
                    "Array literal contains mixed types: " + elemType.toString() + " vs " + nextVal.type.toString()
                );
        }

        return elemType.array();
    };

    // Struct property assignment
    if (node->children.size() > 1 && node->children[0]->type == ASTNode::Type::READ) {
        auto readNode = node->children[0];
        TypedValue parentVal = evaluateExpression(readNode->children[0], env);

        if (!parentVal.type.match(BaseType::Struct))
            throw std::runtime_error("Left-hand side of assignment is not a struct or object");

        auto strPtr = parentVal.get<std::shared_ptr<Struct>>();
        const std::string &prop = readNode->children[1]->strValue;

        auto it = std::find_if(strPtr->fields.begin(), strPtr->fields.end(),
                               [&prop](const auto &pair){ return pair.first == prop; });
        if (it == strPtr->fields.end())
            throw std::runtime_error("Struct does not have field: " + prop);

        env->pushSelfRef(it->second);
        val = evaluateExpression(node->children[1], env);

        if (node->children[1]->type == ASTNode::Type::ARRAY_LITERAL)
            val.type = inferArrayType(node->children[1], env);

        if (!val.type.match(it->second.type))
            throw std::runtime_error("Incompatible types for assignment; expected " +
                                     it->second.type.toString() + " but got " +
                                     val.type.toString() + " for field: " + prop);

        it->second = val;
        env->popSelfRef();
        return val;
    }

    // Normal variable assignment
    if (modify) env->pushSelfRef(env->get(node->strValue));
    val = node->children.empty() ? TypedValue(0) : evaluateExpression(node->children[0], env);

    // Infer array type and override expected type
    Type type;
    if (!node->children.empty() && node->children[0]->type == ASTNode::Type::ARRAY_LITERAL)
        type = inferArrayType(node->children[0], env);
    else 
        type = Type(primVal);

    if (!val.type.match(type))
        throw std::runtime_error("Incompatible types for assignment; expected " +
                                 type.toString() + " but got " +
                                 val.type.toString());

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
    ENV env,
    std::shared_ptr<ASTNode> valNode
) {
    if (readNode->type != ASTNode::Type::READ)
        throw std::runtime_error("Expected READ node for member assignment");

    TypedValue parentVal = evaluateExpression(readNode->children[0], env);
    const std::string &prop = readNode->children[1]->strValue;
    TypedValue val = evaluateExpression(valNode, env);

    switch(parentVal.type.kind) {
        case BaseType::Struct: {
            auto str = parentVal.get<std::shared_ptr<Struct>>();
            auto it = std::find_if(str->fields.begin(), str->fields.end(),
                [&prop](const auto &pair){ return pair.first == prop; });
            if (it == str->fields.end())
                throw std::runtime_error("Struct does not have field: " + prop);

            it->second = val;
            break;
        }
        case BaseType::Array: {
            auto arr = parentVal.get<std::shared_ptr<Array>>();
            if(prop == "length")
                throw std::runtime_error("Cannot modify array length");
            throw std::runtime_error("Cannot modify array elements");
            break;
        }
        default:
            throw std::runtime_error("Cannot assign to non-object property");
    }

    return val;
}


TypedValue Executor::evaluateReadProperty(const TypedValue &target, const std::string &property) {
    switch(target.type.kind) {
        case BaseType::Struct: {
            auto str = target.get<std::shared_ptr<Struct>>();
            return readOnStruct(str, property);
        }
        case BaseType::Array: {
            auto arr = target.get<std::shared_ptr<Array>>();
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