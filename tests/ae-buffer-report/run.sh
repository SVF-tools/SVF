#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
root=$(cd "$(dirname "$0")/../.." && pwd)
: "${LLVM_DIR:?Set LLVM_DIR to an existing LLVM installation}"
build=${SVF_BUILD_DIR:-"$root/Release-build"}
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT
cd "$temporary"

for scenario in stack heap dynamic range safe copy copy_safe copy_offset set string_copy string_cat string_ncat; do
    "$LLVM_DIR/bin/clang" -g -O0 -Xclang -disable-O0-optnone -fno-builtin \
        -D"${scenario^^}" -S -emit-llvm "$root/tests/ae-buffer-report/access.c" -o raw.ll
    "$LLVM_DIR/bin/opt" -S -passes=mem2reg raw.ll -o input.ll
    for mode in dense semi-sparse; do
        if output=$("$build/bin/ae" -overflow -stat=false "-ae-sparsity=$mode" \
            "-extapi=$build/lib/extapi.bc" input.ll 2>&1); then
            :
        else
            status=$?
            printf 'AE failed: %s / %s (status %s)\n%s\n' "$scenario" "$mode" "$status" "$output"
            exit "$status"
        fi
        case "$scenario" in
            safe|copy_safe)
                if grep -qE 'Full Overflow|Buffer Overflow' <<< "$output"; then
                    printf '%s\n' "$output"
                    exit 1
                fi
                ;;
            *)
                case "$scenario" in
                    stack|heap|dynamic) expected='allocate size : [8, 8], access size : [9, 9]' ;;
                    range) expected='allocate size : [8, 8], access size : [8, 9]' ;;
                    copy) expected='allocate size : [4, 4], access size : [7, 7]' ;;
                    copy_offset) expected='allocate size : [4, 4], access size : [5, 5]' ;;
                    set) expected='allocate size : [8, 8], access size : [8, 8]' ;;
                    string_copy) expected='allocate size : [4, 4], access size : [8, 8]' ;;
                    # Detection follows the call transfer: AE scans the modified
                    # destination up to its size (4), then adds the source/count (4).
                    string_cat|string_ncat) expected='allocate size : [4, 4], access size : [8, 8]' ;;
                esac
                if ! grep -Fq "$expected" <<< "$output" ||
                   [[ $(grep -c 'Full Overflow' <<< "$output") != 1 ]] ||
                   ! grep -Fq 'Buffer Overflow (1 found)' <<< "$output" ||
                   ! grep -q '"ln":.*"cl":.*"fl":' <<< "$output"; then
                    printf 'Failed: %s / %s; expected %s\n%s\n' "$scenario" "$mode" "$expected" "$output"
                    exit 1
                fi
                ;;
        esac
        printf 'PASS %s / %s\n' "$scenario" "$mode"
    done
done

# Exercise the shared report representation independently of AE's transfer
# functions, including endpoints that cannot be represented as finite s64_t.
: "${Z3_ROOT:?Set Z3_ROOT to an existing Z3 installation}"
"$LLVM_DIR/bin/clang++" -std=c++17 -UNDEBUG -DEXPERIMENTAL_KEY_INSTRUCTIONS \
    -I"$root/svf/include" -I"$build/include" -I"$LLVM_DIR/include" -I"$Z3_ROOT/include" \
    "$root/tests/ae-buffer-report/bounds.cpp" \
    -L"$build/lib" -L"$LLVM_DIR/lib" -L"$Z3_ROOT/bin" \
    -Wl,-rpath,"$LLVM_DIR/lib:$Z3_ROOT/bin" -lSvfCore -lz3 -lLLVM -o check-bounds
output=$(./check-bounds 2>&1)
grep -Fq 'allocate size : [-oo, +oo], access size : [-oo, +oo]' <<< "$output"
printf 'PASS finite and infinite report bounds\n'
