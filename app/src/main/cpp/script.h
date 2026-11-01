#pragma once
// Interpretador de um subconjunto de Haxe para os scripts (.hx) do h3d.
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <functional>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <stdexcept>
#include <sstream>
#include <algorithm>

namespace hx {

struct Value;
typedef std::shared_ptr<std::vector<Value>> ArrP;
typedef std::shared_ptr<std::map<std::string, Value>> ObjP;

struct Value {
    enum Type { Null, Num, Bool, Str, Arr, Obj };
    Type t = Null;
    double n = 0;
    bool b = false;
    std::string s;
    ArrP a;
    ObjP o;
    static Value num(double v) { Value x; x.t = Num; x.n = v; return x; }
    static Value boolean(bool v) { Value x; x.t = Bool; x.b = v; return x; }
    static Value str(const std::string& v) { Value x; x.t = Str; x.s = v; return x; }
    static Value arr() { Value x; x.t = Arr; x.a = std::make_shared<std::vector<Value>>(); return x; }
    static Value obj() { Value x; x.t = Obj; x.o = std::make_shared<std::map<std::string, Value>>(); return x; }
    bool truthy() const {
        switch (t) {
            case Null: return false;
            case Num: return n != 0;
            case Bool: return b;
            case Str: return !s.empty();
            default: return true;
        }
    }
};

inline std::string numStr(double n) {
    if (std::isfinite(n) && n == std::floor(n) && std::fabs(n) < 1e15) {
        char b[40];
        std::snprintf(b, sizeof b, "%lld", (long long)n);
        return b;
    }
    std::ostringstream os;
    os << n;
    return os.str();
}

inline std::string toStr(const Value& v) {
    switch (v.t) {
        case Value::Null: return "null";
        case Value::Num: return numStr(v.n);
        case Value::Bool: return v.b ? "true" : "false";
        case Value::Str: return v.s;
        case Value::Arr: {
            std::string r = "[";
            for (size_t i = 0; i < v.a->size(); i++) { if (i) r += ", "; r += toStr((*v.a)[i]); }
            return r + "]";
        }
        case Value::Obj: {
            std::string r = "{";
            bool first = true;
            for (auto& kv : *v.o) { if (!first) r += ", "; first = false; r += kv.first + ": " + toStr(kv.second); }
            return r + "}";
        }
    }
    return "";
}

// ---------------- Lexer ----------------
enum { T_END = 0, T_NUM, T_STR, T_ID, T_P };
struct Tok { int type = T_END; std::string s; double n = 0; int line = 0; };

inline std::vector<Tok> lex(const std::string& src) {
    std::vector<Tok> out;
    size_t i = 0, n = src.size();
    int line = 1;
    auto isIdStart = [](char c) { return std::isalpha((unsigned char)c) || c == '_' || c == '$'; };
    auto isIdChar = [](char c) { return std::isalnum((unsigned char)c) || c == '_' || c == '$'; };
    while (i < n) {
        char c = src[i];
        if (c == '\n') { line++; i++; continue; }
        if (std::isspace((unsigned char)c)) { i++; continue; }
        if (c == '/' && i + 1 < n && src[i + 1] == '/') { while (i < n && src[i] != '\n') i++; continue; }
        if (c == '/' && i + 1 < n && src[i + 1] == '*') {
            i += 2;
            while (i + 1 < n && !(src[i] == '*' && src[i + 1] == '/')) { if (src[i] == '\n') line++; i++; }
            i += 2;
            continue;
        }
        Tok t;
        t.line = line;
        if (std::isdigit((unsigned char)c) || (c == '.' && i + 1 < n && std::isdigit((unsigned char)src[i + 1]))) {
            size_t st = i;
            bool dot = false;
            if (c == '0' && i + 1 < n && (src[i + 1] == 'x' || src[i + 1] == 'X')) {
                i += 2;
                while (i < n && std::isxdigit((unsigned char)src[i])) i++;
                t.n = (double)std::strtoll(src.substr(st, i - st).c_str(), nullptr, 16);
            } else {
                while (i < n) {
                    if (std::isdigit((unsigned char)src[i])) i++;
                    else if (src[i] == '.' && !dot && i + 1 < n && src[i + 1] != '.') { dot = true; i++; }
                    else break;
                }
                if (i < n && (src[i] == 'e' || src[i] == 'E')) {
                    size_t j = i + 1;
                    if (j < n && (src[j] == '+' || src[j] == '-')) j++;
                    if (j < n && std::isdigit((unsigned char)src[j])) {
                        while (j < n && std::isdigit((unsigned char)src[j])) j++;
                        i = j;
                    }
                }
                t.n = std::strtod(src.substr(st, i - st).c_str(), nullptr);
            }
            t.type = T_NUM;
            out.push_back(t);
            continue;
        }
        if (isIdStart(c)) {
            size_t st = i;
            while (i < n && isIdChar(src[i])) i++;
            t.type = T_ID;
            t.s = src.substr(st, i - st);
            out.push_back(t);
            continue;
        }
        if (c == '"' || c == '\'') {
            char q = c;
            i++;
            std::string s;
            while (i < n && src[i] != q) {
                if (src[i] == '\\' && i + 1 < n) {
                    i++;
                    char e = src[i];
                    if (e == 'n') s += '\n';
                    else if (e == 't') s += '\t';
                    else if (e == 'r') s += '\r';
                    else s += e;
                    i++;
                } else {
                    if (src[i] == '\n') line++;
                    s += src[i++];
                }
            }
            i++;
            t.type = T_STR;
            t.s = s;
            out.push_back(t);
            continue;
        }
        t.type = T_P;
        if (src.compare(i, 3, "...") == 0) { t.s = "..."; i += 3; }
        else {
            static const char* two[] = {"==", "!=", "<=", ">=", "&&", "||", "+=", "-=", "*=", "/=", "++", "--", "->", "=>"};
            bool found = false;
            for (const char* p2 : two) {
                if (src.compare(i, 2, p2) == 0) { t.s = p2; i += 2; found = true; break; }
            }
            if (!found) { t.s = std::string(1, c); i++; }
        }
        out.push_back(t);
    }
    Tok e;
    e.type = T_END;
    e.line = line;
    out.push_back(e);
    return out;
}

// ---------------- AST ----------------
enum NT {
    N_NUM, N_STR, N_BOOL, N_NULL, N_IDENT, N_ARRAY, N_OBJECT, N_UNARY, N_BINARY, N_ASSIGN, N_CALL,
    N_MEMBER, N_INDEX, N_TERNARY, N_POSTINC,
    S_VAR, S_DECLS, S_BLOCK, S_IF, S_WHILE, S_FOR, S_RETURN, S_BREAK, S_CONTINUE, S_EXPR, S_FUNC
};
struct Node;
typedef std::shared_ptr<Node> NP;
struct Node {
    NT t = N_NULL;
    double n = 0;
    std::string s;
    std::string file;
    std::vector<NP> k;
    std::vector<std::string> params;
    int line = 0;
};

inline NP node(NT t, int line) {
    NP n = std::make_shared<Node>();
    n->t = t;
    n->line = line;
    return n;
}

struct Parser {
    std::vector<Tok> t;
    size_t p = 0;
    std::string file;

    bool isP(const char* s) const { return t[p].type == T_P && t[p].s == s; }
    bool isId(const char* s) const { return t[p].type == T_ID && t[p].s == s; }
    bool accP(const char* s) { if (isP(s)) { p++; return true; } return false; }
    bool accId(const char* s) { if (isId(s)) { p++; return true; } return false; }
    [[noreturn]] void fail(const std::string& m) const {
        throw std::runtime_error(file + ":linha " + std::to_string(t[p].line) + ": " + m);
    }
    void expP(const char* s) {
        if (!accP(s)) fail(std::string("esperado '") + s + "' mas achei '" + t[p].s + "'");
    }
    std::string ident() {
        if (t[p].type != T_ID) fail("esperado um nome");
        return t[p++].s;
    }

    void skipType() {
        if (accP("{")) {
            int d = 1;
            while (d > 0 && t[p].type != T_END) {
                if (isP("{")) d++;
                else if (isP("}")) d--;
                p++;
            }
            return;
        }
        ident();
        while (accP(".")) ident();
        if (isP("<")) {
            int d = 0;
            do {
                if (isP("<")) d++;
                else if (isP(">")) d--;
                p++;
            } while (d > 0 && t[p].type != T_END);
        }
        if (accP("->")) skipType();
    }

    void skipModifiers() {
        while (isId("public") || isId("private") || isId("static") || isId("inline") ||
               isId("override") || isId("extern") || isId("dynamic")) p++;
    }

    void parseTop(std::vector<NP>& items) {
        while (t[p].type != T_END) {
            if (isId("package") || isId("import") || isId("using")) {
                while (!isP(";") && t[p].type != T_END) p++;
                accP(";");
                continue;
            }
            if (accP(";")) continue;
            skipModifiers();
            if (isId("class")) {
                p++;
                ident();
                while (!isP("{") && t[p].type != T_END) p++;
                expP("{");
                while (!isP("}")) {
                    if (t[p].type == T_END) fail("faltou '}' no fim da classe");
                    parseMember(items);
                }
                expP("}");
                continue;
            }
            parseMember(items);
        }
    }

    void parseMember(std::vector<NP>& items) {
        skipModifiers();
        if (accP(";")) return;
        if (isId("function")) { items.push_back(parseFunc()); return; }
        if (isId("var") || isId("final")) { items.push_back(parseVarDecl()); return; }
        fail("no nivel superior so sao permitidos 'var' e 'function' (achei '" + t[p].s + "')");
    }

    NP parseFunc() {
        int ln = t[p].line;
        p++;  // function
        NP f = node(S_FUNC, ln);
        f->file = file;
        f->s = ident();
        expP("(");
        while (!isP(")")) {
            accP("?");
            f->params.push_back(ident());
            if (accP(":")) skipType();
            if (accP("=")) parseExpr();
            if (!accP(",")) break;
        }
        expP(")");
        if (accP(":")) skipType();
        f->k.push_back(parseBlock());
        return f;
    }

    NP parseVarDecl() {
        int ln = t[p].line;
        p++;  // var / final
        NP list = node(S_DECLS, ln);
        do {
            NP v = node(S_VAR, t[p].line);
            v->s = ident();
            if (accP(":")) skipType();
            if (accP("=")) v->k.push_back(parseExpr());
            list->k.push_back(v);
        } while (accP(","));
        accP(";");
        return list->k.size() == 1 ? list->k[0] : list;
    }

    NP parseBlock() {
        int ln = t[p].line;
        expP("{");
        NP b = node(S_BLOCK, ln);
        while (!isP("}")) {
            if (t[p].type == T_END) fail("faltou '}'");
            b->k.push_back(parseStmt());
        }
        expP("}");
        return b;
    }

    NP parseStmt() {
        int ln = t[p].line;
        if (isP("{")) return parseBlock();
        if (isId("var") || isId("final")) return parseVarDecl();
        if (isId("if")) {
            p++;
            expP("(");
            NP c = parseExpr();
            expP(")");
            NP a = parseStmt();
            NP n = node(S_IF, ln);
            n->k = {c, a};
            if (accId("else")) n->k.push_back(parseStmt());
            return n;
        }
        if (isId("while")) {
            p++;
            expP("(");
            NP c = parseExpr();
            expP(")");
            NP n = node(S_WHILE, ln);
            n->k = {c, parseStmt()};
            return n;
        }
        if (isId("for")) {
            p++;
            expP("(");
            NP n = node(S_FOR, ln);
            n->s = ident();
            if (!accId("in")) fail("esperado 'in' no for");
            NP it = parseExpr();
            expP(")");
            n->k = {it, parseStmt()};
            return n;
        }
        if (isId("return")) {
            p++;
            NP n = node(S_RETURN, ln);
            if (!isP(";") && !isP("}")) n->k.push_back(parseExpr());
            accP(";");
            return n;
        }
        if (isId("break")) { p++; accP(";"); return node(S_BREAK, ln); }
        if (isId("continue")) { p++; accP(";"); return node(S_CONTINUE, ln); }
        if (isId("function")) fail("funcoes dentro de funcoes nao sao suportadas");
        NP e = parseExpr();
        accP(";");
        NP n = node(S_EXPR, ln);
        n->k.push_back(e);
        return n;
    }

    // ---- expressoes ----
    NP parseExpr() { return parseAssign(); }

    NP parseAssign() {
        NP l = parseTernary();
        if (t[p].type == T_P) {
            const std::string& s = t[p].s;
            if (s == "=" || s == "+=" || s == "-=" || s == "*=" || s == "/=") {
                std::string op = s;
                int ln = t[p].line;
                if (l->t != N_IDENT && l->t != N_MEMBER && l->t != N_INDEX) fail("alvo de atribuicao invalido");
                p++;
                NP r = parseAssign();
                NP n = node(N_ASSIGN, ln);
                n->s = op;
                n->k = {l, r};
                return n;
            }
        }
        return l;
    }

    NP parseTernary() {
        NP c = parseOr();
        if (isP("?")) {
            int ln = t[p].line;
            p++;
            NP a = parseAssign();
            expP(":");
            NP b = parseAssign();
            NP n = node(N_TERNARY, ln);
            n->k = {c, a, b};
            return n;
        }
        return c;
    }

    NP bin(const std::string& op, NP a, NP b, int ln) {
        NP n = node(N_BINARY, ln);
        n->s = op;
        n->k = {a, b};
        return n;
    }

    NP parseOr() {
        NP l = parseAnd();
        while (isP("||")) { int ln = t[p].line; p++; l = bin("||", l, parseAnd(), ln); }
        return l;
    }
    NP parseAnd() {
        NP l = parseEq();
        while (isP("&&")) { int ln = t[p].line; p++; l = bin("&&", l, parseEq(), ln); }
        return l;
    }
    NP parseEq() {
        NP l = parseCmp();
        while (isP("==") || isP("!=")) { std::string op = t[p].s; int ln = t[p].line; p++; l = bin(op, l, parseCmp(), ln); }
        return l;
    }
    NP parseCmp() {
        NP l = parseRange();
        while (isP("<") || isP(">") || isP("<=") || isP(">=")) { std::string op = t[p].s; int ln = t[p].line; p++; l = bin(op, l, parseRange(), ln); }
        return l;
    }
    NP parseRange() {
        NP l = parseAdd();
        if (isP("...")) { int ln = t[p].line; p++; l = bin("...", l, parseAdd(), ln); }
        return l;
    }
    NP parseAdd() {
        NP l = parseMul();
        while (isP("+") || isP("-")) { std::string op = t[p].s; int ln = t[p].line; p++; l = bin(op, l, parseMul(), ln); }
        return l;
    }
    NP parseMul() {
        NP l = parseUnary();
        while (isP("*") || isP("/") || isP("%")) { std::string op = t[p].s; int ln = t[p].line; p++; l = bin(op, l, parseUnary(), ln); }
        return l;
    }

    NP parseUnary() {
        int ln = t[p].line;
        if (isP("!") || isP("-") || isP("+")) {
            std::string op = t[p].s;
            p++;
            NP n = node(N_UNARY, ln);
            n->s = op;
            n->k = {parseUnary()};
            return n;
        }
        if (isP("++") || isP("--")) {
            std::string op = t[p].s;
            p++;
            NP tgt = parseUnary();
            if (tgt->t != N_IDENT && tgt->t != N_MEMBER && tgt->t != N_INDEX) fail("alvo de ++/-- invalido");
            NP one = node(N_NUM, ln);
            one->n = 1;
            NP n = node(N_ASSIGN, ln);
            n->s = op == "++" ? "+=" : "-=";
            n->k = {tgt, one};
            return n;
        }
        return parsePostfix();
    }

    NP parsePostfix() {
        NP e = parsePrimary();
        while (true) {
            int ln = t[p].line;
            if (isP("(")) {
                p++;
                NP c = node(N_CALL, ln);
                c->k.push_back(e);
                while (!isP(")")) {
                    c->k.push_back(parseExpr());
                    if (!accP(",")) break;
                }
                expP(")");
                e = c;
            } else if (isP(".")) {
                p++;
                NP m = node(N_MEMBER, ln);
                m->s = ident();
                m->k.push_back(e);
                e = m;
            } else if (isP("[")) {
                p++;
                NP ix = node(N_INDEX, ln);
                ix->k = {e, parseExpr()};
                expP("]");
                e = ix;
            } else if (isP("++") || isP("--")) {
                if (e->t != N_IDENT && e->t != N_MEMBER && e->t != N_INDEX) break;
                NP pi = node(N_POSTINC, ln);
                pi->s = t[p].s;
                p++;
                pi->k.push_back(e);
                e = pi;
            } else break;
        }
        return e;
    }

    NP parsePrimary() {
        const Tok& tk = t[p];
        int ln = tk.line;
        if (tk.type == T_NUM) { p++; NP n = node(N_NUM, ln); n->n = tk.n; return n; }
        if (tk.type == T_STR) { p++; NP n = node(N_STR, ln); n->s = tk.s; return n; }
        if (tk.type == T_ID) {
            if (tk.s == "true" || tk.s == "false") { p++; NP n = node(N_BOOL, ln); n->n = tk.s == "true" ? 1 : 0; return n; }
            if (tk.s == "null") { p++; return node(N_NULL, ln); }
            if (tk.s == "function" || tk.s == "new") fail("'" + tk.s + "' nao e suportado nos scripts do h3d");
            p++;
            NP n = node(N_IDENT, ln);
            n->s = tk.s;
            return n;
        }
        if (isP("(")) {
            p++;
            NP e = parseExpr();
            expP(")");
            return e;
        }
        if (isP("[")) {
            p++;
            NP a = node(N_ARRAY, ln);
            while (!isP("]")) {
                a->k.push_back(parseExpr());
                if (!accP(",")) break;
            }
            expP("]");
            return a;
        }
        if (isP("{")) {
            p++;
            NP o = node(N_OBJECT, ln);
            while (!isP("}")) {
                if (t[p].type != T_ID && t[p].type != T_STR) fail("esperado nome do campo");
                NP key = node(N_STR, t[p].line);
                key->s = t[p].s;
                p++;
                expP(":");
                o->k.push_back(key);
                o->k.push_back(parseExpr());
                if (!accP(",")) break;
            }
            expP("}");
            return o;
        }
        fail("expressao inesperada: '" + tk.s + "'");
    }
};

// ---------------- Interpretador ----------------
typedef std::vector<Value> Args;
typedef std::function<Value(Args&)> NativeFn;

struct Interp {
    std::map<std::string, NP> funcs;
    std::unordered_map<std::string, Value> globals;
    std::map<std::string, NativeFn> natives;
    std::map<std::string, double> consts;

    long steps = 0;
    long stepLimit = 3000000;
    int depth = 0;
    std::string curFile = "script";
    Value retVal;

    Interp() { consts["Math.PI"] = 3.14159265358979323846; }

    bool check(const std::string& src, const std::string& name, std::string& err) {
        try {
            Parser ps;
            ps.t = lex(src);
            ps.file = name;
            std::vector<NP> items;
            ps.parseTop(items);
        } catch (std::exception& e) { err = e.what(); return false; }
        return true;
    }

    bool load(const std::string& src, const std::string& name, std::string& err) {
        try {
            Parser ps;
            ps.t = lex(src);
            ps.file = name;
            std::vector<NP> items;
            ps.parseTop(items);
            for (auto& it : items) if (it->t == S_FUNC) funcs[it->s] = it;
            curFile = name;
            steps = 0;
            for (auto& it : items) if (it->t != S_FUNC) exec(it);
        } catch (std::exception& e) {
            err = e.what();
            scopes.clear(); frameBase.clear(); depth = 0;
            return false;
        }
        return true;
    }

    bool has(const std::string& fn) const { return funcs.count(fn) > 0; }

    bool call(const std::string& fn, Args args, std::string& err) {
        auto it = funcs.find(fn);
        if (it == funcs.end()) return true;
        steps = 0;
        try {
            callUser(it->second, args);
        } catch (std::exception& e) {
            err = e.what();
            scopes.clear(); frameBase.clear(); depth = 0;
            return false;
        }
        return true;
    }

private:
    enum Flow { F_NORMAL, F_BREAK, F_CONTINUE, F_RETURN };
    std::vector<std::unordered_map<std::string, Value>> scopes;
    std::vector<size_t> frameBase;

    void step() {
        if (++steps > stepLimit) throw std::runtime_error("o script demorou demais (loop infinito?)");
    }
    [[noreturn]] void err(const NP& n, const std::string& m) {
        throw std::runtime_error(curFile + ":linha " + std::to_string(n->line) + ": " + m);
    }

    Value* lookup(const std::string& name) {
        if (!frameBase.empty()) {
            for (size_t i = scopes.size(); i > frameBase.back(); i--) {
                auto it = scopes[i - 1].find(name);
                if (it != scopes[i - 1].end()) return &it->second;
            }
        }
        auto g = globals.find(name);
        if (g != globals.end()) return &g->second;
        return nullptr;
    }
    void declare(const std::string& name, const Value& v) {
        if (frameBase.empty() || scopes.empty()) globals[name] = v;
        else scopes.back()[name] = v;
    }

    Value callUser(const NP& f, Args& args) {
        if (depth > 100) throw std::runtime_error("recursao profunda demais");
        depth++;
        std::string savedFile = curFile;
        curFile = f->file;
        frameBase.push_back(scopes.size());
        scopes.emplace_back();
        for (size_t i = 0; i < f->params.size(); i++)
            scopes.back()[f->params[i]] = i < args.size() ? args[i] : Value();
        Flow fl = exec(f->k[0]);
        Value r;
        if (fl == F_RETURN) r = retVal;
        retVal = Value();
        scopes.resize(frameBase.back());
        frameBase.pop_back();
        curFile = savedFile;
        depth--;
        return r;
    }

    static bool equal(const Value& a, const Value& b) {
        if (a.t != b.t) return false;
        switch (a.t) {
            case Value::Null: return true;
            case Value::Num: return a.n == b.n;
            case Value::Bool: return a.b == b.b;
            case Value::Str: return a.s == b.s;
            case Value::Arr: return a.a == b.a;
            case Value::Obj: return a.o == b.o;
        }
        return false;
    }

    Value binop(const NP& n, const std::string& op, const Value& a, const Value& b) {
        if (op == "+") {
            if (a.t == Value::Str || b.t == Value::Str) return Value::str(toStr(a) + toStr(b));
            if (a.t == Value::Num && b.t == Value::Num) return Value::num(a.n + b.n);
            err(n, "operandos invalidos para '+'");
        }
        if (op == "==") return Value::boolean(equal(a, b));
        if (op == "!=") return Value::boolean(!equal(a, b));
        if (op == "<" || op == ">" || op == "<=" || op == ">=") {
            if (a.t == Value::Num && b.t == Value::Num) {
                if (op == "<") return Value::boolean(a.n < b.n);
                if (op == ">") return Value::boolean(a.n > b.n);
                if (op == "<=") return Value::boolean(a.n <= b.n);
                return Value::boolean(a.n >= b.n);
            }
            if (a.t == Value::Str && b.t == Value::Str) {
                int c = a.s.compare(b.s);
                if (op == "<") return Value::boolean(c < 0);
                if (op == ">") return Value::boolean(c > 0);
                if (op == "<=") return Value::boolean(c <= 0);
                return Value::boolean(c >= 0);
            }
            err(n, "comparacao invalida");
        }
        if (a.t != Value::Num || b.t != Value::Num) err(n, "operandos invalidos para '" + op + "' (esperado numero)");
        if (op == "-") return Value::num(a.n - b.n);
        if (op == "*") return Value::num(a.n * b.n);
        if (op == "/") return Value::num(a.n / b.n);
        if (op == "%") return Value::num(std::fmod(a.n, b.n));
        err(n, "operador desconhecido: " + op);
    }

    void assignTo(const NP& target, const Value& v) {
        if (target->t == N_IDENT) {
            Value* p = lookup(target->s);
            if (!p) err(target, "variavel nao declarada: " + target->s);
            *p = v;
        } else if (target->t == N_MEMBER) {
            Value b = eval(target->k[0]);
            if (b.t != Value::Obj) err(target, "so e possivel alterar campos de objetos");
            (*b.o)[target->s] = v;
        } else if (target->t == N_INDEX) {
            Value b = eval(target->k[0]);
            Value ix = eval(target->k[1]);
            if (b.t == Value::Arr) {
                if (ix.t != Value::Num) err(target, "indice invalido");
                long i = (long)ix.n;
                if (i < 0 || i > 1000000) err(target, "indice fora dos limites");
                if ((size_t)i >= b.a->size()) b.a->resize((size_t)i + 1);
                (*b.a)[(size_t)i] = v;
            } else if (b.t == Value::Obj) {
                (*b.o)[toStr(ix)] = v;
            } else err(target, "valor nao indexavel");
        }
    }

    Value evalCall(const NP& n) {
        const NP& cal = n->k[0];
        Args args;
        for (size_t i = 1; i < n->k.size(); i++) args.push_back(eval(n->k[i]));
        if (cal->t == N_IDENT) {
            auto f = funcs.find(cal->s);
            if (f != funcs.end()) return callUser(f->second, args);
            auto nt = natives.find(cal->s);
            if (nt != natives.end()) return nt->second(args);
            err(n, "funcao desconhecida: " + cal->s);
        }
        if (cal->t == N_MEMBER) {
            const NP& base = cal->k[0];
            const std::string& m = cal->s;
            if (base->t == N_IDENT && !lookup(base->s)) {
                auto nt = natives.find(base->s + "." + m);
                if (nt != natives.end()) return nt->second(args);
                err(n, "funcao desconhecida: " + base->s + "." + m);
            }
            Value bv = eval(base);
            if (bv.t == Value::Arr) return arrayMethod(n, bv, m, args);
            if (bv.t == Value::Str) return stringMethod(n, bv, m, args);
            err(n, "metodo desconhecido: " + m);
        }
        err(n, "chamada invalida");
    }

    Value arrayMethod(const NP& n, Value& a, const std::string& m, Args& args) {
        auto& v = *a.a;
        if (m == "push") { if (!args.empty()) v.push_back(args[0]); return Value::num((double)v.size()); }
        if (m == "pop") { if (v.empty()) return Value(); Value r = v.back(); v.pop_back(); return r; }
        if (m == "shift") { if (v.empty()) return Value(); Value r = v.front(); v.erase(v.begin()); return r; }
        if (m == "indexOf" || m == "contains" || m == "remove") {
            long idx = -1;
            if (!args.empty())
                for (size_t i = 0; i < v.size(); i++) if (equal(v[i], args[0])) { idx = (long)i; break; }
            if (m == "indexOf") return Value::num((double)idx);
            if (m == "contains") return Value::boolean(idx >= 0);
            if (idx >= 0) v.erase(v.begin() + idx);
            return Value::boolean(idx >= 0);
        }
        if (m == "insert") {
            if (args.size() >= 2 && args[0].t == Value::Num) {
                long i = std::max(0L, std::min((long)v.size(), (long)args[0].n));
                v.insert(v.begin() + i, args[1]);
            }
            return Value();
        }
        if (m == "reverse") { std::reverse(v.begin(), v.end()); return Value(); }
        if (m == "join") {
            std::string sep = args.empty() ? "," : toStr(args[0]), r;
            for (size_t i = 0; i < v.size(); i++) { if (i) r += sep; r += toStr(v[i]); }
            return Value::str(r);
        }
        err(n, "metodo de array desconhecido: " + m);
    }

    Value stringMethod(const NP& n, Value& sv, const std::string& m, Args& args) {
        const std::string& s = sv.s;
        if (m == "toUpperCase") { std::string r = s; for (char& c : r) c = (char)std::toupper((unsigned char)c); return Value::str(r); }
        if (m == "toLowerCase") { std::string r = s; for (char& c : r) c = (char)std::tolower((unsigned char)c); return Value::str(r); }
        if (m == "charAt") {
            long i = args.empty() ? 0 : (long)args[0].n;
            return Value::str(i >= 0 && (size_t)i < s.size() ? std::string(1, s[(size_t)i]) : "");
        }
        if (m == "indexOf") {
            if (args.empty()) return Value::num(-1);
            size_t pos = s.find(toStr(args[0]));
            return Value::num(pos == std::string::npos ? -1 : (double)pos);
        }
        if (m == "substr") {
            long st = args.empty() ? 0 : (long)args[0].n;
            if (st < 0) st = 0;
            if ((size_t)st > s.size()) return Value::str("");
            size_t len = args.size() > 1 ? (size_t)std::max(0L, (long)args[1].n) : std::string::npos;
            return Value::str(s.substr((size_t)st, len));
        }
        err(n, "metodo de string desconhecido: " + m);
    }

    Value eval(const NP& n) {
        step();
        switch (n->t) {
            case N_NUM: return Value::num(n->n);
            case N_STR: return Value::str(n->s);
            case N_BOOL: return Value::boolean(n->n != 0);
            case N_NULL: return Value();
            case N_IDENT: {
                Value* v = lookup(n->s);
                if (!v) err(n, "variavel nao definida: " + n->s);
                return *v;
            }
            case N_ARRAY: {
                Value a = Value::arr();
                for (auto& k : n->k) a.a->push_back(eval(k));
                return a;
            }
            case N_OBJECT: {
                Value o = Value::obj();
                for (size_t i = 0; i + 1 < n->k.size(); i += 2) (*o.o)[n->k[i]->s] = eval(n->k[i + 1]);
                return o;
            }
            case N_UNARY: {
                Value v = eval(n->k[0]);
                if (n->s == "!") return Value::boolean(!v.truthy());
                if (v.t != Value::Num) err(n, "operador unario espera numero");
                return Value::num(n->s == "-" ? -v.n : v.n);
            }
            case N_BINARY: {
                const std::string& op = n->s;
                if (op == "&&") { Value a = eval(n->k[0]); if (!a.truthy()) return a; return eval(n->k[1]); }
                if (op == "||") { Value a = eval(n->k[0]); if (a.truthy()) return a; return eval(n->k[1]); }
                if (op == "...") err(n, "intervalo (a...b) so pode ser usado em 'for'");
                Value a = eval(n->k[0]);
                Value b = eval(n->k[1]);
                return binop(n, op, a, b);
            }
            case N_TERNARY: return eval(n->k[0]).truthy() ? eval(n->k[1]) : eval(n->k[2]);
            case N_ASSIGN: {
                Value rhs = eval(n->k[1]);
                if (n->s != "=") {
                    Value cur = eval(n->k[0]);
                    rhs = binop(n, n->s.substr(0, 1), cur, rhs);
                }
                assignTo(n->k[0], rhs);
                return rhs;
            }
            case N_POSTINC: {
                Value cur = eval(n->k[0]);
                if (cur.t != Value::Num) err(n, "++/-- espera numero");
                assignTo(n->k[0], Value::num(cur.n + (n->s == "++" ? 1 : -1)));
                return cur;
            }
            case N_CALL: return evalCall(n);
            case N_MEMBER: {
                const NP& base = n->k[0];
                if (base->t == N_IDENT && !lookup(base->s)) {
                    auto c = consts.find(base->s + "." + n->s);
                    if (c != consts.end()) return Value::num(c->second);
                    err(n, "nome desconhecido: " + base->s + "." + n->s);
                }
                Value b = eval(base);
                if (b.t == Value::Obj) {
                    auto it = b.o->find(n->s);
                    return it == b.o->end() ? Value() : it->second;
                }
                if (n->s == "length") {
                    if (b.t == Value::Arr) return Value::num((double)b.a->size());
                    if (b.t == Value::Str) return Value::num((double)b.s.size());
                }
                if (b.t == Value::Null) err(n, "acesso a campo '" + n->s + "' de um valor null");
                err(n, "campo desconhecido: " + n->s);
            }
            case N_INDEX: {
                Value b = eval(n->k[0]);
                Value ix = eval(n->k[1]);
                if (b.t == Value::Arr) {
                    if (ix.t != Value::Num) err(n, "indice invalido");
                    long i = (long)ix.n;
                    if (i < 0 || (size_t)i >= b.a->size()) return Value();
                    return (*b.a)[(size_t)i];
                }
                if (b.t == Value::Obj) {
                    auto it = b.o->find(toStr(ix));
                    return it == b.o->end() ? Value() : it->second;
                }
                if (b.t == Value::Str && ix.t == Value::Num) {
                    long i = (long)ix.n;
                    if (i < 0 || (size_t)i >= b.s.size()) return Value();
                    return Value::str(std::string(1, b.s[(size_t)i]));
                }
                err(n, "valor nao indexavel");
            }
            default: err(n, "expressao invalida");
        }
    }

    Flow exec(const NP& n) {
        step();
        switch (n->t) {
            case S_VAR: {
                Value v = n->k.empty() ? Value() : eval(n->k[0]);
                declare(n->s, v);
                return F_NORMAL;
            }
            case S_DECLS: for (auto& k : n->k) exec(k); return F_NORMAL;
            case S_BLOCK: {
                scopes.emplace_back();
                size_t sz = scopes.size();
                Flow f = F_NORMAL;
                for (auto& s : n->k) { f = exec(s); if (f != F_NORMAL) break; }
                scopes.resize(sz - 1);
                return f;
            }
            case S_IF:
                if (eval(n->k[0]).truthy()) return exec(n->k[1]);
                if (n->k.size() > 2) return exec(n->k[2]);
                return F_NORMAL;
            case S_WHILE:
                while (eval(n->k[0]).truthy()) {
                    Flow f = exec(n->k[1]);
                    if (f == F_BREAK) break;
                    if (f == F_RETURN) return f;
                }
                return F_NORMAL;
            case S_FOR: {
                const NP& itn = n->k[0];
                scopes.emplace_back();
                size_t sz = scopes.size();
                Flow result = F_NORMAL;
                if (itn->t == N_BINARY && itn->s == "...") {
                    Value lo = eval(itn->k[0]), hi = eval(itn->k[1]);
                    if (lo.t != Value::Num || hi.t != Value::Num) err(n, "intervalo precisa de numeros");
                    for (long i = (long)lo.n; i < (long)hi.n; i++) {
                        scopes.back()[n->s] = Value::num((double)i);
                        Flow f = exec(n->k[1]);
                        if (f == F_BREAK) break;
                        if (f == F_RETURN) { result = f; break; }
                    }
                } else {
                    Value it = eval(itn);
                    if (it.t != Value::Arr) err(n, "o 'for' precisa de um array ou intervalo");
                    std::vector<Value> items = *it.a;
                    for (auto& item : items) {
                        scopes.back()[n->s] = item;
                        Flow f = exec(n->k[1]);
                        if (f == F_BREAK) break;
                        if (f == F_RETURN) { result = f; break; }
                    }
                }
                scopes.resize(sz - 1);
                return result;
            }
            case S_RETURN:
                retVal = n->k.empty() ? Value() : eval(n->k[0]);
                return F_RETURN;
            case S_BREAK: return F_BREAK;
            case S_CONTINUE: return F_CONTINUE;
            case S_EXPR: eval(n->k[0]); return F_NORMAL;
            default: err(n, "instrucao invalida");
        }
    }
};

}  // namespace hx
