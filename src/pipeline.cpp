#include "pipeline.hpp"
#include "ast.hpp"
#include "codegen.hpp"
#ifdef MINICPP_FALLBACK_PARSER
#include "fallback_parser.hpp"
#else
#include "parser.tab.h"
#endif

#include <memory>
#include <sstream>
#include <stdexcept>

#ifndef MINICPP_FALLBACK_PARSER
using YY_BUFFER_STATE = struct yy_buffer_state*;
extern YY_BUFFER_STATE yy_scan_bytes(const char* bytes, int len);
extern void yy_delete_buffer(YY_BUFFER_STATE buffer);
extern int yyparse(void);
extern int yylineno;
extern mini::Program* g_program;
extern std::vector<std::string> g_parse_errors;
#endif

namespace mini {

CompileResult compileSource(const std::string& source, bool executeProgram) {
    CompileResult result;
    std::unique_ptr<Program> program;
#ifdef MINICPP_FALLBACK_PARSER
    FallbackParseOutput parsed = parseWithFallbackParser(source);
    result.syntaxErrors = std::move(parsed.errors);
    program = std::move(parsed.program);
#else
    g_program = nullptr;
    g_parse_errors.clear();
    yylineno = 1;
    YY_BUFFER_STATE buffer = yy_scan_bytes(source.data(), static_cast<int>(source.size()));
    int parseStatus = yyparse();
    yy_delete_buffer(buffer);
    result.syntaxErrors = g_parse_errors;
    if (parseStatus == 0 && g_program && result.syntaxErrors.empty()) {
        program.reset(g_program);
        g_program = nullptr;
    } else {
        delete g_program;
        g_program = nullptr;
    }
#endif
    if (!program || !result.syntaxErrors.empty()) return result;
    result.parsed = true;
    result.astJson = astToJson(*program);

    SemanticAnalyzer analyzer;
    result.semanticOk = analyzer.analyze(*program);
    result.semanticErrors = analyzer.errors();
    if (!result.semanticOk) return result;

    PythonCodeGenerator generator;
    result.pythonCode = generator.generate(*program);
    if (executeProgram) {
        try {
            Interpreter interpreter(*program);
            result.result = interpreter.run();
        } catch (const RuntimeError& error) {
            result.runtimeError = error.what();
        } catch (const std::exception& error) {
            result.runtimeError = error.what();
        }
    }
    return result;
}

std::string compileResultToJson(const CompileResult& result) {
    std::ostringstream out;
    out << "{\"parsed\":" << (result.parsed ? "true" : "false")
        << ",\"semanticOk\":" << (result.semanticOk ? "true" : "false")
        << ",\"syntaxErrors\":[";
    for (std::size_t i = 0; i < result.syntaxErrors.size(); ++i) {
        if (i) out << ',';
        out << "\"" << jsonEscape(result.syntaxErrors[i]) << "\"";
    }
    out << "],\"semanticErrors\":[";
    for (std::size_t i = 0; i < result.semanticErrors.size(); ++i) {
        if (i) out << ',';
        out << "{\"line\":" << result.semanticErrors[i].line << ",\"message\":\""
            << jsonEscape(result.semanticErrors[i].message) << "\"}";
    }
    out << "],\"runtimeError\":\"" << jsonEscape(result.runtimeError) << "\",\"result\":";
    if (result.result) {
        if (auto* v = std::get_if<std::int64_t>(&*result.result)) out << *v;
        else if (auto* v = std::get_if<double>(&*result.result)) out << *v;
        else out << (std::get<bool>(*result.result) ? "true" : "false");
    } else out << "null";
    out << ",\"resultText\":\"";
    if (result.result) out << jsonEscape(Interpreter::valueToString(*result.result));
    out << "\",\"python\":\"" << jsonEscape(result.pythonCode) << "\",\"ast\":";
    if (result.parsed) out << result.astJson; else out << "null";
    out << '}';
    return out.str();
}

} // namespace mini
