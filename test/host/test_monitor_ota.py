import hashlib
import importlib
from pathlib import Path
import sys
import tempfile
import unittest
from ota_fixture import write_monitor_images
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'scripts'))
package=importlib.import_module('package_web_firmware')
ci=importlib.import_module('firmware_ci')

class MonitorOtaTest(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path=Path(self.temp.name)
        write_monitor_images(self.path)
        (self.path/'srmodels').mkdir()
        (self.path/'srmodels/srmodels.bin').write_bytes(b'speech model bytes')

    def test_manifest_uses_compiled_identity_and_exact_image_hashes(self):
        manifest=package.monitor_ota_manifest(self.path)
        self.assertEqual('0.7.0',manifest['version'])
        self.assertEqual('codex-monitor',manifest['project'])
        self.assertEqual('monitor-ota4m-model810000-v1',manifest['layout'])
        for filename,size,digest in [('firmware.bin','size','sha256'),('srmodels/srmodels.bin','models_size','models_sha256')]:
            data=(self.path/filename).read_bytes()
            self.assertEqual(len(data),manifest[size])
            self.assertEqual(hashlib.sha256(data).hexdigest(),manifest[digest])

    def test_bad_app_identity_and_wrong_partition_table_cannot_publish_ota(self):
        for offset,value in [(0,b'\x00'),(32,b'\x00\x00\x00\x00'),(48,b'-1.00'),(80,b'wrong_project\0')]:
            write_monitor_images(self.path)
            data=bytearray((self.path/'firmware.bin').read_bytes())
            data[offset:offset+len(value)]=value
            (self.path/'firmware.bin').write_bytes(data)
            with self.assertRaises(ValueError):package.monitor_ota_manifest(self.path)
        write_monitor_images(self.path)
        data=bytearray((self.path/'partitions.bin').read_bytes())
        data[4:8]=(0x20000).to_bytes(4,'little')
        (self.path/'partitions.bin').write_bytes(data)
        with self.assertRaisesRegex(ValueError,'partition identity'):package.monitor_ota_manifest(self.path)

    def test_legacy_catalog_keeps_original_layout_without_new_ota_metadata(self):
        import json
        target=self.path/'codex-monitor';target.mkdir()
        names=set(ci.image_names('codex-monitor'))-{'ota_data_initial.bin'}
        images={}
        for name in names:
            file=target/name;file.parent.mkdir(parents=True,exist_ok=True);file.write_bytes(b'legacy')
            images[name]=hashlib.sha256(file.read_bytes()).hexdigest()
        old={'schema':1,'environment':'codex-monitor','fingerprint':'old','version':'old-build','images':images,
             'layout':{'parts':[{'path':name,'offset':0x610000 if name.startswith('srmodels') else 0x10000 if name=='firmware.bin' else 0x8000 if name=='partitions.bin' else 0} for name in sorted(names)],'app_limit':0x600000,'model_limit':0x9f0000}}
        (target/'metadata.json').write_text(json.dumps(old))
        self.assertEqual(old,ci.verify(self.path,'codex-monitor'))
