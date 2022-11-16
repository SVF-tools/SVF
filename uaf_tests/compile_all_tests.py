import sys
import os
from glob import glob
import subprocess as sp

testcases = [
    "uaf_ctf_1",
    "uaf_ctf_2",
    "uaf00",
    "uaf0",
    "uaf2",
    "uaf3",
    "uaf4",
    "uaf5",
    "uaf34",
    "uaf35",
    "uaf36",
    "uaf37",
    "uaf38",
    "uaf39",
    "uaf40",
    "uaf41",
    "uaf42",
    "uaf43",
    "uaf44",
    "uaf45",
    "uaf46",
    "list"
]

for test in testcases:
    cfile = f"{test}/{test}.c"
    llfile = f"{test}/{test}.ll"
    res = f"{test}/{test}_results.txt"
    out = f"{test}/{test}_out.txt"
    clang_cmd = ["clang","-g", "-S", "-c", "-Xclang", "-disable-O0-optnone", "-fno-discard-value-names", "-emit-llvm", cfile, "-o", llfile]
    test_cmd = ["../Release-Build/bin/saber", "--fspta", "--dangling", llfile]
    # mem2reg_cmd = ["opt", "-S", "-mem2reg", llfile, "-o", llfile]
    sp.call(clang_cmd)
    with open(res, 'w') as resfile, open(out, 'w') as outfile:
        sp.call(test_cmd, stdout = outfile, stderr = resfile)