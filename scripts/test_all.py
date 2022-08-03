import os
import subprocess
import argparse
import time
import tempfile
import pandas as pd

def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--testcase_path", default="testcases")
    parser.add_argument("--lib_path", default="/home/pi/github-action/libsysy.a")
    parser.add_argument("--lib_src_path", default="/home/pi/github-action/sylib.c")
    parser.add_argument("--include_path", default="/home/pi/github-action/sylib.h")
    parser.add_argument("--benchmark", action='store_true')
    parser.add_argument("--benchmark_summary_path", default='/home/pi/github-action/summary.md')
    parser.add_argument("--benchmark_data_path", default='/home/pi/data/performance/')
    args = parser.parse_args()
    return args

def run(exe_path, in_path):
    child = None
    start = time.time()
    if os.path.exists(in_path):
        with open(in_file) as f:
            child = subprocess.Popen(exe_path.split(), stdout=subprocess.PIPE, stdin=f)
    else:
        child = subprocess.Popen(exe_path.split(), stdout=subprocess.PIPE)
    out, _ = child.communicate()
    end = time.time()
    out = out.decode("utf-8")
    out = out.strip("\n\r ")
    out += "\n" + str(child.returncode)
    return out, end - start

def run_with(compiler, src_path, in_path, lib_src_path, include_path):
    print("benchmark {} with {}".format(src_file, compiler))
    exe_path = os.path.join(tempfile.mkdtemp(), "exe")
    compile_cmd = "{} -x c++ {} {} -include {} -o {} -O2".format(compiler, src_file, lib_src_path, include_path, exe_path)
    child = subprocess.Popen(compile_cmd.split(), stdout=subprocess.PIPE)
    child.communicate()
    return run(exe_path, in_path)

if __name__ == '__main__':
    args = parse_args()
    results = []
    hele_sum = 0.0
    gcc_sum = 0.0
    clang_sum = 0.0
    for mod in os.listdir(args.testcase_path):
        mod_path = os.path.join(args.testcase_path, mod)
        if args.benchmark and mod != "performance":
            continue
        if not args.benchmark and mod == "performance":
            continue
        for testcase in os.listdir(mod_path):
            if not testcase.endswith(".sy"):
                continue
            testcase = testcase[:-3]
            asm_file = os.path.join(mod_path, testcase + ".s")
            in_file = os.path.join(mod_path, testcase + ".in")
            if args.benchmark:
                in_file = os.path.join(args.benchmark_data_path, testcase + ".in")
            src_file = os.path.join(mod_path, testcase + ".sy")
            out_file = os.path.join(mod_path, testcase + ".out")
            if args.benchmark:
                out_file = os.path.join(args.benchmark_data_path, testcase + ".out")
            exe_file = os.path.join(mod_path, testcase)
            std = None
            out = None
            if os.path.exists(asm_file):
                link_cmd = "g++ {} {} -o {}".format(asm_file, args.lib_path, exe_file)
                run_cmd = "{}".format(exe_file)
                print(link_cmd)
                child = subprocess.Popen(link_cmd.split(), stdout=subprocess.PIPE)
                child.communicate()
                print(run_cmd)
                out, elapsed = run(run_cmd, in_file)
                os.remove(asm_file)
                out = out.strip('\n\r ')
                with open(out_file) as stdout:
                    std = stdout.read()
                    std = std.strip('\n\r ')
                    lines = std.split('\n')
                    if len(lines) > 1:
                        without_returncode = '\n'.join(lines[:-1])
                        without_returncode = without_returncode.strip('\n\r ')
                        std = '\n'.join([without_returncode, lines[-1].strip('\n\r ')])
                if out != std:
                    print("my output: \n{}\nstd output: \n{}".format(out, std))
                    if not args.benchmark:
                        exit(1)
                else:
                    print("[{}] passed".format(testcase))
            else:
                if not args.benchmark:
                    exit(1)
            if args.benchmark:
                result = {}
                result['testcase'] = testcase
                result['passed'] = (out is not None) and (out == std)
                result['hele elapsed'] = elapsed
                hele_sum += elapsed
                _, elapsed = run_with('g++', src_file, in_file, args.lib_src_path, args.include_path)
                gcc_sum += elapsed
                result['gcc elapsed'] = elapsed
                _, elapsed = run_with('clang++', src_file, in_file, args.lib_src_path, args.include_path)
                clang_sum += elapsed
                result['clang elapsed'] = elapsed
                results.append(result)
    if not args.benchmark:
        exit(0)
    with open(args.benchmark_summary_path, 'w') as f:
        f.write("## Overall\n\n")
        f.write(pd.DataFrame([{"hele": hele_sum, "gcc": gcc_sum, "clang": clang_sum}]).to_markdown() + "\n\n")
        f.write("## Cases\n\n")
        f.write(pd.DataFrame(results).to_markdown() + "\n\n")
