#include "speech/speech.hpp"
#include <algorithm>
#include <cctype>
namespace ortho {
namespace {
struct Token { std::size_t begin,end; std::string folded; };
unsigned decode(const std::string& text,std::size_t& i) {
    unsigned c=static_cast<unsigned char>(text[i++]);
    if(c<128)return c;
    const int extra=(c&0xe0)==0xc0?1:(c&0xf0)==0xe0?2:3;
    c &= extra==1?31:extra==2?15:7;
    for(int j=0;j<extra && i<text.size();++j)c=(c<<6)|(static_cast<unsigned char>(text[i++])&63);
    return c;
}
bool letter(unsigned c) {
    return (c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||
        (c>=0xc0&&c<=0x2af&&c!=0xd7&&c!=0xf7)||(c>=0x300&&c<=0x52f)||
        (c>=0x1e00&&c<=0x1fff);
}
std::string folded(const std::string& text) {
    std::string result;
    for(std::size_t i=0;i<text.size();) {
        unsigned c=decode(text,i);
        if(c>='A'&&c<='Z')c+=32;
        if((c>=0xc0&&c<=0xd6)||(c>=0xd8&&c<=0xde))c+=32;
        if(c<128)result+=char(c);
        else if(c<2048){result+=char(0xc0|(c>>6));result+=char(0x80|(c&63));}
        else{result+=char(0xe0|(c>>12));result+=char(0x80|((c>>6)&63));result+=char(0x80|(c&63));}
    }
    return result;
}
std::vector<Token> tokens(const std::string& text) {
    std::vector<Token> result;
    for(std::size_t i=0;i<text.size();) {
        const auto begin=i;
        if(!letter(decode(text,i)))continue;
        while(i<text.size()){auto next=i;if(!letter(decode(text,next)))break;i=next;}
        result.push_back({begin,i,folded(text.substr(begin,i-begin))});
    }
    return result;
}
bool only_space(const std::string& text,std::size_t first,std::size_t last) {
    return std::all_of(text.begin()+first,text.begin()+last,[](unsigned char c){return std::isspace(c);});
}
}
SpeechUtterance make_utterance(const std::string& text,const std::string& language,const std::vector<Pronunciation>& lexicon) {
    auto ordered=lexicon;
    std::stable_sort(ordered.begin(),ordered.end(),[](auto& a,auto& b){return a.priority!=b.priority?a.priority>b.priority:a.source.size()>b.source.size();});
    const auto words=tokens(text);
    std::string result; std::size_t copied=0;
    for(std::size_t i=0;i<words.size();) {
        std::size_t count=0; std::string replacement;
        for(const auto& entry:ordered) {
            if(entry.language!=language||entry.spoken.empty())continue;
            const auto phrase=tokens(entry.source);
            if(phrase.empty()||i+phrase.size()>words.size())continue;
            bool match=true;
            for(std::size_t j=0;j<phrase.size();++j) {
                if(phrase[j].folded!=words[i+j].folded || (j && !only_space(text,words[i+j-1].end,words[i+j].begin))) {match=false;break;}
            }
            if(match){count=phrase.size();replacement=entry.spoken;break;}
        }
        if(!count){++i;continue;}
        result+=text.substr(copied,words[i].begin-copied);result+=replacement;
        copied=words[i+count-1].end;i+=count;
    }
    result+=text.substr(copied);
    return {text,result,language};
}
std::string reading_introduction(const Reading& reading) {
    const auto& p=reading.passage;
    const auto reference=reading.label.find(" "+std::to_string(p.first.chapter)+":");
    const std::string book=p.book=="Ps"?"Psaltaren":reference==std::string::npos?reading.label:reading.label.substr(0,reference);
    return "Läsning ur "+book+", kapitel "+std::to_string(p.first.chapter)+", vers "+std::to_string(p.first.verse)+
        (p.last==p.first?".":" till "+(p.last.chapter!=p.first.chapter?"kapitel "+std::to_string(p.last.chapter)+", vers ":"")+std::to_string(p.last.verse)+".");
}
}
