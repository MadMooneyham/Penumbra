#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Make Penumbra's site (served locally for now: python3 -m http.server in OUT_DIR):

  index.html                     what Penumbra is, and the way in
  install/index.html             the web installer (web/installer.html, with fm1pkg.js, fm1ota.js and the
                                 package's metadata inlined; Chrome or Edge, Web MIDI)
  firmware/penumbra-VERSION.fwsc the package the installer writes; also the download
  words/                         the screen reader's words files (fm1-reader), every one ever made

  tools/make_pages.py build/penumbra-0.7-beta.fwsc 0.7-beta OUT_DIR

The package must be a release build (./build.sh --release X.Y): its identity, FM-1_73XXYYZZ (0.x betas:
FM-1_9XXYYZZ), is what the installer checks the download against and what the FM-1 reports after the install."""
import html
import json
import re
import shutil
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SRC / "web"))
from make_site import product_of, strip_module  # noqa: E402  (Felucca's: the package format)

LANDING = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Penumbra for the FM-1</title>
<style>
  :root { color-scheme: dark; }
  body { margin: 0; background: #07080f; color: #ecebf6; font: 17px/1.5 system-ui, sans-serif; }
  main { max-width: 36rem; margin: 0 auto; padding: 3rem 1rem; }
  h1 { font-weight: 600; letter-spacing: .02em; margin: 0 0 .25rem; }
  .sub { color: #8a8fae; margin: 0 0 2rem; }
  a { color: #ff9646; }
  .go { display: inline-block; padding: .7rem 1.2rem; border: 1px solid #ff9646; border-radius: 8px; text-decoration: none; }
  .small { color: #8a8fae; font-size: 14px; margin-top: 2rem; }
</style>
</head>
<body>
<main>
<h1>Penumbra</h1>
<p class="sub">A space-drone instrument for the M-VAVE FM-1, by Lunar Eclipse Forge. __VERSION__</p>
<p>A deep five-voice drone held on a pedal, VOX far back in the shimmer, a looper that follows the scene,
and holds that let you play the sound itself: throws, bends, freeze, morph. Switch it on and the drone
swells in; you play over it.</p>
<p><a class="go" href="install/">Install __VERSION__</a> &nbsp; <a href="firmware/__PKG__">download the package</a></p>
<p class="small">To go back to the official firmware, the installer can write it, or use M-VAVE's updater, M-UPGRADE,
from <a href="https://www.m-vave.com/download">m-vave.com/download</a>. M-VAVE and FM-1 are trademarks of their owners.</p>
</main>
</body>
</html>
"""


def main(pkg, version, out):
    pkg, out = Path(pkg), Path(out)
    raw = pkg.read_bytes()
    product = product_of(raw)
    if not re.fullmatch(r"FM-1_73\d{6}|FM-1_9\d{6}", product):
        raise SystemExit(f"{pkg}: identity {product!r}: make a release build (./build.sh --release X.Y)")
    if b"FELUCCA-LOADER-1" not in raw:
        raise SystemExit(f"{pkg}: no update loader in it")
    name = f"penumbra-{re.sub(r'[^A-Za-z0-9.-]', '-', version)}.fwsc"
    if out.exists():
        shutil.rmtree(out)
    for d in ("install", "firmware"):
        (out / d).mkdir(parents=True)
    shutil.copy(pkg, out / "firmware" / name)
    page = (SRC / "web" / "installer.html").read_text(encoding="utf-8")
    lib = strip_module((SRC / "web" / "fm1pkg.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((SRC / "web" / "fm1ota.js").read_text(encoding="utf-8"))
    meta = json.dumps({"version": version, "product": product, "pkg": "../firmware/" + name})
    for mark in ("/*LIB*/", "/*META*/"):
        if page.count(mark) != 1:
            raise SystemExit(f"installer.html must contain {mark} once")
    (out / "install" / "index.html").write_text(page.replace("/*LIB*/", lib).replace("/*META*/", meta), encoding="utf-8")
    (out / "index.html").write_text(LANDING.replace("__VERSION__", html.escape(version)).replace("__PKG__", name),
                                    encoding="utf-8")
    shutil.copytree(SRC / "web" / "words", out / "words")
    (out / ".nojekyll").write_text("")
    print(f"site: {out}: index.html, install/ ({product}), firmware/{name} ({len(raw)} B), words/")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:4])
