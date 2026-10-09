"""Small IDF image/table fixtures for OTA packaging validation."""
import struct

def write_monitor_images(directory):
    firmware=bytearray(256)
    firmware[0]=0xe9
    struct.pack_into('<I',firmware,32,0xabcd5432)
    firmware[48:53]=b'0.7.0'
    project=b'fnk0104b_firmware'
    firmware[80:80+len(project)]=project
    (directory/'firmware.bin').write_bytes(firmware)
    entries=(('ota_0',0,0x10,0x10000,0x400000),('ota_1',0,0x11,0x410000,0x400000),('model',1,0x82,0x810000,0x7f0000))
    (directory/'partitions.bin').write_bytes(b''.join(struct.pack('<HBBII16sI',0x50aa,kind,subtype,address,size,label.encode(),0) for label,kind,subtype,address,size in entries))
    (directory/'ota_data_initial.bin').write_bytes(b'\xff'*0x2000)
