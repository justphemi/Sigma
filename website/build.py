#!/usr/bin/env python3
"""Builds the Sigma documentation site.

    python3 website/build.py            # writes website/_site/
    python3 website/build.py --check    # also runs every code example with build/sig

Pages live in website/pages/*.html. Each starts with a comment holding its
title, followed by plain HTML. Code blocks are written as raw text (no HTML
escaping needed):

    <pre class="sigma">...</pre>                 Sigma code, checked with `sig check`
    <pre class="sigma" data-run>...</pre>        run it; the next <pre class="output">
    <pre class="output">...</pre>               block must match what it prints
    <pre class="sigma" data-error>...</pre>      must fail to compile; the next output
                                                 block must match the error message
    <pre class="sigma" data-file="x.sgm">        saved as x.sgm for later examples
    <pre class="sigma" data-skip>                shown but not checked
    <pre class="shell">...</pre>                terminal commands
    data-stdin="..."                             input fed to a data-run example
    data-args="a b"                              command-line arguments for data-run

The navigation order is NAV below.
"""
import html
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
OUT = os.path.join(HERE, "_site")
VERSION = "0.2.0"
REPO = "https://github.com/justphemi/Sigma"

NAV = [
    ("Getting started", ["index", "install", "first-program"]),
    ("Language guide", ["basics", "strings", "control-flow", "functions", "lists", "maps",
                        "errors", "modules", "input-output"]),
    ("Real projects", ["project-layout", "tutorial-cli", "testing", "shipping", "performance"]),
    ("Reference", ["syntax", "builtins", "methods", "cli", "upgrading", "faq"]),
]

KEYWORDS = {"fr", "nah", "grind", "in", "dip", "skip", "cook", "serve", "lock", "yolo",
            "oops", "yeet", "yoink", "and", "or", "not"}
CONSTANTS = {"true", "false", "nocap", "cap", "nil"}
BUILTINS = {"yap", "ask", "len", "type", "num", "int", "str", "abs", "floor", "ceil", "round",
            "sqrt", "pow", "min", "max", "rand", "rand_int", "now", "sleep", "args", "exit",
            "read_file", "write_file", "append_file", "file_exists", "vibecheck"}


# ---------------------------------------------------------------- highlighting

TOKEN = re.compile(r"""
    (?P<block>^[ \t]*---[ \t]*\n.*?^[ \t]*---[ \t]*$)
  | (?P<comment>--[^\n]*)
  | (?P<str>"(?:\\.|[^"\\\n])*"|'(?:\\.|[^'\\\n])*')
  | (?P<num>\b0[xX][0-9A-Fa-f_]+\b|\b\d[\d_]*(?:\.\d[\d_]*)?(?:[eE][+-]?\d+)?\b)
  | (?P<word>[A-Za-z_][A-Za-z0-9_]*)
  | (?P<op>=>|\.\.=?|\+\+|[-+*/%]=|\*\*|==|!=|<=|>=|&&|\|\||[-+*/%<>!:])
  | (?P<other>.|\n)
""", re.X | re.S | re.M)


def span(cls, text):
    return f'<span class="{cls}">{html.escape(text, quote=False)}</span>'


def highlight_string(s):
    # Colour the {expr} parts of an interpolated string as code.
    out, i, buf = [], 0, ""
    while i < len(s):
        c = s[i]
        if c == "\\" and i + 1 < len(s):
            buf += s[i:i + 2]
            i += 2
            continue
        if c == "{":
            depth, j = 1, i + 1
            while j < len(s) and depth:
                depth += {"{": 1, "}": -1}.get(s[j], 0)
                j += 1
            if buf:
                out.append(span("s", buf))
                buf = ""
            out.append(span("p", "{") + highlight_sigma(s[i + 1:j - 1]) + span("p", "}"))
            i = j
            continue
        buf += c
        i += 1
    if buf:
        out.append(span("s", buf))
    return "".join(out)


def highlight_sigma(code):
    out = []
    for m in TOKEN.finditer(code):
        kind, text = m.lastgroup, m.group()
        if kind in ("block", "comment"):
            out.append(span("c", text))
        elif kind == "str":
            out.append(highlight_string(text))
        elif kind == "num":
            out.append(span("n", text))
        elif kind == "word":
            rest = code[m.end():]
            if text in KEYWORDS:
                out.append(span("k", text))
            elif text in CONSTANTS:
                out.append(span("n", text))
            elif text in BUILTINS and rest.lstrip(" ").startswith("("):
                out.append(span("b", text))
            elif rest.startswith("("):
                out.append(span("f", text))
            else:
                out.append(html.escape(text))
        elif kind == "op":
            out.append(span("o", text))
        else:
            out.append(html.escape(text, quote=False))
    return "".join(out)


def highlight_shell(code):
    lines = []
    for line in code.split("\n"):
        if line.startswith("#"):
            lines.append(span("c", line))
        else:
            lines.append(html.escape(line, quote=False))
    return "\n".join(lines)


# ---------------------------------------------------------------- pages

PRE = re.compile(r'<pre class="(sigma|output|shell)"([^>]*)>(.*?)</pre>', re.S)


def dedent_block(text):
    text = text.strip("\n")
    lines = text.split("\n")
    pad = min((len(l) - len(l.lstrip(" ")) for l in lines if l.strip()), default=0)
    return "\n".join(l[pad:] for l in lines)


def attrs_of(s):
    return dict((k, html.unescape(v or "")) for k, v in re.findall(r'([\w-]+)(?:="([^"]*)")?', s))


def load_pages():
    pages = {}
    for section, slugs in NAV:
        for slug in slugs:
            path = os.path.join(HERE, "pages", slug + ".html")
            src = open(path, encoding="utf-8").read()
            m = re.match(r"\s*<!--(.*?)-->", src, re.S)
            meta = dict(re.findall(r"^\s*(\w+):\s*(.+?)\s*$", m.group(1), re.M)) if m else {}
            body = src[m.end():] if m else src
            pages[slug] = {"slug": slug, "section": section, "title": meta.get("title", slug),
                           "desc": meta.get("description", ""), "body": body}
    return pages


def slugify(text):
    text = re.sub(r"<[^>]+>", "", text)
    text = html.unescape(text).lower()
    return re.sub(r"[^a-z0-9]+", "-", text).strip("-")


def render_body(body):
    """Highlight code blocks and give headings ids. Returns (html, headings)."""
    blocks = []

    def code(m):
        kind, attrs, text = m.group(1), m.group(2), dedent_block(m.group(3))
        a = attrs_of(attrs)
        blocks.append((kind, a, text))
        if kind == "sigma":
            inner = highlight_sigma(text)
            label = a.get("data-file") or "sigma"
        elif kind == "shell":
            inner = highlight_shell(text)
            label = "terminal"
        else:
            inner = html.escape(text, quote=False)
            label = "output"
        cls = {"sigma": "code", "shell": "code shell", "output": "code out"}[kind]
        return (f'<div class="{cls}"><div class="code-label">{html.escape(label)}</div>'
                f'<pre><code>{inner}</code></pre></div>')

    out = PRE.sub(code, body)
    headings, used = [], set()

    def head(m):
        level, attrs, text = m.group(1), m.group(2), m.group(3)
        hid = re.search(r'id="([^"]+)"', attrs)
        hid = hid.group(1) if hid else slugify(text)
        base, n = hid, 2
        while hid in used:
            hid, n = f"{base}-{n}", n + 1
        used.add(hid)
        headings.append((int(level), hid, re.sub(r"<[^>]+>", "", text)))
        return (f'<h{level} id="{hid}">{text}'
                f'<a class="anchor" href="#{hid}" aria-label="Link to this section">#</a></h{level}>')

    out = re.sub(r"<h([23])([^>]*)>(.*?)</h\1>", head, out, flags=re.S)
    return out, headings, blocks


def page_order():
    return [slug for _, slugs in NAV for slug in slugs]


def sidebar(pages, current):
    parts = []
    for section, slugs in NAV:
        parts.append(f'<div class="nav-section"><div class="nav-title">{section}</div><ul>')
        for s in slugs:
            cls = ' class="active" aria-current="page"' if s == current else ""
            parts.append(f'<li><a href="{s}.html"{cls}>{html.escape(pages[s]["title"])}</a></li>')
        parts.append("</ul></div>")
    return "".join(parts)


def toc(headings):
    items = [f'<li class="l{lvl}"><a href="#{hid}">{html.escape(html.unescape(t))}</a></li>'
             for lvl, hid, t in headings]
    if len(items) < 2:
        return ""
    return '<nav class="toc" aria-label="On this page"><div class="toc-title">On this page</div><ul>' + \
        "".join(items) + "</ul></nav>"


TEMPLATE = open(os.path.join(HERE, "template.html"), encoding="utf-8").read()


def build(pages):
    if os.path.isdir(OUT):
        shutil.rmtree(OUT)
    os.makedirs(OUT)
    shutil.copytree(os.path.join(HERE, "assets"), os.path.join(OUT, "assets"))
    shutil.copy(os.path.join(ROOT, "icons", "sigma.svg"), os.path.join(OUT, "assets", "sigma.svg"))
    open(os.path.join(OUT, ".nojekyll"), "w").close()

    order = page_order()
    index, all_blocks = [], {}
    for i, slug in enumerate(order):
        p = pages[slug]
        body, headings, blocks = render_body(p["body"])
        all_blocks[slug] = blocks
        prev_link = next_link = ""
        if i > 0:
            q = pages[order[i - 1]]
            prev_link = f'<a class="prev" href="{q["slug"]}.html"><span>Previous</span>{html.escape(q["title"])}</a>'
        if i + 1 < len(order):
            q = pages[order[i + 1]]
            next_link = f'<a class="next" href="{q["slug"]}.html"><span>Next</span>{html.escape(q["title"])}</a>'
        title = "Sigma" if slug == "index" else f'{p["title"]} | Sigma'
        page_html = (TEMPLATE
                     .replace("{{title}}", html.escape(title))
                     .replace("{{description}}", html.escape(p["desc"] or "Documentation for the Sigma programming language."))
                     .replace("{{sidebar}}", sidebar(pages, slug))
                     .replace("{{toc}}", toc(headings))
                     .replace("{{section}}", html.escape(p["section"]))
                     .replace("{{prev}}", prev_link)
                     .replace("{{next}}", next_link)
                     .replace("{{version}}", VERSION)
                     .replace("{{repo}}", REPO)
                     .replace("{{edit}}", f'{REPO}/blob/main/website/pages/{slug}.html')
                     .replace("{{content}}", body))
        open(os.path.join(OUT, slug + ".html"), "w", encoding="utf-8").write(page_html)
        index.extend(search_entries(p, body))

    with open(os.path.join(OUT, "search.json"), "w", encoding="utf-8") as f:
        json.dump(index, f, ensure_ascii=False, separators=(",", ":"))
    return all_blocks


def text_of(fragment):
    fragment = re.sub(r'<div class="code-label">.*?</div>', " ", fragment, flags=re.S)
    fragment = re.sub(r'<a class="anchor".*?</a>', "", fragment, flags=re.S)
    return re.sub(r"\s+", " ", html.unescape(re.sub(r"<[^>]+>", " ", fragment))).strip()


def search_entries(page, body):
    """One entry per section of the page (split at h2/h3)."""
    entries = []
    parts = re.split(r'(<h[23] id="[^"]+">.*?</h[23]>)', body, flags=re.S)
    heading, anchor = page["title"], ""
    for part in parts:
        m = re.match(r'<h[23] id="([^"]+)">(.*?)</h[23]>', part, re.S)
        if m:
            anchor, heading = m.group(1), text_of(m.group(2))
            continue
        text = text_of(part)
        if text or anchor:
            entries.append({"p": page["title"], "h": heading,
                            "u": page["slug"] + ".html" + ("#" + anchor if anchor else ""),
                            "t": text[:5000]})
    return entries


# ---------------------------------------------------------------- checks

EMOJI = re.compile("[\U0001F000-\U0001FAFF☀-➿️]")


def lint_output():
    problems = []
    for dirpath, _, files in os.walk(OUT):
        for name in files:
            if not name.endswith((".html", ".css", ".js", ".json")):
                continue
            path = os.path.join(dirpath, name)
            text = open(path, encoding="utf-8").read()
            for ch, what in (("—", "em dash"), ("–", "en dash")):
                if ch in text:
                    problems.append(f"{os.path.relpath(path, OUT)}: contains an {what}")
            if "gradient" in text.lower():
                problems.append(f"{os.path.relpath(path, OUT)}: contains a gradient")
            for m in EMOJI.finditer(text):
                line = text[:m.start()].count("\n") + 1
                problems.append(f"{os.path.relpath(path, OUT)}:{line}: contains an emoji")
    return problems


def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        f.write(text + "\n")


def run_examples(all_blocks):
    sig = os.path.join(ROOT, "build", "sig")
    if not os.access(sig, os.X_OK):
        sys.exit("build Sigma first (./build.sh) to check the examples")
    env = dict(os.environ)
    failures, count = [], 0
    for slug, blocks in all_blocks.items():
        with tempfile.TemporaryDirectory() as d:
            for kind, a, text in blocks:
                if kind == "sigma" and "data-file" in a:
                    write(os.path.join(d, a["data-file"]), text)
            for i, (kind, a, text) in enumerate(blocks):
                if kind != "sigma" or "data-skip" in a:
                    continue
                count += 1
                name = a.get("data-file", "main.sgm")
                write(os.path.join(d, name), text)
                expected = None
                if i + 1 < len(blocks) and blocks[i + 1][0] == "output":
                    expected = blocks[i + 1][2]
                where = f"{slug}.html, example {count}: {text.splitlines()[0][:50]!r}"
                if "data-error" in a:
                    r = subprocess.run([sig, "check", name], cwd=d, capture_output=True, text=True, env=env)
                    got = r.stderr.strip()
                    if r.returncode == 0:
                        failures.append(f"{where}\n  expected a compile error, but it compiled")
                    elif expected is not None and got != expected.strip():
                        failures.append(f"{where}\n  expected error:\n{expected}\n  got:\n{got}")
                elif "data-run" in a:
                    cmd = [sig, name] + a.get("data-args", "").split()
                    stdin = a.get("data-stdin", "").replace("\\n", "\n")
                    r = subprocess.run(cmd, cwd=d, input=stdin, capture_output=True, text=True, env=env,
                                       timeout=60)
                    got = (r.stdout + r.stderr).rstrip()
                    if expected is None:
                        failures.append(f"{where}\n  data-run example has no output block after it")
                    elif got != expected.rstrip():
                        failures.append(f"{where}\n  expected:\n{expected}\n  got:\n{got}")
                else:
                    r = subprocess.run([sig, "check", name], cwd=d, capture_output=True, text=True, env=env)
                    if r.returncode != 0:
                        failures.append(f"{where}\n{r.stderr}")
    return count, failures


def main():
    pages = load_pages()
    blocks = build(pages)
    problems = lint_output()
    for p in problems:
        print("style:", p)
    print(f"built {len(pages)} pages into {os.path.relpath(OUT, ROOT)}/")
    if "--check" in sys.argv:
        count, failures = run_examples(blocks)
        for f in failures:
            print("\nFAIL", f)
        print(f"checked {count} examples, {len(failures)} failed")
        if failures:
            sys.exit(1)
    if problems:
        sys.exit(1)


if __name__ == "__main__":
    main()
