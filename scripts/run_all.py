import argparse
import subprocess
import os


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument('--data_dir', default='./testcases/functional')
    parser.add_argument('--compiler_path', default='./build/compiler')
    args = parser.parse_args()
    return args


def run_with(args):
    for filename in os.listdir(args.data_dir):
        if not filename.endswith(".sy"):
            continue
        full_path = os.path.join(args.data_dir, filename)
        asm_path = full_path[:-1]
        cmd = "{} {} -o {}".format(args.compiler_path, full_path, asm_path)
        print(cmd)
        subprocess.Popen(cmd.split(), stdout=subprocess.PIPE)


if __name__ == '__main__':
    args = parse_args()
    run_with(args)
