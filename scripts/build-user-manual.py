"""Build Drum's complete guide using the reusable DJL/Meter layout skill."""
from pathlib import Path
import json,re,runpy
ROOT=Path(__file__).resolve().parents[1]
source=ROOT/"docs/manual/drum-manual.json"
data=json.loads(source.read_text(encoding="utf-8-sig"))
assert f'VERSION {data["version"]}' in (ROOT/"CMakeLists.txt").read_text()
for key in ("logo","cover_image"):
    data[key]=str((source.parent/data[key]).resolve())
for chapter in data["chapters"]:
    for block in chapter["blocks"]:
        if block["type"]=="image":block["path"]=str((source.parent/block["path"]).resolve())
registry=(ROOT/"src/SoundParameters.h").read_text()
pattern=r' X\((\w+),"([^"]+)",([^,]+),([^,]+),([^,]+),([^,]+),"([^"]*)",(\d+),"([^"]*)",([^)]+)\)'
pages={0:"Sample",1:"Envelopes and pitch",2:"Filter",3:"LFO",4:"Outputs and sends",6:"Attack layer"}
def number(v):return f"{v:g}"
for page,name in pages.items():
    rows=[]
    for key,label,initial,low,high,step,unit,p,choices,scale in re.findall(pattern,registry):
        if int(p)!=page:continue
        scale=float(scale)
        if choices:
            options=choices.split("|");values=", ".join(options);default=options[int(float(initial))]
        else:
            values=number(float(low)*scale)+" to "+number(float(high)*scale)+unit
            default=number(float(initial)*scale)+unit
        rows.append([label,values,default])
    data["chapters"].append({"title":"Control reference: "+name,"blocks":[
        {"type":"p","text":"Values are from the 0.12.2 control registry. Defaults apply to a new instrument or a page reset. A macro mapping can override the stored base value."},
        {"type":"table","headers":["Control","Range / choices","Default"],"rows":rows,"widths":[.31,.48,.21]}
    ]})
out=ROOT/"output/pdf/DJL-Drum-User-Manual-0.12.2-Windows.pdf"
temp=ROOT/"tmp/pdfs/drum-manual-expanded.json";temp.parent.mkdir(parents=True,exist_ok=True)
temp.write_text(json.dumps(data,indent=2),encoding="utf-8")
renderer=runpy.run_path(str(ROOT/"skills/djl-manual-layout/scripts/build_manual.py"))
renderer["render"](temp,out,Path("C:/Windows/Fonts"))
