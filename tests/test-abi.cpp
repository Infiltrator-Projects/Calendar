// SPDX-License-Identifier: GPL-3.0-or-later
// Validate the package-internal Calendar GNU symbol-version ABI in C++17.
#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <stdexcept>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

using Symbols=std::map<std::string,std::string>;
static void require(bool value,const std::string &message)
{
    if (!value) throw std::runtime_error(message);
}
static std::string trim(std::string text)
{
    const size_t begin=text.find_first_not_of(" \r\n\t");
    if (begin==std::string::npos) return {};
    const size_t end=text.find_last_not_of(" \r\n\t");
    return text.substr(begin,end-begin+1);
}
static Symbols map_symbols(const std::string &path)
{
    std::ifstream input(path);
    require(bool(input),"missing ABI version script: "+path);
    const std::regex node_re(R"(^(CALENDAR_[0-9]+\.[0-9]+)\s*\{)");
    const std::regex symbol_re(R"(^(calendar_[A-Za-z0-9_]+);$)");
    std::string line,node;
    bool global=false;
    Symbols values;
    std::smatch match;
    while(std::getline(input,line)) {
        line=trim(line);
        if(std::regex_search(line,match,node_re)){
            node=match[1].str();
            global=false;
        } else if(line=="global:") {
            require(!node.empty(),"global symbols outside version node");
            global=true;
        } else if(line=="local:"){
            global=false;
        } else if(line.rfind("}",0)==0){
            node.clear();
            global=false;
        } else if(global && std::regex_match(line,match,symbol_re)){
            const auto name=match[1].str();
            require(values.emplace(name,node).second,"duplicate GNU ABI assignment: "+name);
        }
    }
    require(!values.empty(),"version map contains no global Calendar symbols");
    return values;
}
static Symbols baseline_symbols(const std::string &path)
{
    std::ifstream input(path);
    require(bool(input),"missing ABI baseline: "+path);
    const std::regex name_re(R"(^calendar_[A-Za-z0-9_]+$)");
    const std::regex node_re(R"(^CALENDAR_[0-9]+\.[0-9]+$)");
    Symbols values;
    std::string line;
    unsigned number=0;
    while(std::getline(input,line)) {
        ++number;
        line=trim(line);
        if(line.empty() || line[0]=='#') continue;
        const auto at=line.find('@');
        require(at!=std::string::npos && line.find('@',at+1)==std::string::npos,
                "invalid ABI baseline at line "+std::to_string(number));
        const std::string name=line.substr(0,at);
        const std::string node=line.substr(at+1);
        require(std::regex_match(name,name_re) && std::regex_match(node,node_re),
                "malformed ABI symbol/node on line "+std::to_string(number));
        require(values.emplace(name,node).second,
                "duplicate ABI baseline: "+name);
    }
    require(!values.empty(),"ABI baseline is empty");
    return values;
}
static std::string run_readelf(const char *path)
{
    int fd[2];
    require(::pipe(fd)==0,"pipe() failed");
    pid_t pid=::fork();
    require(pid>=0,"fork() failed");
    if(pid==0) {
        ::close(fd[0]);
        if(::dup2(fd[1],STDOUT_FILENO)<0) ::_exit(127);
        ::close(fd[1]);
        ::execlp("readelf","readelf","--dyn-syms","--wide",path,
                 static_cast<char *>(nullptr));
        ::_exit(127);
    }
    ::close(fd[1]);
    std::string output;
    char chunk[8192];
    for(;;) {
        ssize_t n=::read(fd[0],chunk,sizeof(chunk));
        if(n==0) break;
        if(n==-1 && errno==EINTR) continue;
        require(n>0,"reading readelf output failed");
        output.append(chunk,static_cast<size_t>(n));
    }
    ::close(fd[0]);
    int status=0;
    while(::waitpid(pid,&status,0)<0) {
        if(errno!=EINTR) throw std::runtime_error("waitpid() failed");
    }
    require(WIFEXITED(status) && WEXITSTATUS(status)==0,
            "readelf rejected the Calendar shared object");
    return output;
}
static Symbols dynamic_symbols(const char *path)
{
    const std::string output=run_readelf(path);
    const std::regex dynamic_re(
        R"(\b(calendar_[A-Za-z0-9_]+)@@(CALENDAR_[0-9]+\.[0-9]+)\b)");
    Symbols values;
    for(auto it=std::sregex_iterator(output.begin(),output.end(),dynamic_re);
             it!=std::sregex_iterator();++it) {
        require(values.emplace((*it)[1].str(),(*it)[2].str()).second,
                "duplicate versioned exported symbol: "+(*it)[1].str());
    }
    return values;
}
static void require_published(const Symbols &baseline,const Symbols &actual,
                              const std::string &source)
{
    for(const auto &symbol:baseline) {
        auto found=actual.find(symbol.first);
        require(found!=actual.end(),
                "published symbol absent from "+source+": "+symbol.first);
        require(found->second==symbol.second,
                "GNU ABI version moved in "+source+": "+symbol.first+" ("+
                symbol.second+" -> "+found->second+")");
    }
}
int main(int argc,char **argv)
{
    try {
        require(argc==1 || (argc==3 && std::strcmp(argv[1],"--library")==0),
                "usage: calendar-abi-test [--library shared-object]");
        const auto baseline=baseline_symbols("tests/abi-baseline.txt");
        require_published(baseline,map_symbols("src/abi/calendar.map"),
                          "linker version map");
        if(argc==3) {
            std::ifstream check(argv[2],std::ios::binary);
            require(bool(check),"Calendar shared object missing");
            require_published(baseline,dynamic_symbols(argv[2]),
                              "built shared object");
        }
        std::cout<<"Calendar GNU symbol ABI: "<<baseline.size()
                 <<" published symbols qualified\n";
    } catch(const std::exception &error) {
        std::cerr<<"Calendar ABI validation failed: "<<error.what()<<'\n';
        return 1;
    }
    return 0;
}
