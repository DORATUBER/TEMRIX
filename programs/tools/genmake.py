#!/usr/bin/env python3

from pathlib import Path

manifest = Path("programs.manifest")

programs = []
sources = {}

current = None

for line in manifest.read_text().splitlines():
    line = line.rstrip()

    if not line or line.startswith("#"):
        continue

    if not line.startswith((" ", "\t")):
        current = line[:-1]
        programs.append(current)
        sources[current] = []
    else:
        sources[current].append(line.strip())

print("PROGRAMS := " + " ".join(programs))
print()

for prog in programs:
    print(f"{prog}_SRCS := \\")
    for src in sources[prog][:-1]:
        print(f"\t{src} \\")
    print(f"\t{sources[prog][-1]}")
    print()