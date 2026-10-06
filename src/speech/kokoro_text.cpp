#include "speech/kokoro_text.hpp"
#include <onnxruntime_cxx_api.h>
#include <algorithm>
#include <array>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <utility>
namespace ortho {
namespace {
std::u32string decode(const std::string& text) {
    std::u32string result;
    for(size_t i=0;i<text.size();) {
        char32_t c=static_cast<unsigned char>(text[i++]);
        if(c>=0x80) {
            const int extra=(c&0xe0)==0xc0?1:(c&0xf0)==0xe0?2:3;
            c&=extra==1?0x1f:extra==2?0x0f:0x07;
            for(int j=0;j<extra&&i<text.size();++j)c=(c<<6)|(static_cast<unsigned char>(text[i++])&0x3f);
        }
        result+=c;
    }
    return result;
}
std::string encode(std::u32string_view text) {
    std::string result;
    for(char32_t c:text) {
        if(c<0x80)result+=char(c);
        else if(c<0x800){result+=char(0xc0|(c>>6));result+=char(0x80|(c&0x3f));}
        else if(c<0x10000){result+=char(0xe0|(c>>12));result+=char(0x80|((c>>6)&0x3f));result+=char(0x80|(c&0x3f));}
        else{result+=char(0xf0|(c>>18));result+=char(0x80|((c>>12)&0x3f));result+=char(0x80|((c>>6)&0x3f));result+=char(0x80|(c&0x3f));}
    }
    return result;
}
std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path,std::ios::binary);
    const auto name=path.filename().u8string();
    if(!input)throw std::runtime_error("Kokoro pronunciation file is missing: "+std::string(reinterpret_cast<const char*>(name.data()),name.size()));
    std::ostringstream buffer;buffer<<input.rdbuf();
    return buffer.str();
}
void replace_all(std::string& text,const std::string& from,const std::string& to) {
    for(size_t at=text.find(from);at!=std::string::npos;at=text.find(from,at+to.size()))text.replace(at,from.size(),to);
}

// --- Numbers: num2words 0.5.14, lang="sv" ----------------------------------
// The upstream spells digits with num2words before phonemization. This is its
// Swedish card table and merge rule, including its spellings ("etttusen").
using Words=std::pair<std::string,uint64_t>;
const std::vector<Words>& cards() {
    static const std::vector<Words> table=[] {
        std::vector<Words> result={{"triljoner",1000000000000000000ull},{"biljarder",1000000000000000ull},{"biljoner",1000000000000ull},
            {"miljarder",1000000000},{"miljoner",1000000},{"tusen",1000},{"hundra",100},{"nittio",90},{"åttio",80},{"sjuttio",70},
            {"sextio",60},{"femtio",50},{"förtio",40},{"trettio",30}};
        const char* low[]={"tjugo","nitton","arton","sjutton","sexton","femton","fjorton","tretton","tolv","elva","tio","nio","åtta",
            "sju","sex","fem","fyra","tre","två","ett","noll"};
        for(uint64_t i=0;i<21;++i)result.push_back({low[i],20-i});
        return result;
    }();
    return table;
}
Words merge(const Words& left,const Words& right) {
    const auto& [ltext,lnum]=left;const auto& [rtext,rnum]=right;
    if(lnum==1&&rnum<100)return right;
    if(100>lnum&&lnum>rnum)return {ltext+rtext,lnum+rnum};
    if(lnum>=100&&100>rnum)return {ltext+rtext,lnum+rnum};
    if(rnum>=1000000&&lnum==1)return {"en "+rtext.substr(0,rtext.size()-2),lnum+rnum};
    if(rnum>=1000000&&lnum>1)return {ltext+" "+rtext,lnum+rnum};
    if(rnum>lnum)return {ltext+rtext,lnum*rnum};
    return {ltext+" "+rtext,lnum+rnum};
}
// Num2Word_Base.splitnum followed by clean(), which merges left to right.
Words cardinal(uint64_t value) {
    for(const auto& [word,card]:cards()) {
        if(card>value)continue;
        const uint64_t div=value==0?1:value/card,mod=value==0?0:value%card;
        auto result=merge(div==1?Words{"ett",1}:cardinal(div),{word,card});
        return mod?merge(result,cardinal(mod)):result;
    }
    throw std::logic_error("Number card table is incomplete.");
}
std::string cardinal(std::string_view digits) {
    while(digits.size()>1&&digits.front()=='0')digits.remove_prefix(1);
    // Beyond 10^19 the reader spells single digits instead of num2words' long scale.
    if(digits.size()>19) {
        std::string result;
        for(char digit:digits)result+=(result.empty()?"":" ")+cardinal(uint64_t(digit-'0')).first;
        return result;
    }
    uint64_t value=0;for(char digit:digits)value=value*10+uint64_t(digit-'0');
    return cardinal(value).first;
}
bool sv_letter(char32_t c) {
    return (c>='A'&&c<='Z')||(c>='a'&&c<='z')||c==U'Å'||c==U'Ä'||c==U'Ö'||c==U'å'||c==U'ä'||c==U'ö';
}
bool digit(char32_t c) {return c>='0'&&c<='9';}

// --- Lexicon -----------------------------------------------------------------
// lexicon.tsv: "word\tp h o n e s", byte-sorted with unique words. Binary
// search over line offsets keeps the 37 MB file without a per-entry heap node.
class Lexicon {
    std::string data_;
    std::vector<uint32_t> lines_;
    std::unordered_map<std::string,std::string> custom_;
    std::string_view word(uint32_t line) const {
        const auto end=data_.find('\t',line);
        return std::string_view(data_).substr(line,end-line);
    }
    static std::string joined(std::string_view phones) {
        std::string result;
        for(char c:phones)if(c!=' ')result+=c;
        return result;
    }
public:
    Lexicon(const std::filesystem::path& lexicon,const std::filesystem::path& custom):data_(read_file(lexicon)) {
        for(size_t begin=0;begin<data_.size();) {
            auto end=data_.find('\n',begin);if(end==std::string::npos)end=data_.size();
            const auto tab=data_.find('\t',begin);
            if(tab>=end||data_.find('\t',tab+1)<end)throw std::runtime_error("Invalid Kokoro lexicon row.");
            lines_.push_back(static_cast<uint32_t>(begin));begin=end+1;
        }
        for(size_t i=1;i<lines_.size();++i)
            if(!(word(lines_[i-1])<word(lines_[i])))throw std::runtime_error("Kokoro lexicon is not sorted.");
        // Custom overrides win over the NST lexicon. Their words are lowercased.
        std::istringstream input(read_file(custom));
        for(std::string line;std::getline(input,line);) {
            if(line.empty()||line[0]=='#')continue;
            const auto tab=line.find('\t');
            if(tab==std::string::npos||line.find('\t',tab+1)!=std::string::npos)throw std::runtime_error("Invalid Kokoro custom lexicon row.");
            auto text=decode(line.substr(0,tab));
            for(auto& c:text)if((c>='A'&&c<='Z')||(c>=0xc0&&c<=0xde&&c!=0xd7))c+=32;
            custom_[encode(text)]=joined(std::string_view(line).substr(tab+1));
        }
    }
    // Empty when the word is unknown or its entry is empty, as `lex.get(w) or ...`.
    std::string find(const std::string& key) const {
        if(const auto entry=custom_.find(key);entry!=custom_.end())return entry->second;
        const auto at=std::lower_bound(lines_.begin(),lines_.end(),key,[this](uint32_t line,const std::string& value){return word(line)<value;});
        if(at==lines_.end()||word(*at)!=key)return {};
        const auto begin=*at+key.size()+1;
        auto end=data_.find('\n',begin);if(end==std::string::npos)end=data_.size();
        return joined(std::string_view(data_).substr(begin,end-begin));
    }
};

// Flat JSON objects of string keys and integer values, such as {"a": 3}.
std::map<std::u32string,int64_t> json_ids(const std::string& json,const std::string& name) {
    const auto key=json.find("\""+name+"\"");
    if(key==std::string::npos)throw std::runtime_error("Kokoro vocabulary is missing: "+name);
    size_t i=json.find('{',key);
    if(i==std::string::npos)throw std::runtime_error("Invalid Kokoro vocabulary: "+name);
    const auto space=[&]{while(i<json.size()&&(json[i]==' '||json[i]=='\n'||json[i]=='\r'||json[i]=='\t'))++i;};
    const auto hex=[&](size_t at) {
        if(at+4>json.size())throw std::runtime_error("Invalid Kokoro vocabulary escape.");
        return static_cast<char32_t>(std::stoul(json.substr(at,4),nullptr,16));
    };
    std::map<std::u32string,int64_t> result;
    ++i;
    while(true) {
        space();
        if(i<json.size()&&json[i]=='}')return result;
        if(i>=json.size()||json[i]!='"')throw std::runtime_error("Invalid Kokoro vocabulary: "+name);
        std::u32string text;std::string raw;
        for(++i;i<json.size()&&json[i]!='"';++i) {
            if(json[i]!='\\'){raw+=json[i];continue;}
            text+=decode(raw);raw.clear();
            const char e=json[++i];
            if(e=='u') {
                char32_t c=hex(i+1);i+=4;
                if(c>=0xd800&&c<0xdc00&&json.compare(i+1,2,"\\u")==0){c=0x10000+((c-0xd800)<<10)+(hex(i+3)-0xdc00);i+=6;}
                text+=c;
            } else text+=e=='n'?U'\n':e=='t'?U'\t':e=='r'?U'\r':e=='b'?U'\b':e=='f'?U'\f':char32_t(e);
        }
        text+=decode(raw);++i;space();
        if(i>=json.size()||json[i]!=':')throw std::runtime_error("Invalid Kokoro vocabulary: "+name);
        ++i;space();
        size_t used=0;result[text]=std::stoll(json.substr(i,24),&used);i+=used;space();
        if(i<json.size()&&json[i]==',')++i;
    }
}
}

std::string swedish_numbers(const std::string& text) {
    const auto source=decode(text);
    std::u32string spaced;
    for(size_t i=0;i<source.size();++i) {
        if(i&&((digit(source[i])&&sv_letter(source[i-1]))||(sv_letter(source[i])&&digit(source[i-1]))))spaced+=U' ';
        spaced+=source[i];
    }
    std::string result;
    for(size_t i=0;i<spaced.size();) {
        if(!digit(spaced[i])){result+=encode(spaced.substr(i,1));++i;continue;}
        auto end=i;while(end<spaced.size()&&digit(spaced[end]))++end;
        const auto whole=encode(spaced.substr(i,end-i));
        if(end+1<spaced.size()&&(spaced[end]==','||spaced[end]=='.')&&digit(spaced[end+1])) {
            auto last=end+1;while(last<spaced.size()&&digit(spaced[last]))++last;
            result+=cardinal(whole)+" komma";
            for(auto at=end+1;at<last;++at)result+=" "+cardinal(uint64_t(spaced[at]-'0')).first;
            i=last;
        } else {result+=cardinal(whole);i=end;}
    }
    return result;
}

struct KokoroText::Impl {
    Lexicon lexicon;
    std::map<std::u32string,int64_t> letters,vocabulary;
    std::vector<std::string> phones;
    std::unordered_map<std::string,std::string> neural_cache;
    Ort::Env env{ORT_LOGGING_LEVEL_WARNING,"orthodox-reader-kokoro-text"};
    Ort::SessionOptions options;
    Ort::Session encoder{nullptr},decoder{nullptr};
    explicit Impl(const std::filesystem::path& pack):lexicon(pack/"lexicon.tsv",pack/"custom_lexicon.tsv") {
        const auto g2p=read_file(pack/"g2p-config.json");
        letters=json_ids(g2p,"char2id");
        for(const auto& [phone,id]:json_ids(g2p,"phon2id")) {
            if(id<0||id>=1024)throw std::runtime_error("Invalid Kokoro pronunciation vocabulary.");
            if(phones.size()<=size_t(id))phones.resize(size_t(id)+1);
            phones[size_t(id)]=encode(phone);
        }
        vocabulary=json_ids(read_file(pack/"config.json"),"vocab");
        env.DisableTelemetryEvents();options.SetIntraOpNumThreads(1);options.SetInterOpNumThreads(1);
        options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        encoder=Ort::Session(env,(pack/"g2p-encoder.onnx").native().c_str(),options);
        decoder=Ort::Session(env,(pack/"g2p-decoder.onnx").native().c_str(),options);
    }
    // model.G2PTransformer.greedy_decode for one word: BOS letters EOS in,
    // up to max_len=64 phones out, stopping at EOS.
    std::string neural(const std::string& word) {
        if(const auto cached=neural_cache.find(word);cached!=neural_cache.end())return cached->second;
        constexpr int64_t pad=0,bos=1,eos=2,max_len=64;
        std::vector<int64_t> source{bos};
        for(char32_t c:decode(word))
            if(const auto id=letters.find(std::u32string(1,c));id!=letters.end())source.push_back(id->second);
        // The positional table holds 64 positions.
        if(source.size()>max_len-1)source.resize(max_len-1);
        source.push_back(eos);
        const auto memory_info=Ort::MemoryInfo::CreateCpu(OrtArenaAllocator,OrtMemTypeDefault);
        const std::array<int64_t,2> source_shape={1,int64_t(source.size())};
        auto source_tensor=Ort::Value::CreateTensor<int64_t>(memory_info,source.data(),source.size(),source_shape.data(),source_shape.size());
        const char* encoder_in[]={"src"};const char* encoder_out[]={"memory"};
        auto memory=encoder.Run(Ort::RunOptions{nullptr},encoder_in,&source_tensor,1,encoder_out,1);
        std::vector<int64_t> target{bos};
        std::string result;
        for(int64_t step=0;step<max_len;++step) {
            const std::array<int64_t,2> target_shape={1,int64_t(target.size())};
            std::array<Ort::Value,2> inputs={Ort::Value::CreateTensor<int64_t>(memory_info,target.data(),target.size(),target_shape.data(),target_shape.size()),std::move(memory[0])};
            const char* decoder_in[]={"tgt","memory"};const char* decoder_out[]={"logits"};
            auto logits=decoder.Run(Ort::RunOptions{nullptr},decoder_in,inputs.data(),inputs.size(),decoder_out,1);
            memory[0]=std::move(inputs[1]);
            const auto count=logits[0].GetTensorTypeAndShapeInfo().GetElementCount();
            const auto* values=logits[0].GetTensorData<float>();
            const auto next=int64_t(std::max_element(values,values+count)-values);
            if(next==eos)break;
            target.push_back(next);
            if(next!=pad&&next!=bos&&size_t(next)<phones.size())result+=phones[size_t(next)];
        }
        return neural_cache[word]=result;
    }
};

KokoroText::KokoroText(const std::filesystem::path& pack):impl_(std::make_unique<Impl>(pack)){}
KokoroText::~KokoroText()=default;

std::string KokoroText::phonemes(const std::string& input) {
    // g2p_infer.SwedishG2P.phonemize: sentences match [^.!?]+[.!?]?; tokens
    // are runs of the letters below or one of ,.!?;:- and all else is dropped.
    static const std::u32string word_letters=U"abcdefghijklmnopqrstuvwxyzåäöéèüáàâëïABCDEFGHIJKLMNOPQRSTUVWXYZÅÄÖ";
    static const std::u32string punctuation=U",.!?;:-";
    const auto is_letter=[](char32_t c){return word_letters.find(c)!=std::u32string::npos;};
    const auto end_mark=[](char32_t c){return c=='.'||c=='!'||c=='?';};
    const auto text=decode(swedish_numbers(input));
    std::string result;
    for(size_t i=0;i<text.size();) {
        if(end_mark(text[i])){++i;continue;}
        auto end=i;while(end<text.size()&&!end_mark(text[end]))++end;
        if(end<text.size())++end;
        std::string sentence;bool previous_word=false;
        for(size_t at=i;at<end;) {
            if(is_letter(text[at])) {
                auto last=at;while(last<end&&is_letter(text[last]))++last;
                auto word=text.substr(at,last-at);
                for(auto& c:word)if((c>='A'&&c<='Z')||c==U'Å'||c==U'Ä'||c==U'Ö')c+=32;
                const auto key=encode(word);
                auto ipa=impl_->lexicon.find(key);
                if(ipa.empty())ipa=impl_->neural(key);
                if(previous_word)sentence+=' ';
                sentence+=ipa;previous_word=true;at=last;
            } else {
                if(punctuation.find(text[at])!=std::u32string::npos){sentence+=char(text[at]);previous_word=false;}
                ++at;
            }
        }
        if(!sentence.empty())result+=(result.empty()?"":" ")+sentence;
        i=end;
    }
    // g2p_sv: NEURAL_FIXES, then KOKORO_REMAP in its order, then single spaces.
    replace_all(result,"ˈuːɕˌɛj","ˈuːkɛj");
    replace_all(result,"uːəsˈɛs","ˈɔs");
    for(const auto& [from,to]:std::initializer_list<std::pair<const char*,const char*>>{
            {"ʏ","y"},{"ʉ","ɨ"},{"ɵ","ɜ"},{"ɧ","ʂ"},{"ɭ","l"},{"-",""},{"\xcc\x83",""}})
        replace_all(result,from,to);
    std::string collapsed;
    for(char c:result)if(c!=' '||(!collapsed.empty()&&collapsed.back()!=' '))collapsed+=c;
    if(!collapsed.empty()&&collapsed.back()==' ')collapsed.pop_back();
    return collapsed;
}

std::vector<int64_t> KokoroText::tokens(const std::string& text) {
    std::vector<int64_t> result;
    for(char32_t c:decode(phonemes(text)))
        if(const auto id=impl_->vocabulary.find(std::u32string(1,c));id!=impl_->vocabulary.end())result.push_back(id->second);
    return result;
}
}
