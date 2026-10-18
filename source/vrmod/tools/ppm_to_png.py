"""Lossless preview conversion using only Python's standard library."""
import pathlib,struct,sys,zlib
def convert(path):
    with path.open('rb') as f:
        assert f.readline()==b'P6\n'
        w,h=map(int,f.readline().split());assert f.readline()==b'255\n';pixels=f.read()
    assert len(pixels)==w*h*3
    def chunk(t,p):return struct.pack('>I',len(p))+t+p+struct.pack('>I',zlib.crc32(t+p)&0xffffffff)
    scan=b''.join(b'\0'+pixels[y*w*3:(y+1)*w*3] for y in range(h))
    out=path.with_suffix('.png');out.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(scan))+chunk(b'IEND',b''));print(out)
for arg in sys.argv[1:]:convert(pathlib.Path(arg))
