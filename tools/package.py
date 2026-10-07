from pathlib import Path
import hashlib,zipfile,shutil
root=Path(__file__).resolve().parents[1];out=root/'downloads/Next-Level-Tracker.zip';out.parent.mkdir(exist_ok=True)
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED) as z:
 for p in sorted(root.rglob('*')):
  rel=p.relative_to(root)
  if p.is_file() and rel.parts[0] not in {'.git','.github','downloads'} and '__pycache__' not in rel.parts and p.suffix not in {'.pyc','.pdb','.lib'}:z.write(p,rel.as_posix())
with zipfile.ZipFile(out) as z:
 assert z.testzip() is None
 assert 'mod.yml' in z.namelist()
shutil.copy2(root/'images/banner.gif',root/'downloads/Next-Level-Tracker.gif')
(root/'downloads/Next-Level-Tracker-SHA256SUMS.txt').write_text(''.join(hashlib.sha256(f.read_bytes()).hexdigest()+'  '+f.name+'\n' for f in [out,root/'downloads/Next-Level-Tracker.gif']))
print(out)
