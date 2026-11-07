#!/usr/bin/env bash
set -euo pipefail

BIN="${1:-./build/minicpp}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

expect_run() {
    local file="$1"
    local expected="$2"
    local actual
    actual="$("$BIN" "$ROOT/examples/$file" --run)"
    [[ "$actual" == "Program returned: $expected" ]] || {
        echo "FAIL: $file (expected $expected, got: $actual)" >&2
        exit 1
    }
}


expect_output() {
    local actual="$1"
    local expected="$2"
    [[ "$actual" == "$expected" ]] || {
        echo "FAIL: output mismatch (expected: [$expected], got: [$actual])" >&2
        exit 1
    }
}

expect_codegen() {
    local file="$1"
    local expected="$2"
    local tmp
    tmp="$(mktemp)"
    "$BIN" "$ROOT/examples/$file" --emit-python "$tmp" >/dev/null
    local actual
    actual="$(python3 "$tmp")"
    rm -f "$tmp"
    [[ "$actual" == "$expected" ]] || {
        echo "FAIL: generated Python for $file (expected $expected, got: $actual)" >&2
        exit 1
    }
}

# Interpreter tests.
expect_run demo.cpp 120
expect_run functions.cpp 1
expect_run while.cpp 4
expect_run void.cpp 10
expect_run codegen_edge_cases.cpp 42

# Built-in print() must write the same values in the interpreter.
PRINT_EXPECTED=$'10\n3.14\nTrue\nProgram returned: 0'
PRINT_ACTUAL="$("$BIN" "$ROOT/examples/print.cpp" --run)"
expect_output "$PRINT_ACTUAL" "$PRINT_EXPECTED"

# Generated Python must produce the same results.
expect_codegen demo.cpp 120
expect_codegen functions.cpp 1
expect_codegen while.cpp 4
expect_codegen void.cpp 10
expect_codegen codegen_edge_cases.cpp 42

# The generated Python must print the same values and final return value.
PRINT_PY="$(mktemp)"
"$BIN" "$ROOT/examples/print.cpp" --emit-python "$PRINT_PY" >/dev/null
PRINT_PY_OUTPUT="$(python3 "$PRINT_PY")"
rm -f "$PRINT_PY"
expect_output "$PRINT_PY_OUTPUT" $'10\n3.14\nTrue\n0'

# Check that 64-bit integer literals survive AST construction and translation.
TMP_SOURCE="$(mktemp)"
TMP_PY="$(mktemp)"
cat > "$TMP_SOURCE" <<'CPP'
int main() {
    return 9007199254740993;
}
CPP
"$BIN" "$TMP_SOURCE" --run | grep -qx 'Program returned: 9007199254740993'
"$BIN" "$TMP_SOURCE" --emit-python "$TMP_PY" >/dev/null
python3 "$TMP_PY" | grep -qx '9007199254740993'
rm -f "$TMP_SOURCE" "$TMP_PY"

# Built-in print() must reject invalid argument counts and expression use.
TMP_BAD="$(mktemp)"
cat > "$TMP_BAD" <<'CPP'
int main() { print(); return 0; }
CPP
if "$BIN" "$TMP_BAD" --check >/dev/null 2>&1; then
    echo "FAIL: print() without an argument was accepted" >&2
    exit 1
fi
cat > "$TMP_BAD" <<'CPP'
int main() { int x = print(1); return 0; }
CPP
if "$BIN" "$TMP_BAD" --check >/dev/null 2>&1; then
    echo "FAIL: print() used as an expression was accepted" >&2
    exit 1
fi
cat > "$TMP_BAD" <<'CPP'
void print(int value) { return; }
int main() { return 0; }
CPP
if "$BIN" "$TMP_BAD" --check >/dev/null 2>&1; then
    echo "FAIL: redefining built-in print() was accepted" >&2
    exit 1
fi
rm -f "$TMP_BAD"

# Both invalid programs must be rejected.
if "$BIN" "$ROOT/tests/syntax_error.cpp" --check >/dev/null 2>&1; then
    echo "FAIL: syntax_error.cpp was accepted" >&2
    exit 1
fi
if "$BIN" "$ROOT/tests/semantic_error.cpp" --check >/dev/null 2>&1; then
    echo "FAIL: semantic_error.cpp was accepted" >&2
    exit 1
fi

echo "All MiniCpp interpreter and Python-codegen smoke tests passed."
