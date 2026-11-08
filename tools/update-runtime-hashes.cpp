// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith
// Native C++17 generator/checker for Cinnamon runtime integrity manifests.
// Files are enumerated from source; new JavaScript modules cannot be omitted.
// sha256sum is the existing native verification primitive in the Makefile.
#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
namespace fs=std::filesystem;
static void require(bool ok,const std::string& message){
    if(!ok)throw std::runtime_error(message);
}
static std::string capture_sha256(const fs::path& source){
    int pipes[2]{};
    require(pipe(pipes)==0,"cannot allocate checksum pipe");
    const pid_t child=fork();
    if(child<0){close(pipes[0]);close(pipes[1]);throw std::runtime_error("cannot fork SHA-256 worker");}
    if(child==0){
        close(pipes[0]);
        if(dup2(pipes[1],STDOUT_FILENO)<0)_exit(127);
        close(pipes[1]);
        const std::string argument=source.string();
        execlp("sha256sum","sha256sum","--",argument.c_str(),static_cast<char*>(nullptr));
        _exit(127);
    }
    close(pipes[1]);
    std::string result;
    std::array<char,256> buf{};
    bool error=false;
    while(true){
        const ssize_t n=read(pipes[0],buf.data(),buf.size());
        if(n==0)break;
        if(n<0){if(errno==EINTR)continue;error=true;break;}
        if(result.size()+std::size_t(n)>8192){error=true;break;}
        result.append(buf.data(),std::size_t(n));
    }
    close(pipes[0]);
    int status=0;
    while(waitpid(child,&status,0)<0){
        if(errno==EINTR)continue;
        throw std::runtime_error("cannot reap SHA-256 worker");
    }
    require(!error&&WIFEXITED(status)&&WEXITSTATUS(status)==0,
            "cannot hash Cinnamon runtime source: "+source.string());
    require(result.size()>=66,"truncated SHA-256 output for "+source.string());
    const std::string digest=result.substr(0,64);
    for(unsigned char c:digest)
        require((c>='0'&&c<='9')||(c>='a'&&c<='f'),
                "unexpected SHA-256 output for "+source.string());
    require(result[64]==' ',"malformed SHA-256 output");
    return digest;
}
static std::string read_file(const fs::path& file){
    std::ifstream stream(file,std::ios::binary);
    if(!stream)return {};
    return {std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
}
static void write_manifest(const fs::path& path,const std::string& content){
    // Rename only a fully written file so a failed update cannot truncate
    // the manifest used by the packaged applet.
    const fs::path pending=path.string()+".tmp";
    {
        std::ofstream file(pending,std::ios::binary|std::ios::trunc);
        require(bool(file),"cannot create staged runtime manifest");
        file<<content;
        file.flush();
        require(bool(file),"failed writing staged runtime manifest");
    }
    std::error_code error;
    fs::rename(pending,path,error);
    if(error){
        fs::remove(pending);
        throw std::runtime_error("cannot publish runtime manifest: "+error.message());
    }
}
int main(int argc,char** argv){
    try{
        require(argc==1||(argc==2&&std::string(argv[1])=="--check"),
                "Usage: update-runtime-hashes [--check]");
        const bool check_only=argc==2;
        const fs::path root=".";
        const fs::path applet=root/"src/cinnamon";
        const fs::path manifest=applet/"runtime-sources.sha256";
        require(fs::is_directory(applet),"missing Cinnamon applet source directory");
        std::vector<fs::path> files;
        for(const auto& entry:fs::directory_iterator(applet))
            if(entry.is_regular_file()&&entry.path().extension()==".js")
                files.push_back(entry.path());
        std::sort(files.begin(),files.end(),[](const fs::path& a,const fs::path& b){
            return a.filename().string()<b.filename().string();
        });
        files.push_back(applet/"settings-schema.json");
        files.push_back(applet/"stylesheet.css");
        std::string expected;
        for(const auto& path:files){
            require(fs::is_regular_file(path),"missing Cinnamon runtime source: "+path.string());
            expected+=capture_sha256(path)+"  "+path.lexically_normal().generic_string()+"\n";
        }
        if(check_only){
            if(read_file(manifest)!=expected){
                std::cerr<<"src/cinnamon/runtime-sources.sha256 is stale; "
                         <<"run make update-runtime-hashes\n\nExpected manifest:\n"
                         <<expected;
                return 1;
            }
        }else write_manifest(manifest,expected);
        return 0;
    }catch(const std::exception& error){
        std::cerr<<"Calendar native runtime integrity tool: "<<error.what()<<'\n';
        return 1;
    }
}
