import argparse
import subprocess
import os


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument('--data_dir', default='./testcases/')
    parser.add_argument('--compiler_path', default='./build/compiler')
    parser.add_argument('--timeout', default=30, type=int)
    args = parser.parse_args()
    return args


def run_with(args):
    for mod in os.listdir(args.data_dir):
        mod_path = os.path.join(args.data_dir, mod)
        for filename in os.listdir(mod_path):
            if not filename.endswith(".sy"):
                continue
            full_path = os.path.join(mod_path, filename)
            asm_path = full_path[:-1]
            cmd = "{} {} -o {}".format(args.compiler_path, full_path, asm_path)
            print(cmd)
            child = subprocess.Popen(cmd.split(), stdout=subprocess.PIPE)
            try:
                child.communicate(timeout=args.timeout)
            except subprocess.TimeoutExpired:
                print("{} time out".format(full_path))
                exit(1)
            else:
                if child.returncode != 0:
                    print("return code :", child.returncode)
                    exit(1)


if __name__ == '__main__':
    args = parse_args()
    run_with(args)
