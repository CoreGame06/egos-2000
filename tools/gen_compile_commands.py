#!/usr/bin/env python3
"""从 Makefile 的实际编译命令生成编辑器使用的 compile_commands.json。"""

import argparse
import json
from pathlib import Path
import shlex
import shutil
import subprocess


def find_compiler(name):
    found = shutil.which(name)
    if found:
        return str(Path(found).resolve())
    # 本地 xPack 安装不一定已经加入 PATH。
    if "/" not in name:
        candidates = sorted(Path.home().glob(f"xpack-*/bin/{name}"), reverse=True)
        for candidate in candidates:
            if candidate.is_file() and shutil.which(str(candidate)):
                return str(candidate.resolve())
    raise SystemExit(f"找不到编译器 {name}，请通过 RISCV_CC 或 CC 指定其路径。")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", default="riscv-none-elf-gcc")
    parser.add_argument("--host-cc", default="cc")
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    compiler = find_compiler(args.cc)
    host_cc = shlex.split(args.host_cc)
    host_cc[0] = find_compiler(host_cc[0])

    # -n 只打印命令，-B 确保已有构建产物也不会让编译命令被跳过。
    result = subprocess.run(
        ["make", "--no-print-directory", "-Bn", "install",
         f"RISCV_CC={compiler}", f"CC={shlex.join(host_cc)}"],
        cwd=root, text=True, capture_output=True, check=True,
    )
    commands = []
    for line in result.stdout.splitlines():
        words = shlex.split(line)
        if words and words[0] in (compiler, host_cc[0]):
            commands.append(words)

    # 公共库会分别链接到内核、系统服务、用户程序；编辑时优先采用内核的那份配置。
    commands.sort(key=lambda words: (
        0 if any(word.startswith("grass/") for word in words)
        else 1 if words[0] == compiler else 2
    ))
    entries = {}
    for words in commands:
        sources = [word for word in words[1:] if Path(word).suffix in (".c", ".s")]
        flags = []
        index = 1
        while index < len(words):
            word = words[index]
            if word in ("-o", "-T"):
                index += 2
                continue
            if word not in sources and not word.startswith(("-Wl,", "-l")) and word != "-nostdlib":
                flags.append(word)
            index += 1
        for source in sources:
            path = str(root / source)
            entries.setdefault(path, {
                "directory": str(root),
                "file": path,
                "arguments": [words[0], *flags, "-c", source],
            })

    if not entries:
        raise SystemExit("Makefile 没有输出可用的编译命令。")
    output = root / "compile_commands.json"
    output.write_text(json.dumps(list(entries.values()), ensure_ascii=False, indent=2) + "\n")
    print(f"已生成 {output.name}：{len(entries)} 个源码文件。")


if __name__ == "__main__":
    main()
