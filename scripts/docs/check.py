#!/usr/bin/env python3
"""Check local documentation links and extract complete book programs.

Run --syntax-check inside a documented Docker SDK, never as a host build.
Generated sources and their source-location manifest stay under root build/.

MIT License (see: LICENSE)
Copyright (C) 2026 Tomaz Stih
"""

import argparse
import json
import re
import subprocess
import unicodedata
from pathlib import Path
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[2]
FENCES = re.compile(r"^```[^\n]*\n.*?^```[^\n]*(?:\n|$)", re.M | re.S)


def prose(text):
    """Remove fenced examples without shifting link diagnostic line numbers."""
    return FENCES.sub(lambda match: "\n" * match[0].count("\n"), text)


def anchors(text):
    """Resolve Markdown heading slugs, including repeated-heading suffixes."""
    result = set()
    seen = {}
    for title in re.findall(r"^#{1,6}\s+(.+)", prose(text), re.M):
        title = re.sub(r"\[([^\]]+)\]\([^)]*\)", r"\1", title)
        title = title.strip().rstrip("#").strip().lower()
        slug = "".join(
            char for char in title
            if char.isspace() or char in "-_"
            or unicodedata.category(char)[0] in "LN"
        ).replace(" ", "-")
        number = seen.get(slug, 0)
        seen[slug] = number + 1
        result.add(slug + (f"-{number}" if number else ""))
    result.update(re.findall(r'\bid=["\x27]([^"\x27]+)["\x27]', text))
    return result


def documents():
    """Collect maintained books, guides, skills, and asset documentation."""
    return sorted({
        ROOT / "README.md", ROOT / "AGENTS.md",
        *ROOT.joinpath("docs").rglob("*.md"),
        *ROOT.joinpath("src/assets").rglob("README.md"),
        ROOT / "lib/native/third_party/README.md",
    })


def check_links(files):
    """Check local files and Markdown fragments, without network requests."""
    failures = []
    count = 0
    for source in files:
        text = prose(source.read_text())
        links = list(re.finditer(
            r'\[[^\]\n]*\]\((<[^>]+>|[^\s)]+)(?:\s+"[^"]*")?\)', text))
        links += list(re.finditer(r"^\[[^\]]+\]:\s*(\S+)", text, re.M))
        for match in links:
            target = match[1].strip("<>")
            parts = urlsplit(target)
            if parts.scheme or parts.netloc:
                continue
            count += 1
            path = (source.parent / unquote(parts.path)).resolve()
            if not parts.path:
                path = source
            problem = None
            if not path.exists():
                problem = "missing file"
            elif parts.fragment and path.suffix == ".md":
                if unquote(parts.fragment) not in anchors(path.read_text()):
                    problem = "missing heading"
            if problem:
                line = text[:match.start()].count("\n") + 1
                failures.append(
                    f"{source.relative_to(ROOT)}:{line}: {problem}: {target}")
    print(f"Checked {count} local links in {len(files)} Markdown files.")
    for failure in failures:
        print(failure)
    return not failures


def extract_programs(destination):
    """Extract complete C++ programs and retain chapter/line provenance."""
    destination = destination.resolve()
    if not destination.is_relative_to(ROOT / "build"):
        raise ValueError("extracted programs must be under the root build/ directory")
    destination.mkdir(parents=True, exist_ok=True)
    result = []
    chapters = ROOT / "docs/manuals/programming-native"
    for source in sorted(chapters.glob("*.md")):
        text = source.read_text()
        for index, match in enumerate(
                re.finditer(r"```cpp\n(.*?)```", text, re.S), 1):
            if not re.search(r"\bint program\s*\(", match[1]):
                continue
            output = destination / f"{source.stem.lower()}-{index}.cpp"
            output.write_text(match[1])
            result.append({
                "chapter": str(source.relative_to(ROOT)),
                "line": text[:match.start()].count("\n") + 1,
                "file": str(output.relative_to(ROOT)),
            })
    (destination / "examples.json").write_text(json.dumps(result, indent=2) + "\n")
    print(f"Extracted {len(result)} complete book programs to "
          f"{destination.relative_to(ROOT)}.")
    return result


def main():
    """Check links, optionally extract, and syntax-check in an SDK container."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--extract", type=Path,
                        help="write complete programs and a manifest under build/")
    parser.add_argument("--syntax-check", action="store_true",
                        help="check complete programs using GCC inside a Docker SDK")
    args = parser.parse_args()
    success = check_links(documents())
    if args.extract or args.syntax_check:
        try:
            programs = extract_programs(
                ROOT / (args.extract or Path("build/doc-check/current")))
        except ValueError as error:
            parser.error(str(error))
        if args.syntax_check:
            for entry in programs:
                print(f"Checking {entry['chapter']}:{entry['line']}", flush=True)
                checked = subprocess.run([
                    "g++", "-std=c++20", "-Wall", "-Wextra", "-pedantic",
                    "-Werror", "-I", str(ROOT / "include"), "-fsyntax-only",
                    str(ROOT / entry["file"]),
                ], check=False)
                success = checked.returncode == 0 and success
    return 0 if success else 1


if __name__ == "__main__":
    raise SystemExit(main())
