#include "PlnDeserialize.h"
#include <boost/assert.hpp>

using namespace std;

static VRegType toVRegType(const json& vt) {
    if (vt["type-kind"] == "pntr") return VRegType::Ptr64;
    string name = vt["type-name"].get<string>();
    if (name == "int8")  return VRegType::Int8;
    if (name == "int16") return VRegType::Int16;
    if (name == "int32") return VRegType::Int32;
    if (name == "int64") return VRegType::Int64;
    BOOST_ASSERT(false);
    return VRegType::Int64;
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
        return e;
    }
    if (expr_type == "lit-uint") {
        auto e = make_unique<UintLitExpr>();
        e->value = j["value"];
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
        return e;
    }
    if (expr_type == "call") {
        string func_type = j.value("func-type", "");
        if (func_type == "c") {
            auto e = make_unique<CCCallExpr>();
            e->name = j["name"];
            if (j.contains("args")) {
                for (auto& arg : j["args"]) {
                    e->args.push_back(deserializeExpr(arg));
                }
            }
            return e;
        } else {
            auto e = make_unique<PlnCallExpr>();
            e->name = j["name"];
            if (j.contains("args")) {
                for (auto& arg : j["args"]) {
                    e->args.push_back(deserializeExpr(arg));
                }
            }
            return e;
        }
    }

    BOOST_ASSERT(false);
    return nullptr;
}

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
            ve.varName  = jv["var-name"];
            ve.typeName = jv["var-type"]["type-name"];
            if (jv.contains("init")) {
                ve.init = deserializeExpr(jv["init"]);
            }
            s->vars.push_back(move(ve));
        }
        return s;
    }
    if (stmt_type == "not-impl") {
        return nullptr;
    }

    BOOST_ASSERT(false);
    return nullptr;
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

    if (sa.contains("statements")) {
        for (auto& j : sa["statements"]) {
            auto stmt = deserializeStmt(j);
            if (stmt) {
                mod.statements.push_back(move(stmt));
            }
        }
    }
    return mod;
}
