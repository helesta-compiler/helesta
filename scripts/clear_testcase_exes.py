import argparse
import os


def parse_args():
    parser = argparse.ArgumentParser();
    parser.add_argument('--exes_dir', default='./testcases');
    args = parser.parse_args();
    return args;


def run_with(args):
    for mod in os.listdir(args.exes_dir):
        mod_path = os.path.join(args.exes_dir, mod)
        for filename in os.listdir(mod_path):
            if not filename.endswith(".sy"):
                continue
            exe_path = os.path.join(mod_path, filename[:-3])
            if os.path.exists(exe_path):
                print("removing {}".format(exe_path))
                os.remove(exe_path)


if __name__ == '__main__':
    args = parse_args()
    run_with(args)
