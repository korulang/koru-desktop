import struct
c  = b'\x48\xc7\xc0\x01\x00\x00\x00'
c += b'\x48\xc7\xc7\x01\x00\x00\x00'
c += b'\x48\x8d\x35' + b'XXXX'
c += b'\x48\xc7\xc2\x0d\x00\x00\x00'
c += b'\x0f\x05'
c += b'\x48\xc7\xc0\x3c\x00\x00\x00'
c += b'\x48\xc7\xc7\x2a\x00\x00\x00'
c += b'\x0f\x05'
msg = b'hello-linuxu\n'
lea_disp = 46 - 21
c = c[:17] + struct.pack('<i', lea_disp) + c[21:] + msg
ehsz,phsz = 64,56; entry = ehsz+phsz
ident = b'\x7fELF'+bytes([2,1,1,0])+b'\x00'*8
ehdr = ident + struct.pack('<HHIQQQIHHHHHH', 3,0x3e,1,entry,64,0,0,ehsz,phsz,1,0,0,0)
phdr = struct.pack('<IIQQQQQQ', 1,5,0,0,0,ehsz+phsz+len(c),ehsz+phsz+len(c),0x1000)
open('rootfs/bin/hello','wb').write(ehdr+phdr+c)
