import argparse
import os
import shutil

def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input_dir", default="./src")
    parser.add_argument("--output_dir", default="./sub")
    args = parser.parse_args()
    return args


def process(line):
    parts = line.strip().split(" ")
    if len(parts) != 2:
        return line
    if parts[0] != "#include":
        return line
    if "sys/" in parts[1]:
        return line
    hd = parts[1][1:-1].split("/")[-1]
    return "#include \"" + hd + "\"\n"


def dfs(current_path, output_path):
    for file_or_dir in os.listdir(current_path):
        full_path = os.path.join(current_path, file_or_dir)
        if os.path.isfile(full_path) and full_path.endswith(('.cpp', '.h', '.hpp')):
            out_path = os.path.join(output_path, file_or_dir)
            content = ""
            with open(full_path, 'r') as f:
                for line in f:
                    content += process(line)
            with open(out_path, 'w') as f:
                f.write(content)
        if os.path.isdir(full_path):
            dfs(full_path, output_path)


def run_with(args):
    if not os.path.exists(args.output_dir):
        os.mkdir(args.output_dir)
    dfs(args.input_dir, args.output_dir)


if __name__ == '__main__':
    args = parse_args()
    run_with(args)
    shutil.rmtree("./src")
    shutil.rmtree("./testcases")
    shutil.rmtree(".gitignore")
    shutil.rmtree("build")
    shutil.rmtree("./third_party")
    shutil.rmtree("./scripts")
