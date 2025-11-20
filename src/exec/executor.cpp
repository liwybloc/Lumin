#include "executor.hpp"
#include "executils.hpp"
#include "outstream.hpp"
#include "filestream.hpp"
#include <iostream>
#include <optional>

template<typename T, typename ArrayType>
void Executor::printArray(const std::shared_ptr<ArrayType> &arr) {
    std::cout << "[";
    for (size_t i = 0; i < arr->elements.size(); ++i) {
        std::cout << *arr->elements[i];
        if (i + 1 < arr->elements.size()) std::cout << ", ";
    }
    std::cout << "]";
}

void Executor::printStruct(const std::shared_ptr<Struct> &_struct) {
    std::cout << _struct->name << "{";
    int idx = 0;
    for (auto &[name, val] : _struct->fields) {
        std::cout << name << ": "; printValue(val);
        if (++idx < _struct->fields.size()) std::cout << ", ";
    }
    std::cout << "}";
}

template<typename ElemType, typename ArrayType>
void printArrayHelper(const TypedValue &val) {
    printArray<ElemType, ArrayType>(val.get<std::shared_ptr<ArrayType>>());
}

void Executor::printValue(const TypedValue &val) {
    switch(val.type.kind) {
        case BaseType::Int: std::cout << val.get<int>(); break;
        case BaseType::Bool: std::cout << (val.get<bool>() ? "true" : "false"); break;
        case BaseType::String: std::cout << val.get<std::string>(); break;
        case BaseType::Function: std::cout << "[function]"; break;
        case BaseType::NIL: std::cout << "nil"; break;

        case BaseType::ArrayInt: printArray<int, IntArray>(val.get<std::shared_ptr<IntArray>>()); break;
        case BaseType::ArrayBool: printArray<bool, BoolArray>(val.get<std::shared_ptr<BoolArray>>()); break;
        case BaseType::ArrayString: printArray<std::string, StringArray>(val.get<std::shared_ptr<StringArray>>()); break;

        case BaseType::Struct: printStruct(val.get<std::shared_ptr<Struct>>()); break;
        case BaseType::ExportData: std::cout << "[file data of " << val.get<std::shared_ptr<ExportData>>()->fileName << "]"; break;

        default: throw std::runtime_error("Unknown type in printValue");
    }
}

TypedValue Executor::run() {
    executeNode(root, globalEnv);
    if (globalEnv->has("main")) {
        auto mainFunc = globalEnv->get("main");
        if (!mainFunc.type.match(BaseType::Function))
            throw std::runtime_error("main is not a function type - received " + mainFunc.type.toString());
        return *std::get<std::shared_ptr<Function>>(mainFunc.value)->fn({});
    }
    return TypedValue(0);
}

const std::unordered_map<std::string, std::function<void(ENV, Executor*)>>& getImportMaps() {
    static const auto maps = []{
        std::unordered_map<std::string, std::function<void(ENV, Executor*)>> m;
        m["outstream"] = [](ENV e, Executor* exec){ addOutstream(e, exec); };
        m["filestream"] = [](ENV e, Executor* exec){ addFilestream(e, exec); };
        m["monsterzeroultra"] = [](ENV e, Executor* exec) { printf("⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣤⠶⣶⣤⣔⣶⡶⣦⣤⣠⣶⡄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n⠀⠀⠀⠀⠀⠀⠀⠀⢀⣴⣯⡏⠉⣴⣿⢿⢿⣿⢷⣶⣍⡻⣇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n⠀⠀⠀⠀⢠⡶⢤⣴⣿⠟⠀⢁⡞⠛⡟⢣⠐⢳⡀⠈⢻⣯⡺⢦⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n⠀⠀⠀⠀⢸⣇⣾⣟⠎⠀⢠⡞⠀⡀⢀⠆⠀⠀⣧⠀⡀⠹⡳⣦⣿⡦⣄⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n⠀⠀⠀⠀⡸⣻⣟⡟⠀⠀⢸⠁⢀⢧⢸⡾⡀⠶⣿⡿⣳⣅⣼⢻⠙⢿⡶⢤⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n⠀⠀⠀⠞⠉⡿⢡⢡⠀⠀⣿⠀⣿⣯⣭⣹⢝⡊⢸⠚⢻⠝⢺⣗⢆⣸⡷⣄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n⠀⠀⠀⠀⢠⢳⣮⣣⣤⣄⡟⣇⡿⣻⡿⣿⠉⠻⣜⠐⠿⣛⡻⠿⠿⠵⡳⣿⣧⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n⠀⠀⠀⢀⡿⢻⢡⡿⣼⡃⢡⠈⣴⠈⠒⠋⠀⠀⠐⠀⠀⠀⠌⡟⡦⣌⡉⠻⡉⢣⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n⠀⠀⠀⢸⠃⢹⣾⠀⢹⣷⡜⣷⣿⣧⠀⠀⢴⠞⠛⣿⠀⢀⢧⡜⡟⠀⠉⠓⢄⡀⠈⠓⠤⣀⠀⠀⠀⠀⠀⠀⠀\n⠀⠀⠀⠈⠀⢸⢹⡆⢸⣿⡛⡷⣝⣽⠳⢦⣌⡓⠀⣢⠔⡡⢻⡋⠀⠀⠀⠀⠀⠑⢦⡀⠀⠀⠑⠢⣄⠀⠀⠀⠀\n⠀⠀⠀⠀⠀⠈⣼⢇⡼⠿⣿⣿⡿⣿⡄⠀⠀⢹⢉⡰⠛⠁⠈⠁⠀⠀⠀⠀⠀⠀⠀⠙⢶⡀⠀⠀⠀⠙⠢⡀⠀\n⠀⠀⠀⠀⠀⢏⣞⠝⠀⠀⠸⣿⣿⡺⣮⣤⢀⣘⠓⠶⣶⡔⢶⣖⢤⡤⠤⠤⠠⣖⢲⣦⡤⠽⢦⡀⠀⠀⠀⠘⣆\n⠀⠀⠀⢀⢴⣾⡾⠀⠀⠀⢀⠚⣿⣷⡈⠻⣆⡈⠁⢁⣈⣇⠘⣿⣄⠀⠀⠀⠀⢸⠈⡿⡇⠀⠀⠉⠀⠀⠀⠀⢸\n⠀⠀⠀⡎⣫⠟⡇⠀⠀⠀⠘⠀⠈⠻⣿⣶⣾⣽⣲⣗⠋⢻⣦⣿⠀⠀⠀⠀⠀⢠⣤⣆⡧⠤⠤⠄⠤⠐⠒⠂⠁\n⠀⠀⢀⢾⢯⡎⠀⠀⠀⠀⠀⣴⠁⠀⠀⠉⠙⢟⠻⠿⣿⣼⡟⣬⠂⣧⠴⠐⠂⠚⠋⠛⠀⠀⠀⠀⠀⠀⠀⠀⠀\n⠀⠀⣸⡼⢣⣿⡖⠒⠂⠀⠒⡳⠀⠀⠀⠀⠀⠈⠑⠚⠓⣟⣉⣳⠀⢱⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n⠀⠀⣿⡧⢺⢻⣿⣿⣓⣲⣾⠇⠀⠀⠀⠀⠀⠀⠀⠀⠀⡇⡇⠫⣆⠸⠄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n⠀⠀⡻⡅⠊⣶⡇⠀⠀⠀⢸⡆⠀⠀⠀⠀⠀⠀⠀⠀⢀⡇⠀⠀⠸⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n⠀⠀⢻⡿⡾⠈⡇⠀⠀⠀⢸⢇⠀⠀⠀⠀⠀⠀⠀⠀⠘⣴⡀⠀⠀⢸⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n⠀⠀⠰⣝⠿⣐⡏⠀⠀⠀⣼⡸⠀⠀⠀⠀⠀⠀⠀⠀⠀⢣⠱⡄⠀⡟⣷⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n⠀⠀⠀⠙⣿⣿⣯⠀⠀⠀⡟⡇⠀⠀⠀⠀⠀⣀⣀⡀⠤⠬⢆⡿⣄⣃⡈⡄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n⠀⠀⠀⠀⠹⣿⣿⠀⠀⠀⢱⣷⠈⠉⠉⠉⠉⠀⠀⠀⠀⠀⠘⠁⢸⠧⠞⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n⠀⠀⠀⠀⠀⠈⠯⣆⠀⢀⡴⠁⠀⠀⠀⠀⠀⠀⠀⠀⢀⠆⠀⢀⡎⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n⠀⠀⠀⠀⠀⠀⠀⠈⣳⠟⣦⡀⠀⠀⠀⠀⠀⠀⠀⠀⡿⠀⠀⢸⡃⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n⠀⠀⠀⠀⠀⠀⢠⡾⢇⠀⠀⠉⠓⠲⠤⠤⠤⠤⠤⠤⠤⠲⢲⣻⡁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n⠀⠀⠀⠀⠀⣰⠏⠀⡸⠓⢵⣀⡀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣨⣼⣇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n⠀⠀⠀⢀⣾⠛⢀⡞⠁⠀⠀⡞⠉⠉⠉⠁⡖⠀⠈⠹⡍⠉⠈⠙⣞⣆⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n⠀⢠⡴⡟⠉⠀⡾⠀⠀⠀⡸⠀⠀⠀⠀⢠⠇⠀⠀⠀⡇⠀⠀⠀⠘⡜⢦⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n⢸⣿⡽⠀⠀⡰⠁⠀⠀⢰⠃⠀⠀⠀⠀⣼⠄⠀⠀⠀⢱⡄⠀⠀⠀⠹⣄⢳⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n⢨⣿⣄⠀⣴⠃⠀⠀⢀⡏⠀⠀⠀⠀⠀⣯⠀⠀⠀⠀⢸⣳⠀⠀⠀⠀⠙⣦⢝⣶⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n⠀⠻⢿⣿⣥⣀⡀⠀⡼⠀⠀⠀⠀⠀⠀⣇⠀⠀⠀⠀⠀⡟⢇⠀⢀⠀⣀⣌⣿⡃⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n⠀⠀⠸⠟⠙⢻⣿⣶⣿⣿⣷⣶⣤⣀⣰⡿⣿⣶⣶⣦⣤⣼⣾⠿⠛⠛⠋⠉⠉⢳⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n⠀⠀⠀⠀⠀⠀⢧⠉⠙⠁⠈⠉⠛⠛⠚⠛⢀⡇⠉⢯⠙⠉⠁⠀⠀⠀⠀⠀⠀⠀⢣⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n⠀⠀⠀⠀⠀⠀⠸⡄⠀⠀⠀⠀⠀⠀⠀⠀⠰⠂⠀⠀⢧⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢧⠀⠀⠀⠀⠀⠀⠀⠀⠀\n"); };
        return m;
    }();
    return maps;
}

void Executor::handleImports(std::vector<std::shared_ptr<ASTNode>> children, ENV env) {
    const auto &maps = getImportMaps();
    for (auto &child : children) {
        const std::string &name = child->strValue;
        if (name.ends_with(".lum")) {
            if (!exportData.contains(name)) {
                if (!pragmas.contains(name)) throw std::runtime_error("Unknown pragma: " + name);
                if (std::find(handlingModules.begin(), handlingModules.end(), name) != handlingModules.end())
                    throw std::runtime_error("Circular import: " + name);
                executePragma(pragmas[name], std::make_shared<Environment>());
            }
            env->set(child->children[0]->strValue, exportData[name]);
            continue;
        }
        if (!maps.contains(name)) throw std::runtime_error("Unknown module: " + name);
        maps.at(name)(env, this);
    }
}

void Executor::executePragma(std::shared_ptr<ASTNode> node, std::shared_ptr<Environment> env) {
    handlingModules.push_back(node->strValue);
    auto &children = node->children;
    handleImports(children[0]->children, env);
    exportData[node->strValue] = std::make_shared<ExportData>(node->strValue);

    for (size_t i = 2; i < children.size(); ++i)
        executeNode(children[i], env);

    for (auto &exportNode : children[1]->children) {
        std::string var = exportNode->strValue;
        if (!env->has(var)) throw std::runtime_error("Cannot export undefined variable: " + var);
        exportData[node->strValue]->addExport(var, env);
    }

    handlingModules.pop_back();
}

void Executor::executePragmas(std::vector<std::shared_ptr<ASTNode>> children, std::shared_ptr<Environment> env) {
    for (auto &child : children) pragmas[child->strValue] = child;
    executePragma(children.back(), env);
}

ReturnValue Executor::executeFunctionDefinition(
        std::shared_ptr<ASTNode> node,
        std::shared_ptr<Environment> env,
        bool extractSignature)
{
    std::vector<TypedIdentifier> params;
    for (size_t i = 0; i < node->children.size() - 1; ++i) {
        auto c = node->children[i];
        auto c0 = c->children[0];
        auto primVal = c0->primitiveValue;
        auto strVal = c0->strValue;
        params.push_back({ c->strValue, primVal == Primitive::NONE ? Type(strVal)
                                                                   : Type(primVal) });
    }

    Type retType =
        node->primitiveValue == Primitive::NONE
            ? (node->retType == "nil" ? Type(BaseType::NIL) : Type(node->retType))
            : Type(node->primitiveValue);

    if (!extractSignature) {
        env->set(
            node->strValue,
            createFunction(params, node->children.back(), retType, env)
        );
        return {};
    }

    ReturnValue r;
    r.special = std::make_any<std::pair<std::vector<TypedIdentifier>, Type>>(
        std::make_pair(params, retType)
    );
    return r;
}


ReturnValue Executor::executeNode(std::shared_ptr<ASTNode> node, std::shared_ptr<Environment> env, bool extraBit) {
    switch (node->type) {
        case ASTNode::Type::PROGRAM: executePragmas(node->children, env); return {};
        case ASTNode::Type::BLOCK: return executeBlock(node->children, std::make_shared<Environment>(env));
        case ASTNode::Type::STRUCT_DECLARE: handleStructDeclaration(node, env); return {};
        case ASTNode::Type::EXPRESSION_STATEMENT: evaluateExpression(node->children[0], env); return {};
        case ASTNode::Type::PRIMITIVE_ASSIGNMENT: handleAssignment(node, env, Type(node->primitiveValue), false); return {};
        case ASTNode::Type::STRUCT_ASSIGNMENT: handleStructAssignment(node, env); return {};
        case ASTNode::Type::RETURN_STATEMENT:
            return ReturnValue(node->children.empty() ? TypedValue() : evaluateExpression(node->children[0], env));
        case ASTNode::Type::IF_STATEMENT: {
            bool cond = getBoolValue(evaluateExpression(node->children[0], env));
            if (cond) return executeNode(node->children[1], env);
            if (node->children.size() > 2 && node->children[2]->type == ASTNode::Type::ELSE_STATEMENT)
                return executeNode(node->children[2]->children[0], env);
            return {};
        }
        case ASTNode::Type::WHILE_STATEMENT:
            while (getBoolValue(evaluateExpression(node->children[0], env))) {
                auto r = executeNode(node->children[1], env);
                if (r.hasReturn) return r;
            }
            return {};
        case ASTNode::Type::FOR_STATEMENT:
            executeNode(node->children[0], env);
            while (getBoolValue(evaluateExpression(node->children[1], env))) {
                auto r = executeNode(node->children[3], env);
                if (r.hasReturn) return r;
                evaluateExpression(node->children[2], env);
            }
            return {};
        case ASTNode::Type::FUNCTION: {
            std::vector<TypedIdentifier> params;
            for (size_t i = 0; i < node->children.size() - 1; ++i) {
                auto c = node->children[i];
                auto c0 = c->children[0];
                auto primVal = c0->primitiveValue;
                auto strVal = c0->strValue;
                params.push_back({ c->strValue, primVal == Primitive::NONE ? Type(strVal) : Type(primVal) });
            }
            Type retType = node->primitiveValue == Primitive::NONE ? node->retType == "nil" ? Type(BaseType::NIL) : Type(node->retType) : Type(node->primitiveValue);
            if(!extraBit) {
                env->set(node->strValue, createFunction(params, node->children.back(), retType, env));
                return {};
            }
            ReturnValue ret{};
            ret.special = std::make_shared<std::any>(std::make_pair(params, retType));
            return ret;
        }
        case ASTNode::Type::NATIVE_STATEMENT: {
            auto funcData = std::any_cast<std::pair<std::vector<TypedIdentifier>, Type>>(executeNode(node->children[0], env, true).special);
            
        }
        default:
            return evaluateExpression(node, env);
    }
}

ReturnValue Executor::executeBlock(const std::vector<std::shared_ptr<ASTNode>> &nodes, std::shared_ptr<Environment> env) {
    for (auto &n : nodes) {
        auto r = executeNode(n, env);
        if (r.hasReturn) return r;
    }
    return {};
}

template<typename T, typename ArrayType>
TypedValue Executor::arrayOperation(const std::shared_ptr<ArrayType>& arr, const std::vector<int>& indices) {
    if (indices.size() == 1) return TypedValue(*arr->elements[indices[0]]);
    auto result = std::make_shared<ArrayType>();
    for (int idx : indices) result->add(std::make_shared<T>(*arr->elements[idx]));
    return TypedValue(result);
}

template<typename T, typename ArrayType>
TypedValue Executor::arrayOperation(
    const std::shared_ptr<ArrayType>& arr,
    const std::vector<int>& indices,
    std::shared_ptr<ASTNode> valNode,
    std::shared_ptr<Environment> env
) {
    TypedValue val = evaluateExpression(valNode, env);
    std::vector<T> valuesToAssign;

    if constexpr (std::is_same_v<T, int>) {
        if (val.type.match(BaseType::ArrayInt)) {
            const auto valArr = val.get<std::shared_ptr<IntArray>>();
            for (const auto &el : valArr->elements) valuesToAssign.push_back(*el);
        } else {
            valuesToAssign.push_back(getIntValue(val));
        }
    } else if constexpr (std::is_same_v<T, bool>) {
        if (val.type.match(BaseType::ArrayBool)) {
            const auto valArr = val.get<std::shared_ptr<BoolArray>>();
            for (const auto &el : valArr->elements) valuesToAssign.push_back(*el);
        } else {
            valuesToAssign.push_back(getBoolValue(val));
        }
    } else if constexpr (std::is_same_v<T, std::string>) {
        if (val.type.match(BaseType::ArrayString)) {
            const auto valArr = val.get<std::shared_ptr<StringArray>>();
            for (const auto &el : valArr->elements) valuesToAssign.push_back(*el);
        } else {
            valuesToAssign.push_back(getStringValue(val));
        }
    }

    for (size_t i = 0; i < indices.size(); ++i) {
        const int idx = indices[i];
        const T valueToSet =
            i < valuesToAssign.size() ? valuesToAssign[i] : valuesToAssign.back();
        *arr->elements[idx] = valueToSet;
    }

    return TypedValue(arr);
}

std::shared_ptr<Function> Executor::createFunction(
    const std::vector<TypedIdentifier> &params,
    std::shared_ptr<ASTNode> body,
    Type returnType,
    std::shared_ptr<Environment> closureEnv
) {
    return std::make_shared<Function>(Function{
        [this, params, body, closureEnv, returnType](const std::vector<std::shared_ptr<TypedValue>> &args){
            auto local = std::make_shared<Environment>(closureEnv);
            for (size_t i = 0; i < params.size() && i < args.size(); ++i) {
                if(!params[i].type.match(args[i]->type))
                local->set(params[i].ident, *args[i]);
            }
            ReturnValue r = executeNode(body, local);
            if(!returnType.match(r.value.type)) {
                throw std::runtime_error("Function return type mismatch - got " + r.value.type.toString() + " but expected " + returnType.toString());
            }
            return std::make_shared<TypedValue>(r.hasReturn ? r.value : TypedValue());
        }
    });
}

TypedValue Executor::evaluateExpression(std::shared_ptr<ASTNode> node, std::shared_ptr<Environment> env) {
    auto eval = [this, &env](std::shared_ptr<ASTNode> n){ return evaluateExpression(n, env); };

    switch (node->type) {
        case ASTNode::Type::NUMBER: return TypedValue(std::stoi(node->strValue));
        case ASTNode::Type::BOOL: return TypedValue(node->strValue == "1");
        case ASTNode::Type::STRING: return TypedValue(node->strValue);
        case ASTNode::Type::IDENTIFIER: return env->get(node->strValue);
        case ASTNode::Type::SELF_REFERENCE:
            if (env->hasSelfRef()) return env->currentSelfRef();
            return TypedValue();

        case ASTNode::Type::PRIMITIVE_ASSIGNMENT: return handleAssignment(node, env, Type(node->primitiveValue), true);
        case ASTNode::Type::STRUCT_ASSIGNMENT: return handleStructAssignment(node, env);
        case ASTNode::Type::NDARRAY_ASSIGN: return handleNDArrayAssignment(node, env);

        case ASTNode::Type::ARRAY_ACCESS: {
            TypedValue arrVal = eval(node->children[0]);
            auto idxNode = node->children[1];
            switch(arrVal.type.kind) {
                case BaseType::ArrayInt: {
                    auto arr = arrVal.point<IntArray>();
                    return arrayOperation<int, IntArray>(arr, getIndices(arr, idxNode, env));
                }
                case BaseType::ArrayBool: {
                    auto arr = arrVal.point<BoolArray>();
                    return arrayOperation<bool, BoolArray>(arr, getIndices(arr, idxNode, env));
                }
                case BaseType::ArrayString: {
                    auto arr = arrVal.point<StringArray>();
                    return arrayOperation<std::string, StringArray>(arr, getIndices(arr, idxNode, env));
                }
                default: throw std::runtime_error("Attempted array access on non-array");
            }
        }

        case ASTNode::Type::ARRAY_ASSIGN: {
            TypedValue arrVal = eval(node->children[0]);
            auto idxNode = node->children[1];
            auto valNode = node->children[2];
            std::visit(overloaded{
                [this, &idxNode, &env, &valNode](std::shared_ptr<IntArray> &arr) { arrayOperation<int, IntArray>(arr, getIndices(arr, idxNode, env), valNode, env); },
                [this, &idxNode, &env, &valNode](std::shared_ptr<BoolArray> &arr) { arrayOperation<bool, BoolArray>(arr, getIndices(arr, idxNode, env), valNode, env); },
                [this, &idxNode, &env, &valNode](std::shared_ptr<StringArray> &arr) { arrayOperation<std::string, StringArray>(arr, getIndices(arr, idxNode, env), valNode, env); },
                [](auto&) { throw std::runtime_error("Attempted array assignment on non-array"); }
            }, arrVal.value);
            return arrVal;
        }

        case ASTNode::Type::ARRAY_LITERAL: {
            auto arr = std::make_shared<IntArray>();
            for (auto &child : node->children) {
                if (child->type == ASTNode::Type::RANGE) {
                    int start = getIntValue(eval(child->children[0]));
                    int end   = getIntValue(eval(child->children[1]));
                    for (int i = start; i <= end; ++i) {
                        env->pushSelfRef(i); arr->add(std::make_shared<int>(i)); env->popSelfRef();
                    }
                } else {
                    int val = getIntValue(eval(child));
                    env->pushSelfRef(val); arr->add(std::make_shared<int>(val)); env->popSelfRef();
                }
            }
            return arr;
        }

        case ASTNode::Type::CALL: {
            auto calleeVal = eval(node->children[0]);
            if (!calleeVal.type.match(BaseType::Function))
                throw std::runtime_error("Attempted to call a non-function value");
            std::vector<std::shared_ptr<TypedValue>> args;
            for (size_t i = 1; i < node->children.size(); ++i) args.push_back(std::make_shared<TypedValue>(eval(node->children[i])));
            return *calleeVal.get<std::shared_ptr<Function>>()->fn(args);
        }

        case ASTNode::Type::BINARY_OP: {
            int left = getIntValue(eval(node->children[0]));
            int right = getIntValue(eval(node->children[1]));
            switch(node->binopValue) {
                case PLUS:          return TypedValue(left + right);
                case MINUS:         return TypedValue(left - right);
                case MULTIPLY:      return TypedValue(left * right);
                case DIVIDE:        return TypedValue(left / right);
                case MODULUS:       return TypedValue(left % right);
                case COMPARISON:    return TypedValue(left == right);
                case LESS:          return TypedValue(left < right);
                case GREATER:       return TypedValue(left > right);
                case LESS_EQUAL:    return TypedValue(left <= right);
                case GREATER_EQUAL: return TypedValue(left >= right);
                default: throw std::runtime_error("Unsupported binary op");
            }
        }

        case ASTNode::Type::UNARY_OP: {
            int val = getIntValue(eval(node->children[0]));
            switch(node->binopValue) {
                case MINUS: return TypedValue(-val);
                case BITWISE_NOT: return TypedValue(~val);
                case NOT: return TypedValue(!val);
                default: throw std::runtime_error("Unsupported unary op");
            }
        }

        case ASTNode::Type::READ: {
            TypedValue target = eval(node->children[0]);
            return evaluateReadProperty(target, node->children[1]->strValue);
        }

        case ASTNode::Type::SIZED_ARRAY_DECLARE: {
            int size = getIntValue(eval(node->children[0]));
            int val = getIntValue(primitiveValue(node->primitiveValue));
            auto arr = std::make_shared<IntArray>();
            for (int i = 0; i < size; ++i) arr->add(std::make_shared<int>(val));
            return arr;
        }

        default:
            throw std::runtime_error("Unsupported expression type: " + std::to_string(static_cast<int>(node->type)));
    }
}