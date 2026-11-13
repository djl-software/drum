"""Render every page and verify bookmarks, contents, text bounds and encoding."""
from pathlib import Path
import argparse,json
import pdfplumber,pypdfium2 as pdfium
from pypdf import PdfReader
from PIL import Image,ImageDraw,ImageOps
root=Path(__file__).resolve().parents[1]
args=argparse.ArgumentParser();args.add_argument('pdf',type=Path,nargs='?',default=root.parent/'Mac/output/pdf/DJL-Drum-User-Manual-0.26.2-macOS.pdf');args.add_argument('--review-dir',type=Path);options=args.parse_args()
pdf=options.pdf.resolve();out=options.review_dir or root/'tmp/pdfs'/('review-'+pdf.stem);out.mkdir(parents=True,exist_ok=True)
for pattern in ["page-*.png","sheet-*.png"]:
 for stale in out.glob(pattern):stale.unlink()
reader=PdfReader(pdf);toc=json.loads(pdf.with_suffix('.contents.json').read_text())
assert len(reader.outline)==len(toc)
for entry,(_,title,page) in zip(reader.outline,toc):assert entry.title==title and reader.get_destination_page_number(entry)+1==page
text='\n'.join(p.extract_text() for p in reader.pages)
if 'User-Manual' in pdf.name:
 for term in ['Independent send','Settings','minus-infinity','Float','16-band','Source','0.26.2','32','Main'] :assert term.lower() in text.lower(),term
 assert '0.12.3' not in text
else:
 for term in ['79','99','rent-to-own','churn','Kilohearts','assumptions']:assert term.lower() in text.lower(),term
assert not any(bad in text for bad in ['\u00c2','\u00c3','\u00e2\u20ac']),'Encoding corruption'
links=0
with pdfplumber.open(pdf) as book:
 for i,page in enumerate(book.pages):
  links+=len(page.annots or [])
  if i==0:continue
  for char in page.chars:
   assert char['x0']>=44 and char['x1']<=552,(i+1,'horizontal overflow',char['text'],char['x0'],char['x1'])
   assert char['top']>=15 and char['bottom']<=825,(i+1,'vertical overflow')
assert links>=len(toc)
doc=pdfium.PdfDocument(str(pdf));thumbs=[]
for i,page in enumerate(doc):
 im=page.render(scale=1.5).to_pil().convert('RGB');im.save(out/f'page-{i+1:02}.png')
 thumb=ImageOps.contain(im,(595,842));card=Image.new('RGB',(615,870),'#dce3e5');card.paste(thumb,((615-thumb.width)//2,5));ImageDraw.Draw(card).text((14,851),f'Page {i+1}',fill='black');thumbs.append(card)
for start in range(0,len(thumbs),4):
 sheet=Image.new('RGB',(1230,1740),'white')
 for j,thumb in enumerate(thumbs[start:start+4]):sheet.paste(thumb,((j%2)*615,(j//2)*870))
 sheet.save(out/f'sheet-{start//4+1:02}.png')
(out/'validation.json').write_text(json.dumps({'pages':len(doc),'bookmarks':len(toc),'links':links,'bounds':'pass'},indent=2)+'\n')
print(f'Rendered {len(doc)} pages; verified {len(toc)} bookmarks, {links} links, coverage, encoding and text bounds. Review: {out}')
