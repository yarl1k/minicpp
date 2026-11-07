#include "pipeline.hpp"
#include "server.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace {
void printUsage() {
    std::cout << "MiniCpp Compiler\n\n"
              << "Usage:\n"
              << "  minicpp <source.cpp> [--check]\n"
              << "  minicpp <source.cpp> --run\n"
              << "  minicpp <source.cpp> --ast <output.json>\n"
              << "  minicpp <source.cpp> --emit-python <output.py>\n"
              << "  minicpp --serve [port]\n\n"
              << "The source language is a small C++-like subset, not full ISO C++.\n";
}
std::string readSource(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot open source file: " + path);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}
void writeOutput(const std::string& path, const std::string& content) {
    if (path == "-") { std::cout << content; return; }
    std::ofstream output(path, std::ios::binary);
    if (!output) throw std::runtime_error("cannot write output file: " + path);
    output << content;
    if (!output) throw std::runtime_error("failed while writing output file: " + path);
}
}

int main(int argc, char** argv) {
    try {
        if (argc < 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h") {
            printUsage(); return argc < 2 ? 1 : 0;
        }
        if (std::string(argv[1]) == "--serve") {
            int port = argc >= 3 ? std::stoi(argv[2]) : 8080;
            if (port < 1 || port > 65535) throw std::runtime_error("port must be between 1 and 65535");
            return runHttpServer(port);
        }

        const std::string sourcePath = argv[1];
        std::string mode = "--check";
        std::string outputPath;
        if (argc >= 3) mode = argv[2];
        if ((mode == "--ast" || mode == "--emit-python") && argc < 4)
            throw std::runtime_error("missing output path for " + mode);
        if (argc >= 4) outputPath = argv[3];
        if (mode != "--check" && mode != "--run" && mode != "--ast" && mode != "--emit-python")
            throw std::runtime_error("unknown option: " + mode);

        std::string source = readSource(sourcePath);
        mini::CompileResult result = mini::compileSource(source, mode == "--run");
        if (!result.parsed) {
            std::cerr << "Syntax analysis failed:\n";
            for (const auto& error : result.syntaxErrors) std::cerr << "  " << error << '\n';
            return 2;
        }
        if (!result.semanticOk) {
            std::cerr << "Semantic analysis failed:\n";
            for (const auto& error : result.semanticErrors)
                std::cerr << "  line " << error.line << ": " << error.message << '\n';
            return 3;
        }
        if (!result.runtimeError.empty()) {
            std::cerr << "Runtime error: " << result.runtimeError << '\n';
            return 4;
        }

        if (mode == "--run") {
            if (result.result) std::cout << "Program returned: " << mini::Interpreter::valueToString(*result.result) << '\n';
            else std::cout << "Program finished (void result).\n";
        } else if (mode == "--ast") {
            writeOutput(outputPath, result.astJson + "\n");
            std::cout << "AST exported to " << outputPath << '\n';
        } else if (mode == "--emit-python") {
            writeOutput(outputPath, result.pythonCode);
            std::cout << "Python code generated at " << outputPath << '\n';
        } else {
            std::cout << "Syntax analysis: OK\nSemantic analysis: OK\n";
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
