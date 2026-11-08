// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith
// Native C++17 deterministic Cinnamon settings JSON generator and ownership guard.
// Parses the actual schema; do not use textual substring checks as a substitute
// for validating that prohibited temporal settings are absent from the root.
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <stdexcept>
#include <string>
namespace fs=std::filesystem;
static void require(bool yes,const std::string& reason){
    if(!yes)throw std::runtime_error(reason);
}
static void append_utf8(std::string& result,std::uint32_t point){
    require(point<=0x10ffff&&(point<0xd800||point>0xdfff),"Invalid JSON Unicode code point");
    if(point<=0x7f)result.push_back(char(point));
    else if(point<=0x7ff){
        result.push_back(char(0xc0|(point>>6)));
        result.push_back(char(0x80|(point&63)));
    }else if(point<=0xffff){
        result.push_back(char(0xe0|(point>>12)));
        result.push_back(char(0x80|((point>>6)&63)));
        result.push_back(char(0x80|(point&63)));
    }else{
        result.push_back(char(0xf0|(point>>18)));
        result.push_back(char(0x80|((point>>12)&63)));
        result.push_back(char(0x80|((point>>6)&63)));
        result.push_back(char(0x80|(point&63)));
    }
}
static std::string quote(const std::string& text){
    std::string result="\"";
    static const char* digits="0123456789abcdef";
    for(unsigned char c:text){
        if(c=='"')result+="\\\"";
        else if(c=='\\')result+="\\\\";
        else if(c=='\b')result+="\\b";
        else if(c=='\f')result+="\\f";
        else if(c=='\n')result+="\\n";
        else if(c=='\r')result+="\\r";
        else if(c=='\t')result+="\\t";
        else if(c<0x20){
            result+="\\u00";
            result+=digits[c>>4];
            result+=digits[c&15];
        }else result.push_back(char(c));
    }
    return result+"\"";
}
class Parser{
    const std::string& input;
    std::size_t pos=0;
    static std::string indent(int n){return std::string(std::size_t(n)*4,' ');}
    void ws(){
        while(pos<input.size()&&(input[pos]==' '||input[pos]=='\n'||
                                 input[pos]=='\r'||input[pos]=='\t'))++pos;
    }
    bool accept(char c){
        ws();
        if(pos<input.size()&&input[pos]==c){++pos;return true;}
        return false;
    }
    std::uint32_t hex4(){
        require(input.size()-pos>=4,"Truncated Unicode escape in JSON string");
        std::uint32_t code=0;
        for(int i=0;i<4;i++){
            const unsigned char ch=input[pos++];
            unsigned value;
            if(ch>='0'&&ch<='9')value=ch-'0';
            else if(ch>='a'&&ch<='f')value=ch-'a'+10;
            else if(ch>='A'&&ch<='F')value=ch-'A'+10;
            else throw std::runtime_error("Invalid Unicode escape in JSON string");
            code=(code<<4)|value;
        }
        return code;
    }
    std::string string(){
        ws();
        require(pos<input.size()&&input[pos++]=='"',"Expected JSON string");
        std::string result;
        while(pos<input.size()){
            const unsigned char c=input[pos++];
            if(c=='"')return result;
            require(c>=0x20,"Unescaped control character in JSON string");
            if(c!='\\'){result.push_back(char(c));continue;}
            require(pos<input.size(),"Truncated JSON string escape");
            const char next=input[pos++];
            switch(next){
                case '"':result.push_back('"');break;
                case '\\':result.push_back('\\');break;
                case '/':result.push_back('/');break;
                case 'b':result.push_back('\b');break;
                case 'f':result.push_back('\f');break;
                case 'n':result.push_back('\n');break;
                case 'r':result.push_back('\r');break;
                case 't':result.push_back('\t');break;
                case 'u':{
                    std::uint32_t code=hex4();
                    if(code>=0xd800&&code<=0xdbff){
                        require(input.size()-pos>=6&&input[pos]=='\\'&&input[pos+1]=='u',
                                "Missing UTF-16 low surrogate");
                        pos+=2;
                        const auto low=hex4();
                        require(low>=0xdc00&&low<=0xdfff,"Invalid UTF-16 low surrogate");
                        code=0x10000+((code-0xd800)<<10)+(low-0xdc00);
                    }
                    append_utf8(result,code);
                    break;
                }
                default:throw std::runtime_error("Invalid JSON escape");
            }
        }
        throw std::runtime_error("Unterminated JSON string");
    }
    std::string number(){
        ws();
        const auto begin=pos;
        if(pos<input.size()&&input[pos]=='-')++pos;
        require(pos<input.size(),"Truncated JSON number");
        if(input[pos]=='0')++pos;
        else{
            require(input[pos]>='1'&&input[pos]<='9',"Invalid JSON number");
            while(pos<input.size()&&std::isdigit(static_cast<unsigned char>(input[pos])))++pos;
        }
        if(pos<input.size()&&input[pos]=='.'){
            ++pos;
            require(pos<input.size()&&std::isdigit(static_cast<unsigned char>(input[pos])),
                    "Invalid JSON fractional part");
            while(pos<input.size()&&std::isdigit(static_cast<unsigned char>(input[pos])))++pos;
        }
        if(pos<input.size()&&(input[pos]=='e'||input[pos]=='E')){
            ++pos;
            if(pos<input.size()&&(input[pos]=='+'||input[pos]=='-'))++pos;
            require(pos<input.size()&&std::isdigit(static_cast<unsigned char>(input[pos])),
                    "Invalid JSON exponent");
            while(pos<input.size()&&std::isdigit(static_cast<unsigned char>(input[pos])))++pos;
        }
        return input.substr(begin,pos-begin);
    }
    std::string value(int depth,bool root=false){
        require(depth<=64,"Settings JSON exceeds maximum nesting");
        ws();
        require(pos<input.size(),"Unexpected end of JSON");
        if(input[pos]=='{'){
            ++pos;
            std::string out="{";
            if(accept('}'))return out+"}";
            std::set<std::string> keys;
            const std::set<std::string> forbidden={
                "follow-system-temporal","clock-mode","show-seconds",
                "location-configured","latitude","longitude","primary-calendar",
                "secondary-calendar","use-custom-format","custom-format",
                "custom-tooltip-format","format-button"
            };
            do{
                const auto key=string();
                require(keys.insert(key).second,"Duplicate settings key: "+key);
                if(root)require(forbidden.count(key)==0,
                    "Calendar must not own temporal presentation setting: "+key+
                    "; move this to System Settings");
                require(accept(':'),"Expected colon after settings key");
                out+=(out=="{"?"\n":",\n")+indent(depth+1)+quote(key)+": "+
                     value(depth+1);
                if(accept('}'))break;
                require(accept(','),"Expected comma between settings entries");
                require(!accept('}'),"Trailing comma in settings object");
            }while(true);
            return out+"\n"+indent(depth)+"}";
        }
        if(input[pos]=='['){
            ++pos;
            std::string out="[";
            if(accept(']'))return out+"]";
            do{
                out+=(out=="["?"\n":",\n")+indent(depth+1)+value(depth+1);
                if(accept(']'))break;
                require(accept(','),"Expected comma in settings array");
                require(!accept(']'),"Trailing comma in settings array");
            }while(true);
            return out+"\n"+indent(depth)+"]";
        }
        if(input[pos]=='"')return quote(string());
        for(const char* keyword:{"true","false","null"}){
            const std::string word=keyword;
            if(input.compare(pos,word.size(),word)==0){pos+=word.size();return word;}
        }
        return number();
    }
public:
    explicit Parser(const std::string& text):input(text){}
    std::string render(){
        ws();
        require(pos<input.size()&&input[pos]=='{',"Cinnamon settings must be a JSON object");
        auto out=value(0,true)+"\n";
        ws();
        require(pos==input.size(),"Trailing characters after settings JSON");
        return out;
    }
};
static std::string read(const fs::path& path){
    std::ifstream file(path,std::ios::binary);
    require(bool(file),"Cannot read "+path.string());
    return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
}
static void update(const fs::path& target,const std::string& rendered){
    const fs::path stage=target.string()+".tmp";
    {
        std::ofstream file(stage,std::ios::binary|std::ios::trunc);
        require(bool(file),"Cannot stage Calendar settings schema");
        file<<rendered;
        file.flush();
        require(bool(file),"Cannot write Calendar settings schema");
    }
    std::error_code ec;
    fs::permissions(stage,fs::status(target).permissions(),ec);
    require(!ec,"Cannot preserve Calendar settings permissions");
    fs::rename(stage,target,ec);
    if(ec){fs::remove(stage);throw std::runtime_error("Cannot replace settings schema: "+ec.message());}
}
int main(int argc,char**argv){
    try{
        require(argc==1||(argc==2&&std::string(argv[1])=="--check"),
                "Usage: update-settings [--check]");
        const fs::path path="src/cinnamon/settings-schema.json";
        const std::string original=read(path);
        const auto expected=Parser(original).render();
        if(argc==2){
            require(original==expected,
                    "settings-schema.json is not deterministically formatted; run make update-settings");
        }else if(original!=expected)update(path,expected);
        return 0;
    }catch(const std::exception& error){
        std::cerr<<"Calendar settings schema: "<<error.what()<<'\n';
        return 1;
    }
}
