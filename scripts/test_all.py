import os
import subprocess
import argparse

def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--testcase_path", default="testcases")
    parser.add_argument("--lib_path", default="/home/pi/github-action/libsysy.a")
    args = parser.parse_args()
    return args


if __name__ == '__main__':
    args = parse_args()
    for mod in os.listdir(args.testcase_path):
        mod_path = os.path.join(args.testcase_path, mod)
        for testcase in os.listdir(mod_path):
            if not testcase.endswith(".sy"):
                continue
            testcase = testcase[:-3]
            asm_file = os.path.join(mod_path, testcase + ".s")
            in_file = os.path.join(mod_path, testcase + ".in")
            out_file = os.path.join(mod_path, testcase + ".out")
            exe_file = os.path.join(mod_path, testcase)
            link_cmd = "g++ {} {} -o {}".format(asm_file, args.lib_path, exe_file)
            run_cmd = "./{}".format(exe_file)
            print(link_cmd)
            child = subprocess.Popen(link_cmd.split(), stdout=subprocess.PIPE)
            child.communicate()
            print(run_cmd)
            child = None
            if os.path.exists(in_file):
                with open(in_file) as f:
                    child = subprocess.Popen(run_cmd.split(), stdout=subprocess.PIPE, stdin=f)
            else:
                child = subprocess.Popen(run_cmd.split(), stdout=subprocess.PIPE)
            out, _ = child.communicate()
            out = out.decode("utf-8")
            out = out.strip("\n\r")
            out += "\n" + str(child.returncode)
            with open(out_file) as stdout:
                std = stdout.read()
                std = std.strip()
                out = out.strip()
                if out != std:
                    print("my output: \n{}\nstd output: \n{}".format(out, std))
                    exit(1)
                else:
                    print("[{}] passed".format(testcase))
