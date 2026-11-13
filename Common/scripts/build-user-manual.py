"""Build Drum's complete guide using the reusable DJL/Meter layout skill."""
import argparse
from pathlib import Path
import json,re,runpy
ROOT=Path(__file__).resolve().parents[1]
args=argparse.ArgumentParser()
args.add_argument("--font-dir",type=Path,default=Path("/Users/stevefarmer/.cache/codex-runtimes/codex-primary-runtime/dependencies/native/libreoffice-headless/libreoffice/LibreOfficeDev.app/Contents/Resources/fonts/truetype"))
args.add_argument("--manuscript",type=Path,default=ROOT/"docs/manual/drum-manual.json")
manual_args=args.parse_args()
source=manual_args.manuscript.resolve()
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
        if int(p)!=page or key=="keyOverride":continue
        scale=float(scale)
        if choices:
            options=choices.split("|");values=", ".join(options);default=options[int(float(initial))]
        else:
            values=number(float(low)*scale)+" to "+number(float(high)*scale)+unit
            default=number(float(initial)*scale)+unit
        if key.startswith("send"):
            values="Off (minus-infinity) to +6 dB";default="Off"
        rows.append([label,values,default])
    data["chapters"].append({"title":"Control reference: "+name,"blocks":[
        {"type":"p","text":f"Values are from the {data['version']} control registry. Defaults apply to a new instrument or page reset. Envelope timing values also support legacy/macro editing; the current editor uses point-and-curve graphs. A macro can override a main-sound base value. Send shaping uses independent settings."},
        {"type":"table","headers":["Control","Range / choices","Default"],"rows":rows,"widths":[.31,.48,.21]}
    ]})
out=ROOT.parent/f"Mac/output/pdf/DJL-Drum-User-Manual-{data['version']}-macOS.pdf"
temp=ROOT/"tmp/pdfs/drum-manual-expanded.json";temp.parent.mkdir(parents=True,exist_ok=True)
temp.write_text(json.dumps(data,indent=2),encoding="utf-8")
renderer=runpy.run_path(str(ROOT/"skills/djl-manual-layout/scripts/build_manual.py"))
renderer["render"](temp,out,manual_args.font_dir)
