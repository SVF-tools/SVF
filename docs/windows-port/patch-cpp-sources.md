---
title: "C++ Source Patches — Complete Details"
tags:
  - svf
  - documentation
  - build
  - llvm
  - testing
  - windows
  - api
---

# C++ Source Patches — Complete Details

This file contains the exact diffs to be applied to the three C++ files that use POSIX-only APIs not available on native Windows.

---

## File 1: `svf/lib/Util/SVFUtil.cpp`

### Diff — include (line 36)

```diff
-#include <sys/resource.h>		/// increase stack size
+#ifndef _WIN32
+#include <sys/resource.h>		/// increase stack size
+#endif
```

### Diff — `increaseStackSize()` function (line 229)

```diff
 void SVFUtil::increaseStackSize()
 {
+#ifndef _WIN32
     const rlim_t kStackSize = 256L * 1024L * 1024L;   // min stack size = 256 Mb
     struct rlimit rl;
     int result = getrlimit(RLIMIT_STACK, &rl);
     if (result == 0)
     {
         if (rl.rlim_cur < kStackSize)
         {
             rl.rlim_cur = kStackSize;
             result = setrlimit(RLIMIT_STACK, &rl);
             if (result != 0)
                 writeWrnMsg("setrlimit returned result !=0 \n");
         }
     }
+#endif
 }
```

**Motivation:** `getrlimit`/`setrlimit` with `RLIMIT_STACK` do not exist on Windows. On Windows, the stack grows dynamically and the limit is set at compile-time via linker flags (`/STACK:N` with MSVC or MinGW). The function becomes a no-op on Windows; to increase the stack, see the script configurations.

---

## File 2: `svf/lib/Util/ExtAPI.cpp`

### Diff — includes (lines 35–37)

```diff
-#include <sys/stat.h>
 #include "SVFIR/SVFVariables.h"
-#include <dlfcn.h>
+#ifdef _WIN32
+#  include <sys/types.h>
+#  include <sys/stat.h>
+#  define stat  _stat
+#  define popen  _popen
+#  define pclose _pclose
+#else
+#  include <sys/stat.h>
+#endif
+#include "SVFIR/SVFVariables.h"
```

**Motivations:**

- `dlfcn.h` was included but not used (no call to `dlopen`, `dlsym`, `dlclose`, `dlerror` in the file). Removed completely.
- `sys/stat.h` on MSVC uses `_stat` instead of `stat`. On MinGW, `stat` is directly available, but the `#defines` guarantee future compatibility with builds using clang-cl or MSVC.
- `popen`/`pclose` on MSVC are named `_popen`/`_pclose`. The `#define` avoids touching the code that calls them in the `GetStdoutFromCommand` function.

---

## File 3: `svf/include/MemoryModel/PointerAnalysis.h`

### Diff — includes (lines 33–34)

```diff
-#include <unistd.h>
-#include <signal.h>
+#ifndef _WIN32
+#  include <unistd.h>
+#  include <signal.h>
+#endif
```

**Motivations:**

- `unistd.h` does not exist on MSVC. MinGW provides it, but it is a public SVF header, so the guard is required for anyone using SVF as an external library with non-MinGW compilers.
- `signal.h` exists on Windows but with a reduced subset of signals (only `SIGABRT`, `SIGFPE`, `SIGILL`, `SIGINT`, `SIGSEGV`, `SIGTERM`). If POSIX-only signals (`SIGUSR1`, `SIGPIPE`, etc.) are used in the SVF code, they must be wrapped separately.

### Verifying the usage of `signal.h`

Before applying the patch, verify which symbols from `signal.h` are used in the files that include `PointerAnalysis.h`:

```bash
grep -rn "signal\|SIGTERM\|SIGUSR\|SIG_" \
    svf/include/MemoryModel/ \
    svf/lib/MemoryModel/ \
    --include="*.h" --include="*.cpp"
```

If the used signals are only `SIGINT`/`SIGTERM`/`SIGABRT`, add `#include <signal.h>` in the `_WIN32` branch as well (they are supported).

---

## Recommended application order

1. `svf/include/MemoryModel/PointerAnalysis.h` — public header, must be done first to avoid breaking incremental builds
2. `svf/lib/Util/SVFUtil.cpp`
3. `svf/lib/Util/ExtAPI.cpp`

## Linux regression test after patches

```bash
# From the repository root on Linux:
./build.sh
cd Release-build
ctest --output-on-failure   # if Test-Suite is present
bin/wpa --help              # smoke test
```
