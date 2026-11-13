#pragma once
#include "KitLibrary.h"
#include <set>

namespace drum {
// Live rack presets are gzip-compressed XML. Empty MIDI chains label single-key
// zones without introducing another instrument or changing the kit's note numbers.
class LiveNoteMap {
    using Xml = juce::XmlElement;
    static Xml* value(Xml& parent, const juce::String& tag, const juce::String& v) {
        auto* child = parent.createNewChildElement(tag); child->setAttribute("Value", v); return child;
    }
    static void target(Xml& p, const char* name) {
        auto* t=p.createNewChildElement(name);t->setAttribute("Id",0);value(*t,"LockEnvelope","0");
    }
    static void parameter(Xml& p,const juce::String& name,bool boolean=false) {
        auto* c=p.createNewChildElement(name);value(*c,"LomId","0");value(*c,"Manual",boolean?"true":"0");
        auto* range=c->createNewChildElement(boolean?"MidiCCOnOffThresholds":"MidiControllerRange");
        value(*range,"Min",boolean?"64":"0");value(*range,"Max","127");target(*c,"AutomationTarget");
        if(!boolean)target(*c,"ModulationTarget");
    }
    static void presetRef(Xml& p,const char* device) {
        auto* ref=p.createNewChildElement("AbletonDefaultPresetRef");ref->setAttribute("Id",0);
        auto* f=ref->createNewChildElement("FileRef");
        for(auto tag:{"RelativePathType","OriginalFileSize","OriginalCrc"})value(*f,tag,"0");
        value(*f,"Type","2");for(auto tag:{"RelativePath","Path","LivePackName","LivePackId","SourceHint"})value(*f,tag,"");
        ref->createNewChildElement("DeviceId")->setAttribute("Name",device);
    }
    static Xml* device(Xml& parent,const char* type,const juce::String& name) {
        auto* d=parent.createNewChildElement(type);d->setAttribute("Id",0);
        for(auto tag:{"LomId","LomIdView","ModulationSourceCount","LastSelectedTimeableIndex","LastSelectedClipEnvelopeIndex"})value(*d,tag,"0");
        for(auto tag:{"IsExpanded","ShouldShowPresetName","MpePitchBendUsesTuning"})value(*d,tag,"true");
        for(auto tag:{"BreakoutIsExpanded","IsFolded"})value(*d,tag,"false");
        parameter(*d,"On",true);d->createNewChildElement("ParametersListWrapper")->setAttribute("LomId",0);
        d->createNewChildElement("Pointee")->setAttribute("Id",0);
        presetRef(*d->createNewChildElement("LastPresetRef")->createNewChildElement("Value"),type);
        d->createNewChildElement("LockedScripts");value(*d,"UserName",name);value(*d,"Annotation","");
        d->createNewChildElement("SourceContext")->createNewChildElement("Value");
        value(*d,"ViewData","{}");value(*d,"OverwriteProtectionNumber","3077");return d;
    }
    static void range(Xml& parent,const char* tag,int low,int high) {
        auto* r=parent.createNewChildElement(tag);
        value(*r,"Min",juce::String(low));value(*r,"Max",juce::String(high));
        value(*r,"CrossfadeMin",juce::String(low));value(*r,"CrossfadeMax",juce::String(high));
    }
public:
    static std::unique_ptr<Xml> create(const Kit& kit) {
        auto root=std::make_unique<Xml>("Ableton");root->setAttribute("MajorVersion","5");
        root->setAttribute("MinorVersion","12.0_12402");root->setAttribute("SchemaChangeCount","1");
        root->setAttribute("Creator","Ableton Live 12.4d1");
        auto* p=root->createNewChildElement("GroupDevicePreset");value(*p,"OverwriteProtectionNumber","3077");
        auto* d=device(*p->createNewChildElement("Device"),"MidiEffectGroupDevice",kit.name+" - Drum note names");
        if(auto* folded=d->getChildByName("IsFolded"))folded->setAttribute("Value","true");
        for(auto tag:{"Branches","ReturnBranches"})d->createNewChildElement(tag);
        for(auto tag:{"IsBranchesListVisible","ShowsZonesInsteadOfNoteNames"})value(*d,tag,"true");
        for(auto tag:{"IsReturnBranchesListVisible","IsRangesEditorVisible","AreDevicesVisible","AreMacroControlsVisible","AreMacroVariationsControlsVisible","IsAutoSelectEnabled","ShowBranchesInSessionMixer","ChainSelectorFilterMidiCtrl"})value(*d,tag,"false");
        value(*d,"NumVisibleMacroControls","8");value(*d,"ChainSelectorRelativePosition","0");
        value(*d,"ViewsToRestoreWhenUnfolding","1");value(*d,"BranchesSplitterProportion","0.5");value(*d,"RangeTypeIndex","0");
        for(auto tag:{"ChainsListWrapper","ReturnChainsListWrapper"})d->createNewChildElement(tag)->setAttribute("LomId",0);
        for(int n=0;n<16;++n){auto suffix="."+juce::String(n);parameter(*d,"MacroControls"+suffix);
            value(*d,"MacroDisplayNames"+suffix,"");value(*d,"MacroDefaults"+suffix,"0");value(*d,"MacroAnnotations"+suffix,"");
            value(*d,"MacroColor"+suffix,"-1");for(auto tag:{"ForceDisplayGenericValue","ExcludeMacroFromRandomization","ExcludeMacroFromSnapshots"})value(*d,juce::String(tag)+suffix,"false");}
        parameter(*d,"ChainSelector");presetRef(*p->createNewChildElement("PresetRef"),"MidiEffectGroupDevice");
        auto* branches=p->createNewChildElement("BranchPresets");int index=0;
        for(const auto& group:kit.groups)for(const auto& instrument:group.instruments){
            auto* b=branches->createNewChildElement("MidiEffectBranchPreset");b->setAttribute("Id",index++);
            value(*b,"Name",instrument.name);value(*b,"IsSoloed","false");b->createNewChildElement("DevicePresets");
            auto* mix=b->createNewChildElement("MixerPreset")->createNewChildElement("AbletonDevicePreset");mix->setAttribute("Id",0);
            value(*mix,"OverwriteProtectionNumber","3077");parameter(*device(*mix->createNewChildElement("Device"),"MidiBranchMixerDevice",""),"Speaker",true);
            presetRef(*mix->createNewChildElement("PresetRef"),"MidiBranchMixerDevice");range(*b,"BranchSelectorRange",0,127);
            value(*b,"SessionViewBranchWidth","80");value(*b,"DocumentColorIndex","15");value(*b,"AutoColored","true");value(*b,"AutoColorScheme","0");
            b->createNewChildElement("SourceContext");auto* zones=b->createNewChildElement("ZoneSettings");
            range(*zones,"KeyRange",instrument.note,instrument.note);range(*zones,"VelocityRange",1,127);
        }
        p->createNewChildElement("ReturnBranchPresets");return root;
    }
    static Kit linkedKit(const Kit& source){
        Kit result;result.name="DJL Drum Link v1";Group group;
        for(int note=21;note<=108;++note){Instrument entry;entry.note=note;entry.name=noteName(note);
            for(const auto& g:source.groups)for(const auto& i:g.instruments)if(i.note==note)entry=i;
            group.instruments.push_back(entry);}
        result.groups.push_back(group);return result;
    }
    static Result writeLinked(const Kit& source,const juce::File& file){return write(linkedKit(source),file);}
    static Result write(const Kit& kit,const juce::File& file) {
        std::set<int> notes;
        for(const auto& g:kit.groups)for(const auto& i:g.instruments){
            if(i.note<0 || i.note>127 || !notes.insert(i.note).second)return Result::fail("Kit contains duplicate or invalid MIDI assignments.");
        }
        if(notes.empty())return Result::fail("Add instruments to the kit before exporting Live note names.");
        juce::MemoryOutputStream data;
        {juce::GZIPCompressorOutputStream compressed(data,9,31);auto text=create(kit)->toString();
            if(!compressed.write(text.toRawUTF8(),text.getNumBytesAsUTF8()))return Result::fail("Could not compress Live note map.");}
        if(file.getParentDirectory().createDirectory().failed() || !file.replaceWithData(data.getData(),data.getDataSize()))return Result::fail("Could not save Live note map.");
        return Result::success();
    }
};
}
