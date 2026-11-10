#!/usr/bin/env python3
"""ETCH -- by Synth Centric.  Render docs/ETCH_Manual.md to PDF (optional;
needs the Python 'markdown' package and Node.js with Playwright/Chromium)."""
import os
import sys

import markdown
import subprocess

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
CSS = """
body { font-family: 'DejaVu Sans', Arial, sans-serif; font-size: 10.5pt; color: #111; margin: 0 8mm; }
h1 { font-size: 26pt; letter-spacing: 6px; margin-bottom: 0; }
h2 { border-bottom: 2px solid #111; padding-bottom: 2px; margin-top: 22px; page-break-after: avoid; }
h3 { margin-top: 0; }
table { border-collapse: collapse; margin: 8px 0; width: 100%; page-break-inside: avoid; }
td, th { border: 1px solid #999; padding: 3px 6px; vertical-align: top; }
th { background: #222; color: #fff; }
code, pre { font-family: 'DejaVu Sans Mono', monospace; background: #eee; }
pre { padding: 6px; }
blockquote { border-left: 4px solid #111; margin: 8px 0; padding: 4px 10px; background: #f3f3f3; page-break-inside: avoid; }
img { image-rendering: pixelated; width: 240px; border: 1px solid #111; }
"""


def main():
    src = os.path.join(ROOT, "docs", "ETCH_Manual.md")
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "release", "ETCH_Manual.pdf")
    html = markdown.markdown(open(src, encoding="utf-8").read(), extensions=["tables", "fenced_code"])
    page = "<html><head><meta charset='utf-8'><style>%s</style></head><body>%s</body></html>" % (CSS, html)
    tmp = os.path.join(ROOT, "docs", "_manual.html")
    open(tmp, "w", encoding="utf-8").write(page)
    js = """
const { chromium } = require('playwright');
(async () => {
  const b = await chromium.launch();
  const pg = await b.newPage();
  await pg.goto('file://' + process.argv[1]);
  await pg.pdf({ path: process.argv[2], format: 'A4',
    margin: { top: '15mm', bottom: '15mm', left: '10mm', right: '10mm' },
    displayHeaderFooter: true, headerTemplate: '<span></span>',
    footerTemplate: "<div style='font-size:8px;width:100%;text-align:center'>ETCH \u2014 A Generative Acid Machine \u2014 by Synth Centric \u2014 <span class='pageNumber'></span></div>" });
  await b.close();
})();
"""
    subprocess.check_call(["node", "-e", js, tmp, os.path.abspath(out)])
    os.remove(tmp)
    print("wrote", out)


if __name__ == "__main__":
    main()
