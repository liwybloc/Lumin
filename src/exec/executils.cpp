#include "executor.hpp"
#include "executils.hpp"
#include "outstream.hpp"
#include "standard.hpp"
#include <iostream>
#include <optional>
#include <algorithm>
#include <fstream>

Executor::Executor(std::shared_ptr<ParsedASTNode> root) : root(root) {
    globalEnv = std::make_shared<Environment>();

    implStandard(globalEnv, this);

    #ifdef DEBUG
        std::ofstream debugFile("astdebug2.txt");
        if (debugFile.is_open()) {
            debugFile << astToString(root->toAST()).c_str();
            debugFile.close();
        } else {
            std::cerr << "Failed to open astdebug2.txt for writing\n";
        }
    #endif
}

TypedValue Executor::handleNDArrayAssignment(std::shared_ptr<ParsedASTNode> node, ENV env) {
    int efficiency = node->children[0]->intValue;

    std::vector<int> shape;
    for (size_t i = 1; i < node->children.size() - 1; ++i)
        shape.push_back(getIntValue(evaluateExpression(node->children[i], env)));

    int totalElements = 1;
    for (auto dim : shape) totalElements *= dim;

    std::shared_ptr<ParsedASTNode> rhsNode = node->children.back();
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


void Executor::handleStructDeclaration(std::shared_ptr<ParsedASTNode> node, ENV env) {
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

TypedValue Executor::handleStructAssignment(std::shared_ptr<ParsedASTNode> node, ENV env) {
    
    auto structType = env->getType(node->strValue);
    if (!structType)
        error("Unknown struct type: " + node->strValue);

    auto structDef = std::static_pointer_cast<StructType>(structType);
    auto instance = std::make_shared<Struct>(node->strValue, structType);

    std::vector<std::shared_ptr<ParsedASTNode>> args = node->children;

    if (args.size() != structDef->fields.size())
        error("Struct assignment has incorrect number of arguments");

    for (size_t i = 0; i < structDef->fields.size(); ++i) {
        auto &field = structDef->fields[i];
        auto argNode = args[i];
        TypedValue val;

        if (argNode->type == ASTNode::Type::PRIMITIVE_ASSIGNMENT) {
            const std::string fieldName = argNode->strValue;
            TypedValue inner = evaluateExpression(argNode->children[0], env);
            if (!inner.type.match(field.second))
                error("Type mismatch for field: " + fieldName);
            val = inner;
        } else {
            TypedValue literal = evaluateExpression(argNode, env);
            if (!literal.type.match(field.second))
                error("Type mismatch for field at index " + std::to_string(i));
            val = literal;
        }

        instance->fields.emplace_back(field.first, val);
    }

    TypedValue finalVal(instance, Type(node->strValue));
    return finalVal;
}

int Executor::getIntValue(const TypedValue &val) {
    switch(val.type.kind) {
        case BaseType::Bool: return val.get<bool>() ? 1 : 0;
        case BaseType::Int: return val.get<int>();
        case BaseType::Char: return static_cast<int>(val.get<char>());
        default: error("Expected integer value");
    }
}

bool Executor::getBoolValue(const TypedValue &val) {
    switch(val.type.kind) {
        case BaseType::Bool: return val.get<bool>();
        case BaseType::Int: return val.get<int>() != 0;
        default: error("Expected boolean value");
    }
}

std::string Executor::getStringValue(const TypedValue &val) {
    if(!val.type.match(BaseType::String)) error("Expected string value");
    return val.get<std::string>();
}

TypedValue Executor::primitiveValue(const Primitive val) {
    switch (val) {
        case Primitive::INT: return 0;
        case Primitive::STRING: return "";
        case Primitive::BOOL: return false;
        default: error("Invalid primitive value");
    }
}

static bool nodeHasDeclareFlag(const std::shared_ptr<ParsedASTNode> &node, int &outIndex) {
    outIndex = 0;
    if (node->children.size() > 0 && node->children[0]->type == ASTNode::Type::BOOL) {
        outIndex = 1;
        return true;
    }
    return false;
}
TypedValue Executor::handleAssignment(
    std::shared_ptr<ParsedASTNode> node,
    ENV env,
    Primitive primVal,
    bool modify
) {
    int idx = 0;
    bool hadFlag = nodeHasDeclareFlag(node, idx);
    bool isDeclaration = false;
    bool isModify = false;
    
    if (hadFlag) {
        isDeclaration = (node->children[0]->strValue == "1");
        isModify = !isDeclaration;
    } else {
        isDeclaration = !modify;
        isModify = modify;
    }

    auto inferArrayType = [this](const std::shared_ptr<ParsedASTNode> &arrayNode, ENV env) -> Type {
        if (arrayNode->children.empty()) return Type(BaseType::Array);
        TypedValue firstVal = evaluateExpression(arrayNode->children[0], env);
        Type elemType = firstVal.type;

        for (size_t i = 1; i < arrayNode->children.size(); ++i) {
            TypedValue nextVal = evaluateExpression(arrayNode->children[i], env);
            if (!nextVal.type.match(elemType))
                error(
                    "Array literal contains mixed types: " + elemType.toString() + " vs " + nextVal.type.toString()
                );
        }
        return elemType.array();
    };

    if (node->children.size() > idx && node->children[idx]->type == ASTNode::Type::READ && node->strValue == "") {
        auto readNode = node->children[idx];
        TypedValue parentVal = evaluateExpression(readNode->children[0], env);

        if (!parentVal.type.match(BaseType::Struct))
            error("Left-hand side of assignment is not a struct or object");

        auto strPtr = parentVal.get<std::shared_ptr<Struct>>();
        const std::string &prop = readNode->children[1]->strValue;

        auto it = std::find_if(strPtr->fields.begin(), strPtr->fields.end(),
                               [&prop](const auto &pair){ return pair.first == prop; });
        if (it == strPtr->fields.end())
            error("Struct does not have field: " + prop);

        env->pushSelfRef(it->second);
        TypedValue rhsVal = evaluateExpression(node->children[idx + 1], env);

        ASTNode::Type its = node->children[idx + 1]->type;
        if (its == ASTNode::Type::ARRAY_LITERAL || its == ASTNode::Type::SIZED_ARRAY_DECLARE) {
            Type t = inferArrayType(node->children[idx + 1], env);
            rhsVal.type = t;
        }

        if (!rhsVal.type.match(it->second.type))
            error("Incompatible types for assignment; expected " +
                                     it->second.type.toString() + " but got " +
                                     rhsVal.type.toString() + " for field: " + prop);

        it->second = rhsVal;
        env->popSelfRef();
        return rhsVal;
    }

    TypedValue val;
    if (isModify) {
        env->pushSelfRef(env->get(node->strValue));
    }

    if (node->children.size() > 0) {
        if (hadFlag) {
            int exprIndex = (int)node->children.size() - 1;
            if (exprIndex >= idx && node->children[exprIndex] != nullptr)
                val = evaluateExpression(node->children[exprIndex], env);
            else
                val = TypedValue(0);
        } else {
            val = node->children.empty() ? TypedValue(0) : evaluateExpression(node->children[0], env);
        }
    } else {
        val = TypedValue(0);
    }

    Type expectedType;
    if(primVal != Primitive::NONE) {
        if(node->children.size() > 1
            && (node->children.back()->type == ASTNode::Type::ARRAY_LITERAL || node->children.back()->type == ASTNode::Type::SIZED_ARRAY_DECLARE)) {
            Type inferred = inferArrayType(node->children.back(), env);
            val.type = inferred;
            expectedType = inferred;
        } else
            expectedType = Type(primVal);
    } else if (node->children.size() > 1 && node->children[1]->type == ASTNode::Type::NEW_STRUCT) {
        expectedType = Type(node->children[1]->strValue);
    } else {
        expectedType = val.type;
    }

    if (!val.type.match(expectedType))
        error("Incompatible types for assignment; expected " +
                                 expectedType.toString() + " but got " +
                                 val.type.toString() + " for member: " + node->strValue);

    if (isModify) {
        env->modify(node->strValue, val);
        env->popSelfRef();
    } else {
        env->set(node->strValue, val);
    }

    return val;
}

template<typename T>
TypedValue Executor::readOnArray(std::shared_ptr<T> arr, const std::string &property) {
    if (property == "length") return TypedValue(static_cast<int>(arr->elements.size()));
    error("Unknown array property: " + property);
}

TypedValue Executor::readOnStruct(const std::shared_ptr<Struct> &str, const std::string &property) {
    auto it = std::find_if(str->fields.begin(), str->fields.end(),
        [&property](const auto& pair){ return pair.first == property; });

    if (it == str->fields.end()) {
        error("Struct does not have field: " + property);
    }

    return it->second;
}

TypedValue Executor::handleReadAssignment(
    std::shared_ptr<ParsedASTNode> readNode,
    ENV env,
    std::shared_ptr<ParsedASTNode> valNode
) {
    if (readNode->type != ASTNode::Type::READ)
        error("Expected READ node for member assignment");

    TypedValue parentVal = evaluateExpression(readNode->children[0], env);
    const std::string &prop = readNode->children[1]->strValue;
    TypedValue val = evaluateExpression(valNode, env);

    switch(parentVal.type.kind) {
        case BaseType::Struct: {
            auto str = parentVal.get<std::shared_ptr<Struct>>();
            auto it = std::find_if(str->fields.begin(), str->fields.end(),
                [&prop](const auto &pair){ return pair.first == prop; });
            if (it == str->fields.end())
                error("Struct does not have field: " + prop);

            it->second = val;
            break;
        }
        case BaseType::Array: {
            auto arr = parentVal.get<std::shared_ptr<Array>>();
            if(prop == "length")
                error("Cannot modify array length");
            error("Cannot modify array elements");
            break;
        }
        default:
            error("Cannot assign to non-object property");
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
        case BaseType::String: {
            auto str = target.get<std::string>();
            if (property == "length") return TypedValue(static_cast<int>(str.size()));
            error("Unknown string property: " + property);
        }
        default:
            error("Attempted READ on non-object");
    }
}

TypedValue Executor::evalBinaryStringOp(BinaryOp op, const TypedValue &lhs, const TypedValue &rhs) {
    switch (op) {
        case PLUS: {
            if(lhs.type.kind == BaseType::Int) {
                const int L = getIntValue(lhs);
                const int R = getIntValue(rhs);
                return TypedValue(L + R);
            }
            std::ostringstream out;
            out << lhs.get<std::string>();
            printValue(&out, rhs);
            return TypedValue(out.str());
        }
        case MULTIPLY: {
            if(lhs.type.kind == BaseType::Int) {
                const int L = getIntValue(lhs);
                const int R = getIntValue(rhs);
                return TypedValue(L * R);
            }
            const int amt = getIntValue(rhs);
            const std::string base = getStringValue(lhs);
            std::ostringstream out;
            for (int i = 0; i < amt; ++i) out << base;
            return TypedValue(out.str());
        }
        case NOT_EQUAL:
        case COMPARISON: {
            std::ostringstream left, right;
            printValue(&left, lhs);
            printValue(&right, rhs);
            if(op == NOT_EQUAL) return TypedValue(left.str() != right.str());
            else return TypedValue(left.str() == right.str());
        }
        default: break;
    }
    error("Unsupported string binary op");
}

TypedValue Executor::evalBinaryArithmeticOp(BinaryOp op, const TypedValue &lhs, const TypedValue &rhs) {
    const int L = getIntValue(lhs);
    const int R = getIntValue(rhs);

    switch (op) {
        case MINUS:         return TypedValue(L - R);
        case DIVIDE:        return TypedValue(L / R);
        case MODULUS:       return TypedValue(L % R);
        case BITWISE_NOT:   return TypedValue(~L);
        case BITWISE_AND:   return TypedValue(L & R);
        case BITWISE_OR:    return TypedValue(L | R);
        case BITWISE_XOR:   return TypedValue(L ^ R);
        case LESS:          return TypedValue(L <  R);
        case GREATER:       return TypedValue(L >  R);
        case LESS_EQUAL:    return TypedValue(L <= R);
        case GREATER_EQUAL: return TypedValue(L >= R);
        default: break;
    }
    error("Unsupported arithmetic binary op");
}

TypedValue Executor::evalBinaryBoolOp(BinaryOp op, const TypedValue &lhs, const TypedValue &rhs) {
    const bool L = getBoolValue(lhs);
    const bool R = getBoolValue(rhs);

    switch (op) {
        case COMPARISON:    return TypedValue(L == R);
        case NOT:           return TypedValue(!L);
        case AND:           return TypedValue(L && R);
        case OR:            return TypedValue(L || R);
        default: break;
    }
    error("Unsupported boolean binary op");
}