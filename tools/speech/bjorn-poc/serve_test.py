"""HTTP contract Safari needs for seekable audio; no optional dependencies."""
from functools import partial
from http.client import HTTPConnection
from http.server import ThreadingHTTPServer
from pathlib import Path
from tempfile import TemporaryDirectory
from threading import Thread
import unittest

from serve import AuditionHandler


class QuietHandler(AuditionHandler):
    def log_message(self, *_args):
        pass


class RangeTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = TemporaryDirectory()
        cls.content = b"RIFF" + bytes(range(256)) * 4
        Path(cls.folder.name, "sample.wav").write_bytes(cls.content)
        cls.server = ThreadingHTTPServer(("127.0.0.1", 0), partial(QuietHandler, directory=cls.folder.name))
        cls.thread = Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()
        cls.thread.join()
        cls.folder.cleanup()

    def request(self, method="GET", byte_range=None):
        connection = HTTPConnection(*self.server.server_address, timeout=5)
        connection.request(method, "/sample.wav", headers={"Range": byte_range} if byte_range else {})
        response = connection.getresponse()
        result = response.status, dict(response.getheaders()), response.read()
        connection.close()
        return result

    def test_safari_two_byte_probe_is_partial_content(self):
        status, headers, body = self.request(byte_range="bytes=0-1")
        self.assertEqual(status, 206)
        self.assertEqual(headers["Content-Range"], f"bytes 0-1/{len(self.content)}")
        self.assertEqual(headers["Accept-Ranges"], "bytes")
        self.assertEqual(headers["Content-Type"], "audio/wav")
        self.assertEqual(headers["Content-Length"], "2")
        self.assertEqual(body, self.content[:2])

    def test_open_ended_and_suffix_ranges(self):
        for header, expected in (("bytes=100-", self.content[100:]), ("bytes=-16", self.content[-16:]),
                                 ("bytes=1000-99999", self.content[1000:])):
            with self.subTest(header=header):
                status, headers, body = self.request(byte_range=header)
                self.assertEqual(status, 206)
                self.assertEqual(body, expected)
                self.assertEqual(int(headers["Content-Length"]), len(expected))

    def test_invalid_range_is_416(self):
        for header in ("bytes=99999-", "bytes=5-1", "bytes=-0", "bytes=-", "bytes=0-1,4-5"):
            with self.subTest(header=header):
                status, headers, body = self.request(byte_range=header)
                self.assertEqual(status, 416)
                self.assertEqual(headers["Content-Range"], f"bytes */{len(self.content)}")
                self.assertEqual(body, b"")

    def test_full_get_and_head_keep_the_correct_length(self):
        status, headers, body = self.request()
        self.assertEqual(status, 200)
        self.assertEqual(body, self.content)
        status, headers, body = self.request("HEAD", "bytes=0-1")
        self.assertEqual(status, 206)
        self.assertEqual(headers["Content-Length"], "2")
        self.assertEqual(body, b"")


if __name__ == "__main__":
    unittest.main()
