// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith
// Canonical, native C++17 Cinnamon bootstrap generator and drift checker.
// The shim must run before runtimeSupport.js can be loaded. Do not copy
// its implementation into any independent Calendar JavaScript module.
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>
namespace fs=std::filesystem;
static const std::string CANONICAL=R"CAL(function _loadRuntimeSupport() {
    try {
        const Extension = imports.ui.extension;
        if (Extension && typeof Extension.getCurrentExtension === "function") {
            const extension = Extension.getCurrentExtension();
            if (extension && extension.imports && extension.imports.runtimeSupport) {
                return extension.imports.runtimeSupport;
            }
        }
    } catch (error) {
        /* Fall through when Cinnamon's current-extension lookup is unavailable. */
    }
    return require("./runtimeSupport");
})CAL";
static void require(bool good,const std::string& msg){
    if(!good)throw std::runtime_error(msg);
}
static std::string read(const fs::path& path){
    std::ifstream file(path,std::ios::binary);
    require(bool(file),"Cannot read "+path.string());
    return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
}
static void write(const fs::path& path,const std::string& value){
    // The source permissions must remain unchanged when a stale file is fixed.
    const fs::path pending=path.string()+".tmp";
    {
        std::ofstream file(pending,std::ios::binary|std::ios::trunc);
        require(bool(file),"Cannot stage corrected runtime bootstrap");
        file<<value;
        file.flush();
        require(bool(file),"Cannot write corrected runtime bootstrap");
    }
    std::error_code ec;
    fs::permissions(pending,fs::status(path).permissions(),ec);
    require(!ec,"Cannot preserve runtime module permissions: "+ec.message());
    fs::rename(pending,path,ec);
    if(ec){fs::remove(pending);throw std::runtime_error("Cannot publish bootstrap: "+ec.message());}
}
int main(int argc,char**argv){
    try{
        require(argc==1||(argc==2&&std::string(argv[1])=="--check"),
                "Usage: update-runtime-loader [--check]");
        const bool checking=argc==2;
        const fs::path applet="src/cinnamon";
        require(fs::is_directory(applet),"Cannot find Cinnamon application");
        const std::regex bootstrap(R"RE(function _loadRuntimeSupport\(\) \{[\s\S]*?\n\})RE");
        std::vector<fs::path> paths;
        for(const auto& entry:fs::directory_iterator(applet)){
            if(!entry.is_regular_file()||entry.path().extension()!=".js"||
               entry.path().filename()=="runtimeSupport.js")continue;
            const std::string source=read(entry.path());
            if(source.find("function _loadRuntimeSupport()")!=std::string::npos)
                paths.push_back(entry.path());
        }
        std::sort(paths.begin(),paths.end());
        require(!paths.empty(),"no Cinnamon runtime bootstrap users found");
        std::vector<std::string> stale;
        for(const auto& path:paths){
            const auto src=read(path);
            const std::sregex_iterator begin(src.begin(),src.end(),bootstrap),end;
            require(std::distance(begin,end)==1,
                    path.string()+" must contain exactly one runtime bootstrap");
            const auto match=*begin;
            if(match.str()==CANONICAL)continue;
            if(checking){stale.push_back(path.generic_string());continue;}
            write(path,src.substr(0,std::size_t(match.position()))+
                       CANONICAL+src.substr(std::size_t(match.position()+match.length())));
        }
        if(!stale.empty()){
            std::string list;
            for(const auto& path:stale){
                if(!list.empty())list+=", ";
                list+=path;
            }
            throw std::runtime_error(
                "Cinnamon runtime bootstrap drifted from its canonical generator: "+
                list+"; run make update-runtime-loader");
        }
        return 0;
    }catch(const std::exception&error){
        std::cerr<<"Calendar runtime bootstrap: "<<error.what()<<'\n';
        return 1;
    }
}
