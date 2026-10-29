"""Render all manual pages and check links, bounds, required coverage and version."""
from pathlib import Path
import json,re
import pdfplumber,pypdfium2 as pdfium
from pypdf import PdfReader
from PIL import Image,ImageDraw,ImageOps
root=Path(__file__).resolve().parents[1]
pdf=root/"output/pdf/DJL-Drum-User-Manual-0.10.2-Windows.pdf"
out=root/"tmp/pdfs/review-0.10.2";out.mkdir(parents=True,exist_ok=True)
reader=PdfReader(pdf);toc=json.loads(pdf.with_suffix(".contents.json").read_text())
assert len(reader.outline)==36
for entry,(_,title,page) in zip(reader.outline,toc):
    assert entry.title==title and reader.get_destination_page_number(entry)+1==page
text="\n".join(p.extract_text() for p in reader.pages)
for word in ["Why this matters","32","Camelot","Choke","Waveform","Macro","Save","Loop","Click","0.10.2","3A"]:
    assert word in text,word
assert not any(bad in text for bad in ["\u00c2", "\u00c3", "\u00e2\u20ac"]), "Encoding corruption"
assert "0.8.0" not in text and "METER PRO" not in text and "\\'" not in text
links=0
with pdfplumber.open(pdf) as book:
    for i,page in enumerate(book.pages):
        links+=len(page.annots or [])
        for char in page.chars:
            if i==0:continue
            assert char["x0"]>=44 and char["x1"]<=552,(i+1,"horizontal overflow",char["text"],char["x0"],char["x1"])
            assert char["top"]>=15 and char["bottom"]<=825,(i+1,"vertical overflow")
assert links>=40
doc=pdfium.PdfDocument(str(pdf));thumbs=[]
for i,page in enumerate(doc):
    im=page.render(scale=1.5).to_pil().convert("RGB");im.save(out/f"page-{i+1:02}.png")
    thumb=ImageOps.contain(im,(595,842));card=Image.new("RGB",(615,870),"#dce3e5");card.paste(thumb,((615-thumb.width)//2,5))
    ImageDraw.Draw(card).text((14,851),f"Page {i+1}",fill="black");thumbs.append(card)
for start in range(0,len(thumbs),4):
    sheet=Image.new("RGB",(1230,1740),"white")
    for j,thumb in enumerate(thumbs[start:start+4]):sheet.paste(thumb,((j%2)*615,(j//2)*870))
    sheet.save(out/f"sheet-{start//4+1:02}.png")
print(f"Rendered {len(doc)} pages; verified 36 bookmarks, {links} links and all text bounds.")
