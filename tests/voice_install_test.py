"""Exercise offline pack installation, integrity checks, and failed-install cleanup."""
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1]/'tools/speech/install_voice.py'
spec = importlib.util.spec_from_file_location('voice_installer',SCRIPT)
installer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(installer)

class Installation(unittest.TestCase):
    def test_kokoro_offline(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory); source=root/'source'; source.mkdir(); data=root/'data'
            files={}
            for name in installer.KOKORO_FILES:
                content=name.encode(); (source/name).write_bytes(content)
                files[name]={'size':len(content),'sha256':hashlib.sha256(content).hexdigest()}
            manifest={'id':installer.KOKORO_ID,'files':files}
            (source/'voice-pack.json').write_text(json.dumps(manifest))
            command=[sys.executable,str(SCRIPT),'--pack','kokoro','--data-dir',str(data)]
            # Corrupt one source before installation. No partial pack becomes visible.
            (source/'alice.bin').write_bytes(b'corrupt')
            result=subprocess.run(command+['--source',str(source)],capture_output=True,text=True)
            self.assertNotEqual(result.returncode,0)
            self.assertFalse((data/'voices'/installer.KOKORO_ID).exists())
            self.assertEqual(list((data/'voices').iterdir()),[])
            (source/'alice.bin').write_bytes(b'alice.bin')
            subprocess.run(command+['--source',str(source)],check=True,capture_output=True)
            subprocess.run(command+['--verify'],check=True,capture_output=True)
            (data/'voices'/installer.KOKORO_ID/'bjorn.bin').write_bytes(b'corrupt')
            self.assertNotEqual(subprocess.run(command+['--verify'],capture_output=True).returncode,0)
            manifest['id']='../escape'; (source/'voice-pack.json').write_text(json.dumps(manifest))
            self.assertNotEqual(subprocess.run(command+['--source',str(source)],capture_output=True).returncode,0)

if __name__=='__main__': unittest.main()
