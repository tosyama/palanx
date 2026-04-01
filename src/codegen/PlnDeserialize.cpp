#include "PlnDeserialize.h"
#include "PlnCodegenMessage.h"
#include <boost/assert.hpp>
#include <iostream>
using std::cerr;
using std::endl;

using namespace std;

static VRegType toVRegType(const json& vt) {
    if (vt["type-kind"] == "pntr") return VRegType::Ptr64;
    string name = vt["type-name"].get<string>();
    if (name == "int8")   return VRegType::Int8;
    if (name == "int16")  return VRegType::Int16;
    if (name == "int32")  return VRegType::Int32;
    if (name == "int64")  return VRegType::Int64;
    if (name == "flo32")  return VRegType::Float32;
    if (name == "flo64")  return VRegType::Float64;
    cerr << PlnCodegenMessage::getMessage(E_UnknownType, name) << endl;
    exit(1);
}

static unique_ptr<Expr> deserializeExpr(const json& j)
{
    string expr_type = j["expr-type"];

    if (expr_type == "lit-str") {
        auto e = make_unique<StrLitExpr>();
        e->label = j["label"];
        return e;
    }
    if (expr_type == "lit-int") {
        auto e = make_unique<IntLitExpr>();
        e->value = j["value"];
        e->type  = j.contains("value-type") ? toVRegType(j["value-type"]) : VRegType::Int64;
        return e;
    }
    if (expr_type == "lit-uint") {
        auto e = make_unique<UintLitExpr>();
        e->value = j["value"];
        return e;
    }
    if (expr_type == "lit-flo") {
        auto e = make_unique<FloLitExpr>();
        e->value = j["value"];
        e->type  = j.contains("value-type") ? toVRegType(j["value-type"]) : VRegType::Float64;
        return e;
    }
    if (expr_type == "id") {
        auto e = make_unique<IdExpr>();
        e->name = j["name"];
        return e;
    }
    if (expr_type == "convert") {
        auto e = make_unique<ConvertExpr>();
        e->from = toVRegType(j["from-type"]);
        e->to   = toVRegType(j["value-type"]);
        e->src  = deserializeExpr(j["src"]);
        return e;
    }
    if (expr_type == "add") {
        auto e = make_unique<AddExpr>();
        e->left  = deserializeExpr(j["left"]);
        e->right = deserializeExpr(j["right"]);
        e->type  = j.contains("value-type") ? toVRegType(j["value-type"]) : VRegType::Int64;
        return e;
    }
    if (expr_type == "sub") {
        auto e = make_unique<SubExpr>();
        e->left  = deserializeExpr(j["left"]);
        e->right = deserializeExpr(j["right"]);
        e->type  = j.contains("value-type") ? toVRegType(j["value-type"]) : VRegType::Int64;
        return e;
    }
    if (expr_type == "mul") {
        auto e = make_unique<MulExpr>();
        e->left  = deserializeExpr(j["left"]);
        e->right = deserializeExpr(j["right"]);
        e->type  = j.contains("value-type") ? toVRegType(j["value-type"]) : VRegType::Int64;
        return e;
    }
    if (expr_type == "div") {
        auto e = make_unique<DivExpr>();
        e->left  = deserializeExpr(j["left"]);
        e->right = deserializeExpr(j["right"]);
        e->type  = j.contains("value-type") ? toVRegType(j["value-type"]) : VRegType::Int64;
        return e;
    }
    if (expr_type == "mod") {
        auto e = make_unique<ModExpr>();
        e->left  = deserializeExpr(j["left"]);
        e->right = deserializeExpr(j["right"]);
        e->type  = j.contains("value-type") ? toVRegType(j["value-type"]) : VRegType::Int64;
        return e;
    }
    if (expr_type == "neg") {
        auto e     = make_unique<NegExpr>();
        e->operand = deserializeExpr(j["operand"]);
        e->type    = j.contains("value-type") ? toVRegType(j["value-type"]) : VRegType::Int64;
        return e;
    }
    if (expr_type == "cmp") {
        auto e = make_unique<CmpExpr>();
        e->op    = j["op"];
        e->left  = deserializeExpr(j["left"]);
        e->right = deserializeExpr(j["right"]);
        e->operandType = j["left"].contains("value-type")
                         ? toVRegType(j["left"]["value-type"]) : VRegType::Int64;
        return e;
    }
    if (expr_type == "call") {
        string func_type = j.value("func-type", "");
        if (func_type == "c") {
            auto e = make_unique<CCCallExpr>();
            e->name = j["name"];
            if (j.contains("value-type")) {
                e->hasRet  = true;
                e->retType = toVRegType(j["value-type"]);
            }
            if (j.contains("args")) {
                for (auto& arg : j["args"]) {
                    e->args.push_back(deserializeExpr(arg));
                }
            }
            return e;
        } else {
            auto e = make_unique<PlnCallExpr>();
            e->name = j["name"];
            if (j.contains("value-type")) {
                e->hasRet  = true;
                e->retType = toVRegType(j["value-type"]);
            }
            if (j.contains("args"))
                for (auto& arg : j["args"])
                    e->args.push_back(deserializeExpr(arg));
            return e;
        }
    }

    cerr << PlnCodegenMessage::getMessage(E_UnknownExprType, j["expr-type"].get<string>()) << endl;
    exit(1);
}

static vector<unique_ptr<Stmt>> deserializeStatements(const json& arr);  // forward declaration

static unique_ptr<Stmt> deserializeStmt(const json& j)
{
    string stmt_type = j["stmt-type"];

    if (stmt_type == "expr") {
        auto s = make_unique<ExprStmt>();
        s->body = deserializeExpr(j["body"]);
        return s;
    }
    if (stmt_type == "var-decl") {
        auto s = make_unique<VarDeclStmt>();
        for (auto& jv : j["vars"]) {
            VarEntry ve;
            ve.varName  = jv["name"];
            ve.typeName = jv["var-type"]["type-name"];
            if (jv.contains("init")) {
                ve.init = deserializeExpr(jv["init"]);
            }
            s->vars.push_back(move(ve));
        }
        return s;
    }
    if (stmt_type == "assign") {
        auto s = make_unique<AssignStmt>();
        s->name  = j["name"];
        s->type  = j["value"].contains("value-type") ? toVRegType(j["value"]["value-type"])
                                                      : VRegType::Int64;
        s->value = deserializeExpr(j["value"]);
        return s;
    }
    if (stmt_type == "return") {
        auto s = make_unique<ReturnStmt>();
        if (j.contains("values"))
            for (auto& v : j["values"])
                s->values.push_back(deserializeExpr(v));
        return s;
    }
    if (stmt_type == "tapple-decl") {
        auto s = make_unique<TappleDeclStmt>();
        s->funcName = j["value"]["name"];
        for (auto& jv : j["vars"])
            s->vars.push_back({jv["var-name"], toVRegType(jv["var-type"])});
        if (j["value"].contains("args"))
            for (auto& arg : j["value"]["args"])
                s->args.push_back(deserializeExpr(arg));
        for (auto& vt : j["value"]["value-types"])
            s->retTypes.push_back(toVRegType(vt));
        return s;
    }
    if (stmt_type == "block") {
        auto s = make_unique<BlockStmt>();
        s->body = deserializeStatements(j["body"]);
        return s;
    }
    if (stmt_type == "if") {
        auto s = make_unique<IfStmt>();
        s->cond = deserializeExpr(j["cond"]);
        auto tb = make_unique<BlockStmt>();
        tb->body = deserializeStatements(j["then"]["body"]);
        s->thenStmt = move(tb);
        if (j.contains("else")) {
            if (j["else"]["stmt-type"] == "if") {
                s->elseStmt = deserializeStmt(j["else"]);
            } else {
                auto eb = make_unique<BlockStmt>();
                eb->body = deserializeStatements(j["else"]["body"]);
                s->elseStmt = move(eb);
            }
        }
        return s;
    }
    if (stmt_type == "while") {
        auto s = make_unique<WhileStmt>();
        s->cond = deserializeExpr(j["cond"]);
        auto body = make_unique<BlockStmt>();
        body->body = deserializeStatements(j["body"]);
        s->body = move(body);
        return s;
    }
    if (stmt_type == "break") {
        return make_unique<BreakStmt>();
    }
    if (stmt_type == "continue") {
        return make_unique<ContinueStmt>();
    }
    if (stmt_type == "not-impl") {
        return nullptr;
    }

    cerr << PlnCodegenMessage::getMessage(E_UnknownStmtType, stmt_type) << endl;
    exit(1);
}

static vector<unique_ptr<Stmt>> deserializeStatements(const json& arr)
{
    vector<unique_ptr<Stmt>> result;
    for (auto& j : arr) {
        auto stmt = deserializeStmt(j);
        if (stmt)
            result.push_back(move(stmt));
    }
    return result;
}

Module deserialize(const json& sa)
{
    Module mod;
    mod.original = sa.value("original", "");

    if (sa.contains("str-literals")) {
        for (auto& j : sa["str-literals"]) {
            mod.strLiterals.push_back({j["label"], j["value"]});
        }
    }

    if (sa.contains("functions")) {
        for (auto& jf : sa["functions"]) {
            PlnFunc pf;
            pf.name = jf["name"];
            for (auto& jp : jf["parameters"])
                pf.params.push_back({jp["name"], toVRegType(jp["var-type"])});
            if (jf.contains("ret-type")) {
                pf.hasRetType = true;
                pf.retType    = toVRegType(jf["ret-type"]);
            }
            if (jf.contains("rets") && jf["rets"].size() == 1) {
                pf.retVarName = jf["rets"][0]["name"];
            } else if (jf.contains("rets") && jf["rets"].size() >= 2) {
                for (auto& r : jf["rets"])
                    pf.retVars.push_back({r["name"], toVRegType(r["var-type"])});
            }
            pf.isExport = jf.value("export", false);
            pf.body = deserializeStatements(jf["body"]);
            mod.functions.push_back(move(pf));
        }
    }

    if (sa.contains("statements"))
        mod.statements = deserializeStatements(sa["statements"]);

    return mod;
}
