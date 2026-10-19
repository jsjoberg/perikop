#!/usr/bin/env python3
"""Local audition server with byte-range support for Safari media playback (MIT).

Standard library only. Bind to loopback and optionally proxy via Tailscale Serve.
"""
import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import re


class AuditionHandler(SimpleHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    extensions_map = {**SimpleHTTPRequestHandler.extensions_map, ".wav": "audio/wav"}

    def end_headers(self):
        # Avoid retaining the previous server's non-seekable media responses.
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def send_head(self):
        self.byte_range = None
        path = Path(self.translate_path(self.path))
        if not path.is_file():
            return super().send_head()
        try:
            source = path.open("rb")
        except OSError:
            self.send_error(404, "File not found")
            return None
        size = path.stat().st_size
        start, end = 0, size - 1
        request = self.headers.get("Range")
        if request:
            match = re.fullmatch(r"bytes=(\d*)-(\d*)", request.strip())
            valid = bool(match and any(match.groups()) and size)
            if valid:
                first, last = match.groups()
                if first:
                    start = int(first)
                    end = min(int(last), size - 1) if last else size - 1
                    valid = start <= end and start < size
                else:
                    suffix = int(last)
                    valid = suffix > 0
                    start = max(0, size - suffix)
            if not valid:
                source.close()
                self.send_response(416)
                self.send_header("Content-Range", f"bytes */{size}")
                self.send_header("Content-Length", "0")
                self.send_header("Accept-Ranges", "bytes")
                self.end_headers()
                return None
            self.byte_range = (start, end)
        self.send_response(206 if self.byte_range else 200)
        self.send_header("Content-Type", self.guess_type(str(path)))
        self.send_header("Accept-Ranges", "bytes")
        self.send_header("Content-Length", str(max(0, end - start + 1)))
        self.send_header("Last-Modified", self.date_time_string(path.stat().st_mtime))
        if self.byte_range:
            self.send_header("Content-Range", f"bytes {start}-{end}/{size}")
        self.end_headers()
        source.seek(start)
        return source

    def copyfile(self, source, output):
        if self.byte_range is None:
            return super().copyfile(source, output)
        remaining = self.byte_range[1] - self.byte_range[0] + 1
        while remaining:
            block = source.read(min(remaining, 64 * 1024))
            if not block:
                break
            output.write(block)
            remaining -= len(block)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", type=Path, required=True)
    parser.add_argument("--port", type=int, default=8766)
    args = parser.parse_args()
    with ThreadingHTTPServer(("127.0.0.1", args.port), partial(AuditionHandler, directory=args.directory.resolve())) as server:
        server.serve_forever()


if __name__ == "__main__":
    main()
