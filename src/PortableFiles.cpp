#include "PortableFiles.h"
namespace drum {
namespace {
template<class Function> Result visit(juce::XmlElement& xml, Function&& fn) {
    if(xml.hasTagName("Instrument")){auto result=fn(xml);if(!result.ok)return result;}
    for(auto* child:xml.getChildIterator()){auto result=visit(*child,fn);if(!result.ok)return result;}
    return Result::success();
}
juce::String fingerprint(const juce::File& file){
    auto stream=file.createInputStream();if(!stream)return {};
    uint64_t hash=14695981039346656037ull;char bytes[65536];
    for(;;){int count=stream->read(bytes,sizeof(bytes));if(count<=0)break;for(int n=0;n<count;++n){hash^=(unsigned char)bytes[n];hash*=1099511628211ull;}}
    return juce::String::toHexString((juce::int64)hash)+"-"+juce::String(file.getSize());
}
}
juce::File PortableFiles::root(){
    auto test=juce::SystemStats::getEnvironmentVariable("DJL_DRUM_TEST_LIBRARY",{});
    if(test.isNotEmpty())return juce::File(test).getParentDirectory();
    auto config=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("DJL/Drum/data-root.txt").loadFileAsString().trim();
    return juce::File::isAbsolutePath(config)?juce::File(config):juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("DJL/Drum");
}
Result PortableFiles::collect(juce::XmlElement& xml,const juce::File& document){
    auto samples=document.getParentDirectory().getParentDirectory().getChildFile("Samples");
    return visit(xml,[&](juce::XmlElement& instrument){
        auto path=instrument.getStringAttribute("samplePath");if(path.isEmpty())return Result::success();
        if(!juce::File::isAbsolutePath(path))return Result::fail("Sample path is not absolute.");
        juce::File source(path);if(!source.existsAsFile())return Result::fail("Missing sample: "+source.getFileName()+". Relink it before saving a portable file.");
        if(!samples.createDirectory())return Result::fail("Cannot create Samples folder.");
        auto hash=fingerprint(source);if(hash.isEmpty())return Result::fail("Cannot read sample: "+source.getFileName());
        auto target=samples.getChildFile(hash).getChildFile(source.getFileName());
        if(!target.getParentDirectory().createDirectory())return Result::fail("Cannot create sample folder.");
        if(target.existsAsFile()){if(!source.hasIdenticalContentTo(target))return Result::fail("Sample fingerprint conflict; existing audio was not overwritten.");}
        else {juce::TemporaryFile temp(target);if(!source.copyFileTo(temp.getFile()) || !temp.overwriteTargetFileWithTemporary())return Result::fail("Cannot collect sample: "+source.getFileName());}
        instrument.setAttribute("samplePath",target.getRelativePathFrom(document.getParentDirectory()).replaceCharacter('\\','/'));
        return Result::success();
    });
}
Result PortableFiles::resolve(juce::XmlElement& xml,const juce::File& document){
    auto base=document.getParentDirectory().getParentDirectory();
    return visit(xml,[&](juce::XmlElement& instrument){
        auto path=instrument.getStringAttribute("samplePath");if(path.isEmpty() || juce::File::isAbsolutePath(path))return Result::success();
        auto sample=document.getParentDirectory().getChildFile(path);
        if(!sample.isAChildOf(base))return Result::fail("Sample path escapes the portable folder.");
        instrument.setAttribute("samplePath",sample.getFullPathName());return Result::success();
    });
}
Result PortableFiles::write(const juce::XmlElement& source,const juce::File& document){
    auto xml=std::make_unique<juce::XmlElement>(source);auto result=collect(*xml,document);if(!result.ok)return result;
    if(!document.getParentDirectory().createDirectory())return Result::fail("Cannot create destination folder.");
    juce::TemporaryFile temp(document);if(!temp.getFile().replaceWithText(xml->toString()) || !temp.overwriteTargetFileWithTemporary())return Result::fail("Cannot save file. Existing document was not replaced.");return Result::success();
}
}
