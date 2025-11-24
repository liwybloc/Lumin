#include "executor.hpp"
#include "executils.hpp"
#include "outstream.hpp"
#include "filestream.hpp"
#include <iostream>
#include <optional>
#include <parserutils.hpp>

void Executor::printArray(std::ostream *out, const std::shared_ptr<Array> &arr) {
    *out << "[";
    for (size_t i = 0; i < arr->elements.size(); ++i) {
        printValue(out, arr->elements[i]);
        if (i + 1 < arr->elements.size()) {
            *out << ", ";
        }
    }
    *out << "]";
}

void Executor::printStruct(std::ostream *out, const std::shared_ptr<Struct> &st) {
    *out << st->name << "{";
    int idx = 0;
    for (const auto &[name, val] : st->fields) {
        *out << name << ": ";
        if(val.type.kind == BaseType::String) {
            *out << "\"";
            printValue(out, val);
            *out << "\"";
        } else {
            printValue(out, val);
        }
        if (++idx < st->fields.size()) {
            *out << ", ";
        }
    }
    *out << "}";
}

void Executor::printValue(std::ostream *out, const TypedValue &val) {
    switch (val.type.kind) {
        case BaseType::Int:
            *out << val.get<int>();
            return;
        case BaseType::Bool:
            *out << (val.get<bool>() ? "true" : "false");
            return;
        case BaseType::String:
            *out << val.get<std::string>();
            return;
        case BaseType::Function:
            *out << "[function]";
            return;
        case BaseType::NIL:
            *out << "nil";
            return;
        case BaseType::Array:
            printArray(out, val.get<std::shared_ptr<Array>>());
            return;
        case BaseType::Struct:
            printStruct(out, val.get<std::shared_ptr<Struct>>());
            return;
        case BaseType::ExportData:
            *out << "[file data of " << val.get<std::shared_ptr<ExportData>>()->fileName << "]";
            return;
        default:
            error("Unknown type in printValue");
    }
}

TypedValue Executor::run() {
    executeNode(root, globalEnv);
    if (globalEnv->has("main")) {
        auto mainFunc = globalEnv->get("main");
        if (!mainFunc.type.match(BaseType::Function))
            error("main is not a function type - received " + mainFunc.type.toString());
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

void Executor::handleImports(std::vector<std::shared_ptr<ParsedASTNode>> children, ENV env) {
    const auto &maps = getImportMaps();
    for (auto &child : children) {
        const std::string &name = child->strValue;
        if (name.ends_with(".lum")) {
            if (!exportData.contains(name)) {
                if (!pragmas.contains(name)) error("Unknown pragma: " + name);
                if (std::find(handlingModules.begin(), handlingModules.end(), name) != handlingModules.end())
                    error("Circular import: " + name);
                executePragma(pragmas[name], std::make_shared<Environment>());
            }
            env->set(child->children[0]->strValue, exportData[name]);
            continue;
        }
        if (!maps.contains(name)) error("Unknown module: " + name);
        maps.at(name)(env, this);
    }
}

void Executor::executePragma(std::shared_ptr<ParsedASTNode> node, ENV env) {
    handlingModules.push_back(node->strValue);
    auto &children = node->children;
    handleImports(children[0]->children, env);
    exportData[node->strValue] = std::make_shared<ExportData>(node->strValue);

    for (size_t i = 2; i < children.size(); ++i)
        executeNode(children[i], env);

    for (auto &exportNode : children[1]->children) {
        std::string var = exportNode->strValue;
        if (!env->has(var)) error("Cannot export undefined variable: " + var);
        exportData[node->strValue]->addExport(var, env);
    }

    handlingModules.pop_back();
}

void Executor::executePragmas(std::vector<std::shared_ptr<ParsedASTNode>> children, ENV env) {
    for (auto &child : children) pragmas[child->strValue] = child;
    executePragma(children.back(), env);
}

FunctionData Executor::executeFunctionDefinition(
        std::shared_ptr<ParsedASTNode> node,
        ENV env)
{
    std::vector<Parameter> params;
    for (size_t i = 0; i < node->children.size() - 1; ++i) {
        auto c = node->children[i];
        auto c0 = c->children[0];
        auto primVal = c0->primitiveValue;
        auto strVal = c0->strValue;


        Parameter param = {
            c->strValue,
            primVal == Primitive::NONE ? Type(strVal) : Type(primVal),
            c->children.size() > 1 && c->children[1]->type == ASTNode::Type::ARRAY_ASSIGN
        };
        params.push_back(param);
    }

    Type retType =
        node->primitiveValue == Primitive::NONE
            ? (node->retType == "nil" ? Type(BaseType::NIL) : Type(node->retType))
            : Type(node->primitiveValue);

    return std::make_shared<_FunctionData>(params, retType, node->children.back());
}

ReturnValue Executor::executeNode(std::shared_ptr<ParsedASTNode> node, ENV env, bool extraBit) {
    switch (node->type) {
        case ASTNode::Type::CONTINUE: return ReturnValue(true, false);
        case ASTNode::Type::BREAK: return ReturnValue(false, true);
        case ASTNode::Type::PROGRAM: executePragmas(node->children, env); return {};
        case ASTNode::Type::BLOCK: return executeBlock(node->children, std::make_shared<Environment>(env));
        case ASTNode::Type::STRUCT_DECLARE: handleStructDeclaration(node, env); return {};
        case ASTNode::Type::PRIMITIVE_ASSIGNMENT: handleAssignment(node, env, node->primitiveValue, true); return {};
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
                if (r.hasBreak) break;
            }
            return {};
        case ASTNode::Type::FOR_STATEMENT: {
            auto localEnv = std::make_shared<Environment>(env);
            if (node->strValue == "0") {
                executeNode(node->children[0], localEnv);
                while (getBoolValue(evaluateExpression(node->children[1], localEnv))) {
                    auto r = executeNode(node->children[3], localEnv);
                    if (r.hasReturn) return r;
                    if (r.hasBreak) break;
                    evaluateExpression(node->children[2], localEnv);
                }
                return {};
            }

            const auto &varDecl = node->children[0];
            const auto &iterableExpr = node->children[1];
            const auto &body = node->children[2];

            const auto iterableValue = evaluateExpression(iterableExpr, localEnv);
            if(iterableValue.type.kind != BaseType::Array) {
                error("Expected array for enhanced for loop");
            }
            for (const auto &item : iterableValue.point<Array>()->elements) {
                localEnv->set(varDecl->strValue, item);
                auto r = executeNode(body, localEnv);
                if (r.hasReturn) return r;
            }
            return {};
        }

        case ASTNode::Type::FUNCTION: {
            auto funcData = executeFunctionDefinition(node, env);
            auto func = createFunction(funcData, env);
            env->set(node->strValue, func);
            return TypedValue(func);
        }
        case ASTNode::Type::NATIVE_STATEMENT: {
            auto funcData = executeFunctionDefinition(node->children[0], env);
            auto nativeFunc = createNativeFunction(node->strValue, funcData, env);
            env->set(node->strValue, nativeFunc);
            return {};
        }
        default: evaluateExpression(node, env); return {};
    }
}

ReturnValue Executor::executeBlock(const std::vector<std::shared_ptr<ParsedASTNode>> &nodes, ENV env) {
    for (auto &n : nodes) {
        auto r = executeNode(n, env);
        if (r.hasReturn || r.hasBreak || r.hasContinue) return r;
    }
    return {};
}

TypedValue Executor::arrayOperation(const std::shared_ptr<Array>& arr, const std::vector<int>& indices) {
    if (indices.size() == 1) return arr->elements[indices[0]];

    auto result = std::make_shared<Array>();
    result->elementType = arr->elementType; // preserve element type
    for (int idx : indices) {
        result->elements.push_back(arr->elements[idx]);
    }
    return TypedValue(result, result->elementType.array());
}

TypedValue Executor::arrayOperation(
    const std::shared_ptr<Array>& arr,
    const std::vector<int>& indices,
    std::shared_ptr<ParsedASTNode> valNode,
    ENV env
) {
    TypedValue val = evaluateExpression(valNode, env);
    std::vector<TypedValue> valuesToAssign;

    if (val.type.match(BaseType::Array)) {
        auto valArr = val.get<std::shared_ptr<Array>>();
        valuesToAssign = valArr->elements;
    } else {
        valuesToAssign.push_back(val);
    }

    for (size_t i = 0; i < indices.size(); ++i) {
        const int idx = indices[i];
        const TypedValue &valueToSet =
            i < valuesToAssign.size() ? valuesToAssign[i] : valuesToAssign.back();
        arr->elements[idx] = valueToSet;
    }

    return TypedValue(arr, arr->elementType.array());
}

void Executor::parseArg(std::shared_ptr<Environment> env, Parameter* param, std::shared_ptr<TypedValue> arg, int i) {
    if(!param->type.match(arg->type))
        error("Expected type " + param->type.toString() + " but got " + arg->type.toString());
    env->set(param->ident, *arg);
}

std::shared_ptr<Function> Executor::createFunction(
    FunctionData funcData,
    ENV closureEnv
) {
    return std::make_shared<Function>(Function{
        [this, funcData, closureEnv](const std::vector<std::shared_ptr<TypedValue>> &args) {
            auto local = std::make_shared<Environment>(closureEnv);

            if(args.size() < funcData->params.size()) {
                error("Too few arguments provided for function");
            }

            for (size_t i = 0; i < args.size(); ++i) {
                if (i >= funcData->params.size()) {
                    error("Too many arguments provided for function");
                }

                auto &param = funcData->params[i];
                if (param.vararg) {
                    auto varargArray = std::make_shared<Array>();
                    varargArray->elementType = param.type;

                    while (i < args.size()) {
                        if (!param.type.match(args[i]->type)) {
                            error(
                                "Expected type " + param.type.toString() + 
                                " but got " + args[i]->type.toString()
                            );
                        }
                        varargArray->elements.push_back(*args[i]);
                        ++i;
                    }
                    local->set(param.ident, TypedValue(varargArray, param.type.array()));
                } else {
                    if (!param.type.match(args[i]->type)) {
                        error(
                            "Expected type " + param.type.toString() + 
                            " but got " + args[i]->type.toString()
                        );
                    }
                    local->set(param.ident, *args[i]);
                }
            }

            ReturnValue r = executeNode(funcData->body, local);

            if(r.hasBreak || r.hasContinue)
                error("Unexpected break or continue statement in function");

            if (!funcData->retType.match(r.value.type)) {
                error(
                    "Function return type mismatch - got " + r.value.type.toString() + 
                    " but expected " + funcData->retType.toString()
                );
            }

            return std::make_shared<TypedValue>(r.hasReturn ? r.value : TypedValue());
        }
    });
}

std::shared_ptr<Function> Executor::createNativeFunction(std::string name, FunctionData funcData, ENV env) {
    if(env->nativeInqueries.find(name) == env->nativeInqueries.end())
        error("Unable to link native function: " + name);
    auto nf = env->nativeInqueries[name];
    return std::make_shared<Function>(Function{
        [this, funcData, nf, env](const std::vector<std::shared_ptr<TypedValue>> &args){
            std::unordered_map<std::string, TypedValue> params;
            for (size_t i = 0; i < funcData->params.size() && i < args.size(); ++i) {
                if(!funcData->params[i].type.match(args[i]->type))
                    error("Expected type " + funcData->params[i].type.toString() + " but got " + args[1]->type.toString());
                params[funcData->params[i].ident] = *args[i];
            }
            ReturnValue r = nf(env, this, params);
            if(!funcData->retType.match(r.value.type)) {
                error("Native function return type mismatch - got " + r.value.type.toString() + " but expected " + funcData->retType.toString());
            }
            return std::make_shared<TypedValue>(r.hasReturn ? r.value : TypedValue());
        }
    });
}

TypedValue Executor::evaluateExpression(std::shared_ptr<ParsedASTNode> node, ENV env) {
    printf("evaluating node: %s\n", astTypeToString(node->type).c_str());
    auto eval = [this, &env](std::shared_ptr<ParsedASTNode> n){ return evaluateExpression(n, env); };

    switch (node->type) {
        case ASTNode::Type::NUMBER: return TypedValue(std::stoi(node->strValue));
        case ASTNode::Type::BOOL: return TypedValue(node->strValue == "1");
        case ASTNode::Type::STRING: return TypedValue(node->strValue);
        case ASTNode::Type::IDENTIFIER: return env->get(node->strValue);
        case ASTNode::Type::SELF_REFERENCE:
            if (env->hasSelfRef()) return env->currentSelfRef();
            return TypedValue();

        case ASTNode::Type::PRIMITIVE_ASSIGNMENT: return handleAssignment(node, env, node->primitiveValue, true);
        case ASTNode::Type::STRUCT_ASSIGNMENT: return handleStructAssignment(node, env);
        case ASTNode::Type::NDARRAY_ASSIGN: return handleNDArrayAssignment(node, env);

        case ASTNode::Type::ARRAY_ACCESS: {
            TypedValue arrVal = eval(node->children[0]);
            auto idxNode = node->children[1];

            if (arrVal.type.kind == BaseType::String) {
                const std::string s = getStringValue(arrVal);
                std::vector<int> indices = getIndices(nullptr, idxNode, env);

                std::string result;
                result.reserve(indices.size());

                for (int idx : indices) {
                    if (idx < 0 || idx >= static_cast<int>(s.size()))
                        error("String index out of bounds");
                    result.push_back(s[idx]);
                }

                return TypedValue(result);
            }

            if (arrVal.type.kind != BaseType::Array)
                error("Attempted array access on non-array");

            auto arr = arrVal.get<std::shared_ptr<Array>>();
            auto indices = getIndices(arr, idxNode, env);

            return arrayOperation(arr, indices);
        }

        case ASTNode::Type::ARRAY_ASSIGN: {
            TypedValue arrVal = eval(node->children[0]);
            auto idxNode = node->children[1];
            auto valNode = node->children[2];

            if (arrVal.type.kind != BaseType::Array) {
                error("Attempted array assignment on non-array");
            }

            auto arr = arrVal.get<std::shared_ptr<Array>>();
            auto indices = getIndices(arr, idxNode, env);

            arrayOperation(arr, indices, valNode, env);

            return arrVal;
        }

        case ASTNode::Type::ARRAY_LITERAL: {
            if (node->children.empty()) {
                auto arr = std::make_shared<Array>();
                arr->elementType = Type(BaseType::NIL);
                return TypedValue(arr, arr->elementType.array());
            }

            TypedValue firstVal;
            bool firstValSet = false;

            auto arr = std::make_shared<Array>();

            for (auto &child : node->children) {
                if (child->type == ASTNode::Type::RANGE) {
                    if (firstValSet && !firstVal.type.match(BaseType::Int))
                        error("RANGE literal is only allowed for integer arrays");

                    int start = getIntValue(eval(child->children[0]));
                    int end   = getIntValue(eval(child->children[1]));
                    for (int i = start; i <= end; ++i) {
                        env->pushSelfRef(TypedValue(i));
                        arr->elements.push_back(TypedValue(i));
                        env->popSelfRef();
                    }
                    firstValSet = true;
                    if (!firstValSet) firstVal = TypedValue(0);
                } else {
                    TypedValue val = evaluateExpression(child, env);
                    if (!firstValSet) {
                        firstVal = val;
                        arr->elementType = val.type;
                        firstValSet = true;
                    } else if (!val.type.match(arr->elementType)) {
                        error(
                            "Array literal elements must have the same type: got " +
                            val.type.toString() + " but expected " + arr->elementType.toString()
                        );
                    }
                    env->pushSelfRef(val);
                    arr->elements.push_back(val);
                    env->popSelfRef();
                }
            }

            return TypedValue(arr, arr->elementType.array());
        }

        case ASTNode::Type::CALL: {
            auto calleeVal = eval(node->children[0]);
            if (!calleeVal.type.match(BaseType::Function))
                error("Attempted to call a non-function value");
            std::vector<std::shared_ptr<TypedValue>> args;
            for (size_t i = 1; i < node->children.size(); ++i) args.push_back(std::make_shared<TypedValue>(eval(node->children[i])));
            return *calleeVal.get<std::shared_ptr<Function>>()->fn(args);
        }

        case ASTNode::Type::BINARY_OP: {
            const auto lhs = eval(node->children[0]);
            const auto rhs = eval(node->children[1]);
            const auto op  = node->binopValue;

            if (op < STRING_END) return evalBinaryStringOp    (op, lhs, rhs);
            if (op < ARITH_END)  return evalBinaryArithmeticOp(op, lhs, rhs);
            if (op < BOOL_END)   return evalBinaryBoolOp      (op, lhs, rhs);

            error("Unsupported binary op");
        }

        case ASTNode::Type::UNARY_OP: {
            int val = getIntValue(eval(node->children[0]));
            switch(node->binopValue) {
                case MINUS: return TypedValue(-val);
                case BITWISE_NOT: return TypedValue(~val);
                case NOT: return TypedValue(!val);
                default: error("Unsupported unary op");
            }
        }

        case ASTNode::Type::READ: {
            TypedValue target = eval(node->children[0]);
            return evaluateReadProperty(target, node->children[1]->strValue);
        }

        case ASTNode::Type::SIZED_ARRAY_DECLARE: {
            int size = getIntValue(eval(node->children[0]));
            TypedValue val = primitiveValue(node->primitiveValue);

            auto arr = std::make_shared<Array>();
            arr->elementType = val.type;

            for (int i = 0; i < size; ++i) {
                arr->elements.push_back(val);
            }

            return TypedValue(arr, arr->elementType.array());
        }


        default:
            error("Unsupported expression type: " + std::to_string(static_cast<int>(node->type)));
    }
}