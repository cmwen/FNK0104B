import hashlib
import importlib.util
from pathlib import Path
import struct
import unittest
spec=importlib.util.spec_from_file_location('model_bundle',Path(__file__).resolve().parents[2]/'apps/codex-monitor/tools/model_bundle.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)

def bundle(models):
    header_size=4+len(models)*36+sum(len(files)*40 for _,files in models)
    header=bytearray(struct.pack('<I',len(models)));payload=bytearray()
    for model,files in models:
        header+=struct.pack('<32sI',model,len(files))
        for name,data in files:
            header+=struct.pack('<32sII',name,header_size+len(payload),len(data));payload+=data
    return bytes(header+payload)

class ModelBundleTest(unittest.TestCase):
    def test_directory_and_file_order_do_not_change_compatibility_hash(self):
        a=bundle([(b'wake',[(b'weights',b'123'),(b'info',b'456')]),(b'vad',[(b'index',b'abc')])])
        b=bundle([(b'vad',[(b'index',b'abc')]),(b'wake',[(b'info',b'456'),(b'weights',b'123')])])
        self.assertNotEqual(hashlib.sha256(a).digest(),hashlib.sha256(b).digest())
        canonical=module.canonicalize(a)
        self.assertEqual(canonical,module.canonicalize(b))
        self.assertEqual(canonical,module.canonicalize(canonical))
        self.assertEqual(len(a),len(canonical))
        changed=bundle([(b'vad',[(b'index',b'abd')]),(b'wake',[(b'info',b'456'),(b'weights',b'123')])])
        self.assertNotEqual(canonical,module.canonicalize(changed))

    def test_truncation_overlap_and_duplicate_names_are_rejected(self):
        valid=bundle([(b'wake',[(b'weights',b'123'),(b'info',b'456')])])
        for data in [b'',valid[:20],valid[:-1],valid+b'padding',bundle([(b'wake',[(b'x',b'1'),(b'x',b'2')])])]:
            with self.assertRaises(ValueError):module.canonicalize(data)
        overlap=bytearray(valid);struct.pack_into('<I',overlap,72,0)
        with self.assertRaises(ValueError):module.canonicalize(overlap)
