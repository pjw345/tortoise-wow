"""Package one patched DBC using the official StormLib native writer/reader."""
import argparse, ctypes as C, hashlib, json, pathlib

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stormlib',required=True)
    parser.add_argument('--candidate',required=True,type=pathlib.Path)
    args=parser.parse_args();root=args.candidate.resolve();source=root/'raw/Spell.dbc';output=root/'Data/patch-Z.MPQ'
    if output.exists():raise FileExistsError(output)
    output.parent.mkdir(exist_ok=True)
    lib=C.CDLL(args.stormlib);handle=C.c_void_p;word=C.c_uint32
    signatures={
        'SFileCreateArchive':[C.c_char_p,word,word,C.POINTER(handle)],
        'SFileAddFileEx':[handle,C.c_char_p,C.c_char_p,word,word,word],
        'SFileCloseArchive':[handle],
        'SFileOpenArchive':[C.c_char_p,word,word,C.POINTER(handle)],
        'SFileOpenFileEx':[handle,C.c_char_p,word,C.POINTER(handle)],
        'SFileReadFile':[handle,C.c_void_p,word,C.POINTER(word),C.c_void_p],
        'SFileCloseFile':[handle],
    }
    for name,types in signatures.items():getattr(lib,name).argtypes=types;getattr(lib,name).restype=C.c_bool
    archive=handle()
    assert lib.SFileCreateArchive(str(output).encode(),0,16,C.byref(archive)),'Create classic MPQ'
    try:assert lib.SFileAddFileEx(archive,str(source).encode(),b'DBFilesClient\\Spell.dbc',0x200,2,2),'Add compressed DBC'
    finally:assert lib.SFileCloseArchive(archive)
    archive=handle();file=handle();expected=source.read_bytes()
    assert lib.SFileOpenArchive(str(output).encode(),0,0x100,C.byref(archive)),'Read-only reopen'
    try:
        assert lib.SFileOpenFileEx(archive,b'DBFilesClient\\Spell.dbc',0,C.byref(file))
        try:
            buffer=C.create_string_buffer(len(expected));read=word()
            assert lib.SFileReadFile(file,buffer,len(expected),C.byref(read),None)
            assert read.value==len(expected) and buffer.raw==expected,'Native MPQ roundtrip differs'
        finally:assert lib.SFileCloseFile(file)
    finally:assert lib.SFileCloseArchive(archive)
    manifest=json.loads((root/'manifest.json').read_text());manifest['mpq_sha256']=hashlib.sha256(output.read_bytes()).hexdigest()
    manifest['mpq_bytes']=output.stat().st_size
    manifest['addon_sha256']={f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in (root/'Interface/AddOns/WarbandEnchantingRods').iterdir() if f.is_file()}
    manifest['packager']={'library':'StormLib v9.25','revision':'28c9b4be3f23c6b3a5ff55cacac7dbe5b9cdc4fc','archive_format':'classic MPQ v1 / header version 0','compression':'zlib','native_roundtrip':'byte-identical'}
    (root/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(json.dumps({key:manifest[key] for key in ['mpq_sha256','mpq_bytes','packager']},indent=2))

if __name__=='__main__':main()
