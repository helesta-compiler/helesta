import argparse
import subprocess
import os
import shutil


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument('--data_dir', default='./testcases/')
    parser.add_argument('--compiler_path', default='./build/compiler')
    parser.add_argument('--fuzz', action='append')
    args = parser.parse_args()
    return args


def run_with(args):
    for mod in os.listdir(args.data_dir):
        mod_path = os.path.join(args.data_dir, mod)
        for o in args.fuzz:
            if mod == "performance":
                continue
            o_path = os.path.join(args.data_dir, mod + "_" + o)
            os.mkdir(o_path)
        for filename in os.listdir(mod_path):
            if not filename.endswith(".sy"):
                continue
            full_path = os.path.join(mod_path, filename)
            in_path = os.path.join(mod_path, filename[:-2] + "in")
            out_path = os.path.join(mod_path, filename[:-2] + "out")
            asm_path = full_path[:-1]
            cmd = "{} {} -o {}".format(args.compiler_path, full_path, asm_path)
            print(cmd)
            child = subprocess.Popen(cmd.split(), stdout=subprocess.PIPE)
            child.communicate()
            if mod != 'performance' and child.returncode != 0:
                print("return code :", child.returncode)
                exit(1)
            if mod != 'performance':
                for o in args.fuzz:
                    o_path = os.path.join(args.data_dir, mod + "_" + o)
                    shutil.copy(full_path, os.path.join(o_path, filename[:-2] + "sy"))
                    if os.path.exists(in_path):
                        shutil.copy(in_path, os.path.join(o_path, filename[:-2] + "in"))
                    if os.path.exists(out_path):
                        shutil.copy(out_path, os.path.join(o_path, filename[:-2] + "out"))
                    asm_path = os.path.join(o_path, filename[:-2] + "s")
                    cmd = "{} {} -o {} --{}".format(args.compiler_path, full_path, asm_path, o)
                    print(cmd)
                    child = subprocess.Popen(cmd.split(), stdout=subprocess.PIPE)
                    child.communicate()
                    if child.returncode != 0:
                        exit(1)


if __name__ == '__main__':
    args = parse_args()
    run_with(args)
