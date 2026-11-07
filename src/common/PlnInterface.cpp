/// Writes a module's declarations as an interface (.pai).
///
/// @file PlnInterface.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <vector>
#include "PlnDefs.h"
#include "PlnInterface.h"

using namespace std;
using json = nlohmann::json;

namespace {

class Writer {
public:
	explicit Writer(const json& ast) : ast_(ast) {}
	string write();

private:
	const json& ast_;

	[[noreturn]] void fail(const json& node, const string& what) const;
	string type(const json& t) const;
	string expr(const json& e) const;
	string operand(const json& e) const;
	string args(const json& list) const;
	string vars(const json& node, const json& list) const;
	string funcDecl(const json& f) const;
	string statement(const json& s) const;
};

const map<string, string> binaryOps = {
	{"add", "+"}, {"sub", "-"}, {"mul", "*"}, {"div", "/"}, {"mod", "%"},
	{"bitand", "&"}, {"bitor", "|"}, {"bitxor", "^"}, {"shl", "<<<"}, {"shr", ">>>"},
	{"logical-and", "&&"}, {"logical-or", "||"},
};

void Writer::fail(const json& node, const string& what) const
{
	string at = ast_["original"].get<string>();
	if (node.contains("loc"))
		at += ":" + to_string(node["loc"][0].get<int>()) + ":" + to_string(node["loc"][1].get<int>());
	throw runtime_error(at + ": error: an interface cannot state " + what + ".");
}

string Writer::type(const json& t) const
{
	if (t.contains("not-impl")) fail(t, "this type");
	string kind = t["type-kind"];
	if (kind == "prim") return t["type-name"];
	if (kind == "embed") return "$" + type(t["base-type"]);
	if (kind == "keep") return "_";
	if (kind == "pntr") {
		string at = t.value("mutable", true) ? "@!" : "@";
		const json& base = t["base-type"];
		if (base["type-kind"] != "func") return at + type(base);
		string s = at + "func(";
		const char* sep = "";
		for (auto& p : base["parameters"]) {
			s += sep + type(p["var-type"]);
			sep = ", ";
		}
		s += ")";
		const json& ret = base["ret-type"];
		bool isVoid = ret["type-kind"] == "prim" && ret["type-name"] == "void";
		return isVoid ? s : s + " -> " + type(ret);
	}
	// Only "arr" is left: the other kinds come from C headers, whose declarations are not written.
	string mark = t["specifier"] == "fixed" ? "#" : t["specifier"] == "variable" ? "+" : "";
	string size = t["size-expr"].is_null() ? "" : expr(t["size-expr"]);
	return "[" + mark + size + "]" + (t.value("embedded", false) ? "$" : "") + type(t["base-type"]);
}

// A nested operator is bracketed rather than relying on precedence; brackets leave no trace in the AST.
string Writer::operand(const json& e) const
{
	string kind = e.value("expr-type", "");
	bool isOperator = binaryOps.count(kind) || kind == "cmp" || kind == "cast" || kind == "chain-call";
	return isOperator ? "(" + expr(e) + ")" : expr(e);
}

string Writer::args(const json& list) const
{
	string s;
	const char* sep = "";
	for (auto& a : list) {
		s += sep + expr(a);
		sep = ", ";
	}
	return s;
}

string Writer::expr(const json& e) const
{
	string kind = e.value("expr-type", "");
	auto op = binaryOps.find(kind);
	if (op != binaryOps.end()) return operand(e["left"]) + " " + op->second + " " + operand(e["right"]);
	if (kind == "cmp") return operand(e["left"]) + " " + e["op"].get<string>() + " " + operand(e["right"]);
	if (kind == "lit-int") {
		bool isBool = e.contains("value-type") && e["value-type"].value("type-name", "") == "bool";
		if (isBool) return e["value"] == "1" ? "true" : "false";
		return e["value"];
	}
	if (kind == "lit-uint") return e["value"].get<string>() + "u";
	if (kind == "lit-str") {
		string s = "\"";
		for (char c : e["value"].get<string>()) {
			switch (c) {
				case '"': s += "\\\""; break;
				case '\\': s += "\\\\"; break;
				case '\n': s += "\\n"; break;
				case '\t': s += "\\t"; break;
				case '\r': s += "\\r"; break;
				default: s += c;
			}
		}
		return s + "\"";
	}
	if (kind == "lit-flo" || kind == "id") return e[kind == "id" ? "name" : "value"];
	if (kind == "neg") return "-" + operand(e["operand"]);
	if (kind == "bitnot") return "~" + operand(e["operand"]);
	if (kind == "logical-not") return "!" + operand(e["operand"]);
	if (kind == "sizeof") return "sizeof(" + type(e["type"]) + ")";
	if (kind == "cast") return operand(e["src"]) + " as " + type(e["target-type"]);
	if (kind == "call") return e["name"].get<string>() + "(" + args(e["args"]) + ")";
	if (kind == "member-call")
		return operand(e["object"]) + "." + e["method"].get<string>() + "(" + args(e["args"]) + ")";
	if (kind == "chain-call") return operand(e["value"]) + " -> " + expr(e["call"]);
	if (kind == "arr-index") return operand(e["array"]) + "[" + expr(e["index"]) + "]";
	if (kind == "field-access") return operand(e["object"]) + "." + e["field"].get<string>();
	if (kind == "addr-of") return (e["mutable"].get<bool>() ? "@!" : "@") + expr(e["object"]);
	fail(e, "this expression");
}

string Writer::vars(const json& node, const json& list) const
{
	string s;
	const char* sep = "";
	for (auto& v : list) {
		if (!v.contains("var-type")) fail(v.contains("loc") ? v : node, "this parameter or return");
		if (v.contains("init")) fail(v["init"], "a parameter's default value");
		s += sep + type(v["var-type"]) + " " + v["name"].get<string>();
		sep = ", ";
	}
	return s;
}

string Writer::funcDecl(const json& f) const
{
	bool isSyscall = f["func-type"] == "syscall";
	string s = string("export ") + (isSyscall ? "syscall " : "func ") + f["name"].get<string>()
		+ "(" + vars(f, f["parameters"]) + ")";
	if (f.contains("rets")) s += " -> " + vars(f, f["rets"]);
	else if (f.contains("ret-type")) s += " -> " + type(f["ret-type"]);
	if (isSyscall) s += " = " + expr(f["syscall-number"]);
	return s + ";\n";
}

string Writer::statement(const json& s) const
{
	string exported = s.value("export", false) ? "export " : "";
	string kind = s["stmt-type"];
	string name = s.value("name", "");
	if (kind == "const-decl") return "const " + name + " = " + expr(s["value"]) + ";\n";
	if (kind == "type-alias") return exported + "type " + name + " = " + type(s["type"]) + ";\n";
	if (kind == "enum-def") {
		string out = exported + "type " + name + " enum {\n";
		for (auto& e : s["enumerators"]) {
			out += "\t" + e["name"].get<string>();
			if (e.contains("value")) out += " = " + expr(e["value"]);
			out += ",\n";
		}
		return out + "};\n";
	}
	if (kind == "struct-def") {
		string out = exported + "type " + name + " {\n";
		for (auto& f : s["fields"])
			out += "\t" + type(f["var-type"]) + " " + f["name"].get<string>() + ";\n";
		return out + "};\n";
	}
	string path = s["path-type"] == "src" ? "\"" + s["path"].get<string>() + "\"" : "<" + s["path"].get<string>() + ">";
	string alias = s.contains("alias") ? " as " + s["alias"].get<string>() : "";
	auto names = [](const json& list) {
		string out;
		const char* sep = "";
		for (auto& n : list) {
			out += sep + n.get<string>();
			sep = ", ";
		}
		return out;
	};
	if (kind == "cinclude") return "cinclude " + path + alias + " export " + names(s["export-types"]) + ";\n";
	string targets = s.contains("targets") ? names(s["targets"]) + " from " : "";
	return "import " + targets + path + alias + ";\n";
}

// Every library any `cinclude` of the module links, in source order, however deep in a body.
void collectLinks(const json& node, vector<string>& libs)
{
	if (node.is_object() && node.value("stmt-type", "") == "cinclude") {
		for (auto& l : node.value("libs", json::array()))
			if (find(libs.begin(), libs.end(), l.get<string>()) == libs.end()) libs.push_back(l);
		return;
	}
	if (node.is_structured())
		for (auto& child : node) collectLinks(child, libs);
}

void collectIds(const json& node, set<string>& ids)
{
	if (node.is_object() && node.value("expr-type", "") == "id") {
		ids.insert(node["name"].get<string>());
		return;
	}
	// `Color.Red` names a type, never a variable or a const.
	if (node.is_object() && node.value("expr-type", "") == "field-access"
			&& node["object"].value("expr-type", "") == "id")
		return;
	if (node.is_structured())
		for (auto& child : node) collectIds(child, ids);
}

bool hasCall(const json& node)
{
	string kind = node.is_object() ? node.value("expr-type", "") : "";
	if (kind == "call" || kind == "member-call" || kind == "chain-call") return true;
	if (node.is_structured())
		for (auto& child : node)
			if (hasCall(child)) return true;
	return false;
}

// The variable's type with each size its array literal gave filled in, if the
// sizes are then known without running anything: all a const can read from it.
optional<json> sizedArrayType(const json& var, const set<string>& consts)
{
	if (!var.contains("var-type")) return nullopt;
	json result = var["var-type"];
	static const json noRows = json::array();
	bool hasLiteral = var.contains("init") && var["init"].value("expr-type", "") == "arr-lit";
	const json* rows = hasLiteral ? &var["init"]["items"] : &noRows;
	json* level = result["type-kind"] == "embed" ? &result["base-type"] : &result;
	if (level->value("type-kind", "") != "arr") return nullopt;
	for (bool top = true; level->value("type-kind", "") == "arr"; top = false) {
		json& size = (*level)["size-expr"];
		bool isVarr = (*level)["specifier"] == "variable";
		if (size.is_null() && (top || !isVarr)) {
			if (rows->empty()) return nullopt;
			size = {{"expr-type", "lit-int"}, {"value", to_string(rows->size())}};
		} else if (!size.is_null()) {
			set<string> ids;
			collectIds(size, ids);
			for (auto& id : ids)
				if (!consts.count(id)) return nullopt;
			if (hasCall(size)) return nullopt;
		}
		rows = !rows->empty() && (*rows)[0].contains("items") ? &(*rows)[0]["items"] : &noRows;
		level = &(*level)["base-type"];
	}
	return result;
}

string Writer::write()
{
	const json& stmts = ast_["ast"]["statements"];

	// A const stays if everything its value names does: another const staying,
	// or a top-level array declared before it whose size is known. Any other
	// could not be used by a declaration anyway (E_ConstUsedBeforeKnown).
	set<string> consts;
	for (auto& s : stmts)
		if (s.value("stmt-type", "") == "const-decl") consts.insert(s["name"].get<string>());
	map<const json*, json> usedVars;
	for (bool changed = true; changed;) {
		changed = false;
		usedVars.clear();
		map<string, pair<const json*, json>> arrays;
		for (auto& s : stmts) {
			string kind = s.value("stmt-type", "");
			if (kind == "var-decl") {
				for (auto& v : s["vars"]) {
					auto sized = sizedArrayType(v, consts);
					if (sized) arrays[v["name"]] = {&v, *sized};
				}
			} else if (kind == "const-decl" && consts.count(s["name"])) {
				set<string> ids;
				collectIds(s["value"], ids);
				for (auto& id : ids) {
					auto arr = arrays.find(id);
					if (arr != arrays.end()) {
						usedVars[arr->second.first] = arr->second.second;
					} else if (!consts.count(id)) {
						consts.erase(s["name"].get<string>());
						changed = true;
						break;
					}
				}
			}
		}
	}

	string out;
	string module = ast_["module"];
	size_t dot = module.rfind('.');
	if (dot != string::npos) out += "library " + module.substr(0, dot) + ";\n";
	out += "built \"" PALAN_VERSION "\";\n";
	vector<string> libs;
	collectLinks(ast_["ast"], libs);
	for (auto& lib : libs) out += "link \"" + lib + "\";\n";
	for (auto& s : stmts) {
		string kind = s.value("stmt-type", "");
		if (kind == "var-decl") {
			for (auto& v : s["vars"]) {
				auto used = usedVars.find(&v);
				if (used != usedVars.end()) out += type(used->second) + " " + v["name"].get<string>() + ";\n";
			}
		} else if (kind == "const-decl") {
			if (consts.count(s["name"])) out += statement(s);
		} else if (kind == "struct-def" || kind == "enum-def" || kind == "type-alias" || kind == "import"
				|| (kind == "cinclude" && s.contains("export-types"))) {
			out += statement(s);
		}
	}
	for (auto& f : ast_.value("export", json::array())) out += funcDecl(f);
	return out;
}

} // namespace

string writeInterface(const json& ast)
{
	return Writer(ast).write();
}
