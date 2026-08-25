#!/usr/bin/env python3
"""
Relic Core WASM demo dev server.

Serves the captive-portal assets (src/access-point/assets/) as the live web
root, and maps the WASM modules (sss.js/sss.wasm/qr.js/qr.wasm) from
demo/scripts/.  This way, edits made in src/access-point/assets/ are reflected
immediately without having to regenerate the pages/ directory.

Usage:
    python3 demo/serve.py [PORT]

Then open http://localhost:<PORT>/index.html in your browser.
"""

import http.server
import os
import sys

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8000
ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), '..', 'src',
                                     'access-point', 'assets'))
WASM_DIR = os.path.normpath(os.path.join(os.path.dirname(__file__), 'scripts'))


class Handler(http.server.SimpleHTTPRequestHandler):
    extensions_map = {
        **http.server.SimpleHTTPRequestHandler.extensions_map,
        '.wasm': 'application/wasm',
    }

    def translate_path(self, path):
        path = path.split('?', 1)[0].split('#', 1)[0]
        parts = [p for p in path.lstrip('/').split('/') if p]

        # Map the WASM demo modules (sss.js/sss.wasm/qr.js/qr.wasm/
        # compression.js/compression.wasm/qr_decode.js/qr_decode.wasm) from
        # demo/scripts/ into the /scripts/ namespace.
        if parts[:1] == ['scripts'] and parts[-1] in ('sss.js', 'sss.wasm',
                                                      'qr.js', 'qr.wasm',
                                                      'compression.js',
                                                      'compression.wasm',
                                                      'qr_decode.js',
                                                      'qr_decode.wasm'):
            return os.path.join(WASM_DIR, *parts[1:])

        # Everything else is served live from the captive-portal assets.
        return os.path.join(ROOT, *parts)


if __name__ == '__main__':
    os.chdir(ROOT)
    http.server.ThreadingHTTPServer(('0.0.0.0', PORT), Handler).serve_forever()

