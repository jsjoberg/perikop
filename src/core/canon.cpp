// The book order of the Orthodox Study Bible, framed by Septuagint numbering.
#include "core/model.hpp"
namespace ortho {
const std::vector<CanonBook>& osb_canon() {
    // Brenton's Greek joins Ezra and Nehemiah as 2 Esdras 1-23 and prints
    // Esther with its additions as one book; the OSB separates and names them.
    static const std::vector<CanonBook> books=[] {
        std::vector<CanonBook> result;
        for(const char* code:{"Gen","Exod","Lev","Num","Deut","Josh","Judg","Ruth","1Sam","2Sam","1Kgs","2Kgs","1Chr","2Chr","PrMan","1Esd"})
            result.push_back({code,code});
        result.push_back({"Ezra","Ezra",1,10});
        result.push_back({"Neh","Ezra",11,23});
        for(const char* code:{"Tob","Jdt"})result.push_back({code,code});
        result.push_back({"Esth","EsthGr"});
        for(const char* code:{"1Macc","2Macc","3Macc","Ps","Job","Prov","Eccl","Song","Wis","Sir",
            "Hos","Amos","Micah","Joel","Obad","Jonah","Nah","Hab","Zeph","Hag","Zech","Mal",
            "Isa","Jer","Baruch","Lam","EpJer","Ezek","Sus","Dan","Bel"})result.push_back({code,code});
        for(const char* code:{"Matt","Mark","Luke","John","Acts","Rom","1Cor","2Cor","Gal","Eph","Phil","Col","1Thess","2Thess",
            "1Tim","2Tim","Titus","Philemon","Heb","James","1Peter","2Peter","1John","2John","3John","Jude","Rev"})
            result.push_back({code,code,1,999,true});
        return result;
    }();
    return books;
}
const CanonBook* canon_book(const std::string& frame_book,int chapter) {
    for(const auto& book:osb_canon())
        if(book.frame_book==frame_book&&book.first_chapter<=chapter&&chapter<=book.last_chapter)return &book;
    return nullptr;
}
std::string frame_source(const std::string& language,const std::string& book) {
    return new_testament_book(book)?source_for_language(language,book):"grc-lxx";
}
}
