#include "storage/content_import.hpp"
#include "storage/sqlite.hpp"
extern "C" {
#include <aes.h>
}
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <miniz_tinfl.h>
#include <picosha2.h>
#include <span>
#include <stdexcept>

namespace ortho {
namespace {
using Bytes = std::vector<unsigned char>;
constexpr std::array<const char*, 66> books{
    "Gen",   "Exod",     "Lev",  "Num",   "Deut",   "Josh",   "Judg",  "Ruth",   "1Sam",   "2Sam",  "1Kgs",
    "2Kgs",  "1Chr",     "2Chr", "Ezra",  "Neh",    "Esth",   "Job",   "Ps",     "Prov",   "Eccl",  "Song",
    "Isa",   "Jer",      "Lam",  "Ezek",  "Dan",    "Hos",    "Joel",  "Amos",   "Obad",   "Jonah", "Micah",
    "Nah",   "Hab",      "Zeph", "Hag",   "Zech",   "Mal",    "Matt",  "Mark",   "Luke",   "John",  "Acts",
    "Rom",   "1Cor",     "2Cor", "Gal",   "Eph",    "Phil",   "Col",   "1Thess", "2Thess", "1Tim",  "2Tim",
    "Titus", "Philemon", "Heb",  "James", "1Peter", "2Peter", "1John", "2John",  "3John",  "Jude",  "Rev"};
[[noreturn]] void invalid(const std::string& message) {
    throw std::runtime_error(message);
}
Bytes read_file(const std::filesystem::path& path, std::size_t maximum) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file || file.tellg() < 0 || static_cast<std::uint64_t>(file.tellg()) > maximum)
        invalid("Filen kunde inte läsas eller har fel storlek.");
    Bytes data(static_cast<std::size_t>(file.tellg()));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size())))
        invalid("Filen kunde inte läsas helt.");
    return data;
}
struct Reader {
    std::span<const unsigned char> data;
    std::size_t at = 0;
    unsigned number(unsigned size) {
        if (size > data.size() - at)
            invalid("Importmallen är ofullständig.");
        unsigned result = 0;
        for (unsigned i = 0; i < size; ++i)
            result |= unsigned(data[at++]) << (8 * i);
        return result;
    }
    std::string string(std::size_t size) {
        if (size > data.size() - at)
            invalid("Importmallen är ofullständig.");
        std::string result(reinterpret_cast<const char*>(data.data() + at), size);
        at += size;
        return result;
    }
};
Bytes inflate(std::span<const unsigned char> data, std::size_t size) {
    if (!size || size > std::size_t{32} * 1024 * 1024)
        invalid("Fel storlek på importerade data.");
    Bytes out(size);
    if (tinfl_decompress_mem_to_mem(out.data(), out.size(), data.data(), data.size(),
                                    TINFL_FLAG_PARSE_ZLIB_HEADER) != size)
        invalid("PDF-innehållet kunde inte packas upp.");
    return out;
}
// Read only string tokens from the pinned content streams. No PDF objects, actions or scripts execute.
Bytes string_bytes(const Bytes& data) {
    Bytes out;
    std::size_t i = 0;
    const auto next = [&]() {
        if (i == data.size())
            invalid("Ofullständig PDF-text.");
        return data[i++];
    };
    const auto hex = [](unsigned char c) {
        if (c >= '0' && c <= '9')
            return int(c - '0');
        if (c >= 'a' && c <= 'f')
            return int(c - 'a' + 10);
        if (c >= 'A' && c <= 'F')
            return int(c - 'A' + 10);
        invalid("Ogiltig PDF-textkodning.");
    };
    while (i < data.size()) {
        auto c = next();
        if (c == '%') {
            while (i < data.size() && data[i] != '\n' && data[i] != '\r')
                ++i;
        } else if (c == '(') {
            unsigned depth = 1;
            while (depth) {
                c = next();
                if (c == '\\') {
                    c = next();
                    if (c >= '0' && c <= '7') {
                        unsigned value = c - '0';
                        for (int n = 0; n < 2 && i < data.size() && data[i] >= '0' && data[i] <= '7'; ++n)
                            value = value * 8 + next() - '0';
                        out.push_back(static_cast<unsigned char>(value));
                    } else if (c == '\n' || c == '\r') {
                        if (c == '\r' && i < data.size() && data[i] == '\n')
                            ++i;
                    } else {
                        switch (c) {
                        case 'n':
                            c = '\n';
                            break;
                        case 'r':
                            c = '\r';
                            break;
                        case 't':
                            c = '\t';
                            break;
                        case 'b':
                            c = '\b';
                            break;
                        case 'f':
                            c = '\f';
                            break;
                        default:
                            break;
                        }
                        out.push_back(c);
                    }
                } else if (c == '(') {
                    ++depth;
                    out.push_back(c);
                } else if (c == ')') {
                    if (--depth)
                        out.push_back(c);
                } else
                    out.push_back(c);
            }
        } else if (c == '<') {
            if (i < data.size() && data[i] == '<') {
                ++i;
                continue;
            }
            int high = -1;
            while ((c = next()) != '>') {
                if (std::isspace(c))
                    continue;
                const int value = hex(c);
                if (high < 0)
                    high = value;
                else {
                    out.push_back(static_cast<unsigned char>(high * 16 + value));
                    high = -1;
                }
            }
            if (high >= 0)
                out.push_back(static_cast<unsigned char>(high * 16));
        }
    }
    return out;
}
std::string normalized(const std::string& text) {
    std::string out;
    bool space = false;
    for (unsigned char c : text) {
        if (std::isspace(c))
            space = !out.empty();
        else {
            if (space)
                out += ' ';
            out += char(c);
            space = false;
        }
    }
    return out;
}
struct ImportedVerse {
    unsigned book, chapter, verse;
    std::string text;
};
struct ImportedAlignment {
    std::string from, to;
    Alignment value;
};
} // namespace
ContentImportResult import_reformationsbibeln(const std::filesystem::path& old_testament,
                                              const std::filesystem::path& new_testament,
                                              const std::filesystem::path& recipe,
                                              const std::filesystem::path& destination,
                                              const ImportProgress& progress) {
    const auto report = [&](int percent) {
        if (progress)
            progress(percent);
    };
    std::array<Bytes, 2> pdfs{read_file(old_testament, 5453146), read_file(new_testament, 2990947)};
    const std::array<std::string, 2> hashes{
        "f4b3c7f1acb68dd8cb976c1a518bd5338dab8b7d8bda7340e90fdcfaaabd4b70",
        "59046fcd7f3633bcfd3e46b1778712101a1c820a2714069a757619cbe8b63b75"};
    for (std::size_t i = 0; i < pdfs.size(); ++i)
        if (picosha2::hash256_hex_string(pdfs[i]) != hashes[i])
            invalid(
                i == 0
                    ? "Gamla testamentets PDF stämmer inte med den stödda utgåvan. Hämta rätt fil via länken."
                    : "Nya testamentets PDF stämmer inte med den stödda utgåvan. Hämta rätt fil via länken.");
    report(5);
    const auto packed = read_file(recipe, std::size_t{16} * 1024 * 1024);
    if (packed.size() < 4)
        invalid("Importmallen är ofullständig.");
    Reader header{packed};
    const auto unpacked_size = header.number(4);
    const auto unpacked = inflate(std::span(packed).subspan(4), unpacked_size);
    Reader input{unpacked};
    if (input.string(4) != "SRB2")
        invalid("Fel version av importmallen.");
    const auto font_count = input.number(2);
    if (font_count > 254)
        invalid("För många PDF-kodningar.");
    std::vector<std::array<std::string, 256>> fonts(font_count);
    for (auto& font : fonts)
        for (auto& letter : font)
            letter = input.string(input.number(1));
    const auto stream_count = input.number(4);
    if (stream_count > 4000)
        invalid("För många PDF-sidor.");
    std::vector<Bytes> streams;
    streams.reserve(stream_count);
    for (unsigned n = 0; n < stream_count; ++n) {
        const auto half = input.number(1), offset = input.number(4), length = input.number(4),
                   size = input.number(4);
        const auto key = input.string(16);
        if (half > 1 || offset > pdfs[half].size() || length > pdfs[half].size() - offset || length < 32 ||
            length % 16)
            invalid("Felaktigt PDF-område.");
        const auto encrypted = std::span(pdfs[half]).subspan(offset, length);
        Bytes compressed(encrypted.begin() + 16, encrypted.end());
        AES_ctx aes{};
        AES_init_ctx_iv(&aes, reinterpret_cast<const unsigned char*>(key.data()), encrypted.data());
        AES_CBC_decrypt_buffer(&aes, compressed.data(), compressed.size());
        const auto padding = compressed.back();
        if (!padding || padding > 16 ||
            !std::all_of(compressed.end() - padding, compressed.end(), [padding](unsigned char c) {
                return c == padding;
            }))
            invalid("PDF-innehållet kunde inte avkodas.");
        compressed.resize(compressed.size() - padding);
        streams.push_back(string_bytes(inflate(compressed, size)));
        report(5 + int(65 * (n + 1) / stream_count));
    }
    const auto count = input.number(4);
    if (count != 31171)
        invalid("Importmallen saknar bibelverser.");
    std::vector<ImportedVerse> verses;
    verses.reserve(count);
    std::string complete_text;
    const auto text_pieces = [&]() {
        std::string text;
        const auto pieces = input.number(2);
        for (unsigned p = 0; p < pieces; ++p) {
            const auto stream = input.number(2), offset = input.number(4), length = input.number(2),
                       font = input.number(1);
            if (font == 255) {
                text += ' ';
                continue;
            }
            if (stream >= streams.size() || font >= fonts.size() || offset > streams[stream].size() ||
                length > streams[stream].size() - offset)
                invalid("Felaktigt textområde i importmallen.");
            for (auto c : std::span(streams[stream]).subspan(offset, length))
                text += fonts[font][c];
        }
        return normalized(text);
    };
    for (unsigned n = 0; n < count; ++n) {
        ImportedVerse verse{input.number(1), input.number(1), input.number(1), {}};
        if (verse.book >= books.size() || !verse.chapter || !verse.verse)
            invalid("Felaktig vers i importmallen.");
        verse.text = text_pieces();
        if (verse.text.empty())
            invalid("En importerad vers saknar text.");
        complete_text += verse.text + '\n';
        verses.push_back(std::move(verse));
    }
    const auto alignment_count = input.number(4);
    if (alignment_count > 30000)
        invalid("För många versmappningar.");
    const std::array<std::string, 6> sources{srb_source,        "sv1917",     "grc-lxx",
                                             "grc-patriarchal", "en-lxx2012", "en-web"};
    std::vector<ImportedAlignment> alignments;
    const auto reference = [&]() {
        return VerseRef{int(input.number(2)), int(input.number(2)), input.string(input.number(1))};
    };
    for (unsigned n = 0; n < alignment_count; ++n) {
        const auto from = input.number(1), to = input.number(1), kind = input.number(1);
        if (from >= sources.size() || to >= sources.size() || kind > 6)
            invalid("Felaktig versmappning.");
        const auto book = input.string(input.number(1)), target = input.string(input.number(1));
        const auto first = reference(), last = reference(), target_first = reference(),
                   target_last = reference();
        alignments.push_back(
            {sources[from],
             sources[to],
             {static_cast<AlignmentKind>(kind), {book, first, last}, {target, target_first, target_last}}});
    }
    std::vector<ImportedVerse> headings;
    const auto heading_count = input.number(4);
    if (heading_count > 10000)
        invalid("För många avsnittsrubriker.");
    std::string complete_headings;
    for (unsigned n = 0; n < heading_count; ++n) {
        ImportedVerse heading{input.number(1), input.number(1), input.number(1), {}};
        heading.text = text_pieces();
        if (heading.text.empty() || !std::ranges::any_of(verses, [&](const auto& verse) {
                return verse.book == heading.book && verse.chapter == heading.chapter &&
                       verse.verse == heading.verse;
            }))
            invalid("En avsnittsrubrik saknar text eller tillhörande vers.");
        complete_headings += heading.text + '\n';
        headings.push_back(std::move(heading));
    }
    if (input.string(64) != picosha2::hash256_hex_string(complete_headings))
        invalid("Kontrollen av avsnittsrubriker misslyckades.");
    const auto expected_text = input.string(64);
    if (input.at != input.data.size() || picosha2::hash256_hex_string(complete_text) != expected_text)
        invalid("Textkontrollen misslyckades. Inget innehåll har installerats.");
    report(75);
    std::filesystem::create_directories(destination.parent_path());
    auto db = storage::open(destination, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
    storage::Transaction transaction(db.get());
    storage::exec(
        db.get(),
        "CREATE TABLE IF NOT EXISTS content_metadata(key TEXT PRIMARY KEY,value TEXT NOT NULL) STRICT;"
        "CREATE TABLE IF NOT EXISTS imported_verse(book TEXT NOT NULL,chapter INTEGER NOT NULL,"
        "verse INTEGER NOT NULL,text TEXT NOT NULL,PRIMARY KEY(book,chapter,verse)) STRICT, WITHOUT ROWID;"
        "CREATE TABLE IF NOT EXISTS imported_heading(book TEXT NOT NULL,chapter INTEGER NOT NULL,"
        "verse INTEGER NOT NULL,ordinal INTEGER NOT NULL,text TEXT NOT NULL,"
        "PRIMARY KEY(book,chapter,verse,ordinal)) STRICT, WITHOUT ROWID;"
        "CREATE TABLE IF NOT EXISTS imported_alignment(source TEXT,target TEXT,book TEXT,kind INTEGER,"
        "fc INTEGER,fv INTEGER,fs TEXT,lc INTEGER,lv INTEGER,ls TEXT,tbook TEXT,"
        "tfc INTEGER,tfv INTEGER,tfs TEXT,tlc INTEGER,tlv INTEGER,tls TEXT,"
        "PRIMARY KEY(source,target,book,fc,fv,fs)) STRICT, WITHOUT ROWID;"
        "DELETE FROM imported_verse; DELETE FROM imported_heading; DELETE FROM imported_alignment; DELETE "
        "FROM content_metadata;");
    storage::Statement insert(db.get(), "INSERT INTO imported_verse VALUES(?,?,?,?)");
    for (const auto& verse : verses) {
        insert.text(1, books[verse.book]);
        insert.number(2, int(verse.chapter));
        insert.number(3, int(verse.verse));
        insert.text(4, verse.text);
        insert.row();
        insert.reset();
    }
    storage::Statement heading_insert(db.get(), "INSERT INTO imported_heading VALUES(?,?,?,?,?)");
    for (std::size_t n = 0; n < headings.size(); ++n) {
        const auto& heading = headings[n];
        heading_insert.text(1, books[heading.book]);
        heading_insert.number(2, int(heading.chapter));
        heading_insert.number(3, int(heading.verse));
        heading_insert.number(4, int(n));
        heading_insert.text(5, heading.text);
        heading_insert.row();
        heading_insert.reset();
    }
    storage::Statement mapping(db.get(),
                               "INSERT INTO imported_alignment VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
    for (const auto& row : alignments) {
        mapping.text(1, row.from);
        mapping.text(2, row.to);
        mapping.text(3, row.value.from.book);
        mapping.number(4, int(row.value.kind));
        const auto bind_ref = [&](int column, const VerseRef& ref) {
            mapping.number(column, ref.chapter);
            mapping.number(column + 1, ref.verse);
            mapping.text(column + 2, ref.suffix);
        };
        bind_ref(5, row.value.from.first);
        bind_ref(8, row.value.from.last);
        mapping.text(11, row.value.to.book);
        bind_ref(12, row.value.to.first);
        bind_ref(15, row.value.to.last);
        mapping.row();
        mapping.reset();
    }
    storage::Statement metadata(db.get(), "INSERT INTO content_metadata VALUES('srb2026',?)");
    metadata.text(1, expected_text);
    metadata.row();
    transaction.commit();
    report(100);
    return {66, int(verses.size())};
}
} // namespace ortho
