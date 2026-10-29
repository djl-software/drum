"""Reusable Meter-style DJL manual renderer. Product facts live in the JSON manuscript."""
import argparse, json, io
from pathlib import Path
from xml.sax.saxutils import escape
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, Image, PageBreak, Table, TableStyle, Flowable, KeepTogether
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib import colors
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from PIL import Image as PILImage
from pypdf import PdfReader

W,H=595.276,841.89
CONTENT=W-92
INK=colors.HexColor("#17252d"); MINT=colors.HexColor("#6fe0c1"); MUTED=colors.HexColor("#58707c"); LIGHT=colors.HexColor("#edf4f5"); DARK=colors.HexColor("#10171d")
def render(manuscript,output,font_dir):
    data=json.loads(manuscript.read_text(encoding="utf-8-sig")); base=manuscript.parent
    for name,file in [("Segoe","segoeui.ttf"),("SegoeBold","segoeuib.ttf"),("SegoeItalic","segoeuii.ttf")]:
        pdfmetrics.registerFont(TTFont(name,str(font_dir/file)))
    pdfmetrics.registerFontFamily("Segoe",normal="Segoe",bold="SegoeBold",italic="SegoeItalic",boldItalic="SegoeBold")
    styles={
      "toc":ParagraphStyle("toc",fontName="Segoe",fontSize=9.2,leading=12,textColor=INK,spaceAfter=4),
      "body":ParagraphStyle("body",fontName="Segoe",fontSize=10.2,leading=15,textColor=INK,spaceAfter=9),
      "h":ParagraphStyle("h",fontName="SegoeBold",fontSize=23,leading=28,textColor=INK,spaceAfter=15),
      "sub":ParagraphStyle("sub",fontName="SegoeBold",fontSize=12,leading=17,textColor=INK,spaceBefore=10,spaceAfter=7,keepWithNext=True),
      "caption":ParagraphStyle("caption",fontName="SegoeItalic",fontSize=8,leading=11,textColor=MUTED,spaceAfter=10),
      "cell":ParagraphStyle("cell",fontName="Segoe",fontSize=9,leading=12,textColor=INK),
      "th":ParagraphStyle("th",fontName="SegoeBold",fontSize=9,leading=12,textColor=colors.white)}
    def p(text,style="body"):return Paragraph(text,styles[style])
    def image(path,max_height=300,crop=None):
        path=base/path
        with PILImage.open(path) as im:
            if crop:
                im=im.crop(crop);buffer=io.BytesIO();im.save(buffer,format="PNG");buffer.seek(0);source=buffer
            else:source=str(path)
            iw,ih=im.size
        scale=min(CONTENT/iw,max_height/ih)
        return Image(source,width=iw*scale,height=ih*scale,hAlign="LEFT")
    def block(item):
        kind=item["type"]
        if kind in ("p","sub"):return [p(item["text"],"body" if kind=="p" else "sub")]
        if kind=="space":return [Spacer(1,item.get("height",8))]
        if kind=="steps":return [p(f'<b>{i}.</b> {text}') for i,text in enumerate(item["items"],1)]
        if kind=="image":return [KeepTogether([image(item["path"],item.get("max_height",300),item.get("crop")),Spacer(1,6),p(item.get("caption",""),"caption")])]
        if kind=="why":
            t=Table([[p("<b>Why this matters</b><br/>"+item["text"])]],colWidths=[CONTENT])
            t.setStyle(TableStyle([("BACKGROUND",(0,0),(-1,-1),LIGHT),("LINEBEFORE",(0,0),(0,-1),3,MINT),("LEFTPADDING",(0,0),(-1,-1),12),("RIGHTPADDING",(0,0),(-1,-1),12),("TOPPADDING",(0,0),(-1,-1),9),("BOTTOMPADDING",(0,0),(-1,-1),5)]))
            return [t,Spacer(1,10)]
        if kind=="table":
            rows=[[p(escape(str(v)),"th") for v in item["headers"]]]+[[p(escape(str(v)),"cell") for v in row] for row in item["rows"]]
            widths=item.get("widths",[1/len(item["headers"])]*len(item["headers"]))
            t=Table(rows,colWidths=[CONTENT*v for v in widths],repeatRows=1,hAlign="LEFT")
            t.setStyle(TableStyle([("BACKGROUND",(0,0),(-1,0),DARK),("ROWBACKGROUNDS",(0,1),(-1,-1),[LIGHT,colors.white]),("VALIGN",(0,0),(-1,-1),"TOP"),("LEFTPADDING",(0,0),(-1,-1),9),("RIGHTPADDING",(0,0),(-1,-1),9),("TOPPADDING",(0,0),(-1,-1),7),("BOTTOMPADDING",(0,0),(-1,-1),7),("LINEBELOW",(0,0),(-1,0),2,MINT)]))
            return [t,Spacer(1,10)]
        raise ValueError(f"Unknown block: {kind}")
    class Cover(Flowable):
        def __init__(self):super().__init__();self.width=CONTENT;self.height=H-130
        def draw(self):
            c=self.canv;c.saveState();c.setFillColor(DARK);c.rect(-52,-70,W+12,H+100,fill=1,stroke=0)
            if data.get("logo"):
                logo=base/data["logo"]
                with PILImage.open(logo) as im:lw,lh=im.size
                c.drawImage(str(logo),0,665,width=105,height=105*lh/lw,mask="auto")
            c.setFillColor(MINT);c.setFont("SegoeBold",44);c.drawString(0,594,data["product"].upper())
            c.setFillColor(colors.white);c.setFont("Segoe",25);c.drawString(0,556,"The complete user manual")
            c.setFillColor(MINT);c.setFont("Segoe",12);c.drawString(0,526,data["scope"])
            c.setFont("SegoeBold",25);c.drawString(0,470,data["platform"].upper())
            if data.get("cover_image"):
                im=image(data["cover_image"],330);im.drawOn(c,0,95)
            c.setFillColor(colors.white);c.setFont("Segoe",10)
            c.drawString(0,58,data.get("cover_caption",data["platform"]+"  |  Version "+data["version"]))
            c.setFillColor(MUTED);c.setFont("Segoe",9);c.drawString(0,30,data.get("publisher","DJL Software"));c.drawString(0,12,data["date"]);c.restoreState()
    def page(c,doc):
        if doc.page==1:return
        c.setStrokeColor(MINT);c.setLineWidth(1.5);c.line(46,H-39,W-46,H-39)
        c.setFont("Segoe",8);c.setFillColor(MUTED);c.drawString(46,H-29,"DJL  /  "+data["product"].upper()+"  /  USER MANUAL")
        c.drawString(46,26,data.get("footer","Version "+data["version"]+"  |  "+data["platform"]));c.drawRightString(W-46,26,str(doc.page))
    class Doc(SimpleDocTemplate):
        def afterFlowable(self,f):
            if hasattr(f,"chapter_id"):
                self.canv.bookmarkPage(f.chapter_id);self.canv.addOutlineEntry(f.chapter_title,f.chapter_id,0)
                self.found.append((f.chapter_id,f.chapter_title,self.page))
    def build(toc):
        d=Doc(str(output),pagesize=(W,H),leftMargin=46,rightMargin=46,topMargin=58,bottomMargin=48,title=data["product"]+" - Complete User Manual",author=data.get("publisher","DJL Software"),subject="Version "+data["version"])
        d.found=[];story=[Cover(),PageBreak(),p("Contents","h")]
        for i,ch in enumerate(data["chapters"],1):
            page_no=toc[i-1][2] if toc else ""
            story.append(p(f'<link href="#chapter-{i}" color="#17252d">{i:02d}  {escape(ch["title"])}</link> <font color="#58707c">  {page_no}</font>',"toc"))
        story.append(PageBreak())
        for i,ch in enumerate(data["chapters"],1):
            h=p(f'{i:02d}  {escape(ch["title"])}',"h");h.chapter_id=f"chapter-{i}";h.chapter_title=ch["title"];story.append(h)
            for item in ch["blocks"]:story.extend(block(item))
            if i<len(data["chapters"]):story.append(PageBreak())
        d.build(story,onFirstPage=page,onLaterPages=page)
        return d.found
    output.parent.mkdir(parents=True,exist_ok=True)
    toc=build(None)
    for _ in range(4):
        actual=build(toc)
        if actual==toc:break
        toc=actual
    else:raise RuntimeError("Contents pagination did not converge")
    reader=PdfReader(output)
    assert len(reader.outline)==len(data["chapters"])
    for n,pg in enumerate(reader.pages,1):assert pg.extract_text().strip(),f"Blank page {n}"
    output.with_suffix(".contents.json").write_text(json.dumps(toc,indent=2),encoding="utf-8")
    print(f"{output}: {len(reader.pages)} pages, {len(toc)} chapters")
if __name__=="__main__":
    ap=argparse.ArgumentParser();ap.add_argument("manuscript",type=Path);ap.add_argument("--output",required=True,type=Path);ap.add_argument("--font-dir",type=Path,default=Path("C:/Windows/Fonts"))
    args=ap.parse_args();render(args.manuscript.resolve(),args.output.resolve(),args.font_dir)
