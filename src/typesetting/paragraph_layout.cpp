#include "typesetting/paragraph_layout.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <wx/graphics.h>
#include <wx/tokenzr.h>
namespace ortho {
namespace {
struct Node {
    std::map<unsigned, std::size_t> children;
    std::vector<int> weights;
};
struct Patterns {
    std::vector<Node> trie{1};
    std::map<wxString, std::vector<int>> exceptions;
    int left = 2, right = 2;
};
std::map<std::string, Patterns> dictionaries;
bool combining(unsigned c) {
    return (c >= 0x300 && c <= 0x36f) || (c >= 0x1ab0 && c <= 0x1aff);
}
void add_pattern(Patterns& patterns, const wxString& pattern) {
    std::size_t node = 0;
    std::vector<int> weights{0};
    for (auto ch : pattern) {
        const unsigned c = ch.GetValue();
        if (c >= '0' && c <= '9') {
            weights.back() = int(c - '0');
            continue;
        }
        auto found = patterns.trie[node].children.find(c);
        if (found == patterns.trie[node].children.end()) {
            const auto next = patterns.trie.size();
            patterns.trie[node].children.emplace(c, next);
            patterns.trie.emplace_back();
            node = next;
        } else
            node = found->second;
        weights.push_back(0);
    }
    patterns.trie[node].weights = std::move(weights);
}
int font_category(unsigned c) {
    return c >= 0x590 && c <= 0x5ff ? 1 : 0;
}
struct ShapedPart {
    wxString text;
    wxFont font;
};
std::vector<ShapedPart> shaped_parts(const wxFont& base, const wxString& text) {
    std::vector<ShapedPart> parts;
    int previous = -1;
    for (auto c : text) {
        const int category = font_category(c.GetValue());
        if (category != previous) {
            auto font = base;
            if (category == 1)
                font.SetFaceName("Noto Serif Hebrew");
            parts.push_back({{}, font});
            previous = category;
        }
        parts.back().text += c;
    }
    return parts;
}
wxFont marker_font(const wxFont& base) {
    auto font = base;
    font.SetFractionalPointSize(base.GetFractionalPointSize() * 0.57);
    return font;
}
double shaped_width(wxDC& dc, wxGraphicsContext* gc, const wxFont& base, const wxString& text) {
    double width = 0;
    for (const auto& part : shaped_parts(base, text)) {
        if (gc) {
            const auto ppi = dc.GetPPI();
            gc->SetFont(gc->GetRenderer()->CreateFontAtDPI(part.font, wxRealPoint(ppi.x, ppi.y),
                                                           dc.GetTextForeground()));
            double w = 0;
            gc->GetTextExtent(part.text, &w, nullptr);
            width += w;
        } else {
            dc.SetFont(part.font);
            width += dc.GetTextExtent(part.text).x;
        }
    }
    dc.SetFont(base);
    return width;
}
struct Break {
    int word, cut;
    bool hyphen;
};
struct State {
    double cost = std::numeric_limits<double>::infinity();
    int previous = -1, fitness = 1;
};
} // namespace
void load_hyphenation(const std::filesystem::path& directory) {
    dictionaries.clear();
    for (const auto& [language, file, left, right] :
         std::vector<std::tuple<std::string, std::string, int, int>>{
             {"sv", "hyph-sv.tex", 2, 2}, {"el", "hyph-grc.tex", 1, 1}, {"en", "hyph-en-us.tex", 2, 3}}) {
        std::ifstream input(directory / file);
        if (!input)
            throw std::runtime_error("Bundled hyphenation patterns missing: " + file);
        std::string cleaned, line;
        while (std::getline(input, line)) {
            line = line.substr(0, line.find('%'));
            cleaned += line + "\n";
        }
        const auto begin = cleaned.find("\\patterns{");
        const auto end = cleaned.find('}', begin);
        if (begin == std::string::npos || end == std::string::npos)
            throw std::runtime_error("Invalid hyphenation file: " + file);
        Patterns patterns;
        patterns.left = left;
        patterns.right = right;
        std::istringstream tokens(cleaned.substr(begin + 10, end - begin - 10));
        std::string token;
        while (tokens >> token)
            add_pattern(patterns, wxString::FromUTF8(token));
        const auto exception_begin = cleaned.find("\\hyphenation{");
        if (exception_begin != std::string::npos) {
            const auto exception_end = cleaned.find('}', exception_begin);
            std::istringstream entries(
                cleaned.substr(exception_begin + 13, exception_end - exception_begin - 13));
            while (entries >> token) {
                wxString word;
                std::vector<int> points;
                for (auto c : wxString::FromUTF8(token)) {
                    if (c == '-')
                        points.push_back(int(word.length()));
                    else
                        word += c;
                }
                patterns.exceptions.emplace(word, std::move(points));
            }
        }
        dictionaries.emplace(language, std::move(patterns));
    }
}
std::vector<int> hyphenation_points(const wxString& word, const std::string& language) {
    auto it = dictionaries.find(language);
    if (it == dictionaries.end())
        return {};
    const auto& patterns = it->second;
    // Strip punctuation only for pattern matching; keep the source spelling.
    int first = 0, last = int(word.length());
    while (first < last && !wxIsalpha(word[first]))
        ++first;
    while (last > first && !wxIsalpha(word[last - 1]) && !combining(word[last - 1].GetValue()))
        --last;
    auto folded = word.Mid(first, last - first).Lower();
    for (auto c : folded)
        if (!wxIsalpha(c) && !combining(c.GetValue()))
            return {};
    if (auto exception = patterns.exceptions.find(folded); exception != patterns.exceptions.end()) {
        auto points = exception->second;
        for (auto& p : points)
            p += first;
        return points;
    }
    wxString padded = "." + folded + ".";
    std::vector<int> weights(padded.length() + 1, 0);
    for (std::size_t start = 0; start < padded.length(); ++start) {
        std::size_t node = 0;
        for (std::size_t i = start; i < padded.length(); ++i) {
            auto child = patterns.trie[node].children.find(padded[i].GetValue());
            if (child == patterns.trie[node].children.end())
                break;
            node = child->second;
            const auto& w = patterns.trie[node].weights;
            for (std::size_t k = 0; k < w.size(); ++k)
                weights[start + k] = std::max(weights[start + k], w[k]);
        }
    }
    std::vector<int> result;
    for (int i = patterns.left; i <= int(folded.length()) - patterns.right; ++i)
        if ((weights[i + 1] & 1) && !combining(folded[i].GetValue()))
            result.push_back(first + i);
    return result;
}
TextLayout layout_paragraph(wxDC& dc, const wxString& text, int width, const std::string& language) {
    return layout_paragraph(dc, std::vector<TextFragment>{{text, {}}}, width, language);
}
TextLayout layout_paragraph(wxDC& dc, const std::vector<TextFragment>& fragments, int width,
                            const std::string& language) {
    TextLayout result;
    result.line_height = std::max(1, int(dc.GetTextExtent(wxString::FromUTF8("Ågjἄ")).GetHeight() * 1.05));
    width = std::max(1, width);
    std::vector<wxString> words, labels;
    std::vector<int> tags;
    for (const auto& fragment : fragments) {
        wxStringTokenizer tokens(fragment.text, " \t\r\n");
        bool first = true;
        while (tokens.HasMoreTokens()) {
            words.push_back(tokens.GetNextToken());
            labels.push_back(first ? fragment.label : wxString{});
            tags.push_back(fragment.tag);
            first = false;
        }
    }
    if (words.empty()) {
        result.lines.emplace_back();
        return result;
    }
    const int n = int(words.size());
    const auto base_font = dc.GetFont();
    std::unique_ptr<wxGraphicsContext> measuring(
        wxGraphicsRenderer::GetDefaultRenderer()->CreateMeasuringContext());
    const double natural_space = std::max(1.0, shaped_width(dc, measuring.get(), base_font, " "));
    std::vector<double> prefixes;
    for (const auto& label : labels)
        prefixes.push_back(label.empty() ? 0
                                         : shaped_width(dc, measuring.get(), marker_font(base_font), label) +
                                               natural_space * 0.5);
    std::map<std::tuple<int, int, int, bool>, double> measures;
    auto measure = [&](int w, int first, int last, bool hyphen) {
        const auto key = std::tuple{w, first, last, hyphen};
        if (auto it = measures.find(key); it != measures.end())
            return it->second;
        const auto run = words[w].Mid(first, last - first) + (hyphen ? "-" : "");
        const double value =
            shaped_width(dc, measuring.get(), base_font, run) + (first == 0 ? prefixes[w] : 0);
        measures.emplace(key, value);
        return value;
    };
    std::vector<double> sums(n + 1, 0);
    std::vector<Break> breaks{{0, 0, false}};
    for (int w = 0; w < n; ++w) {
        auto points = hyphenation_points(words[w], language);
        sums[w + 1] = sums[w] + measure(w, 0, int(words[w].length()), false);
        // An exceptionally long token still has a grapheme-safe emergency route.
        if (sums[w + 1] - sums[w] > width && points.empty()) {
            for (int k = 2; k < int(words[w].length()) - 1; ++k)
                if (!combining(words[w][k].GetValue()))
                    points.push_back(k);
        }
        for (int k : points)
            breaks.push_back({w, k, true});
        breaks.push_back({w + 1, 0, false});
    }
    std::map<unsigned, double> character_widths;
    const auto protrusion = [&](const wxString& word, bool left) {
        const auto character = left ? word[0] : word[word.length() - 1];
        const unsigned c = character.GetValue();
        double fraction = 0;
        if (left && (c == '"' || c == '\'' || c == 0x2018 || c == 0x201c || c == 0xab))
            fraction = 0.5;
        if (!left) {
            if (c == '.' || c == ',')
                fraction = 0.7;
            else if (c == ':' || c == ';' || c == 0x387)
                fraction = 0.45;
            else if (c == '"' || c == '\'' || c == 0x2019 || c == 0x201d || c == 0xbb)
                fraction = 0.5;
            else if (c == '-')
                fraction = 0.2;
        }
        if (!fraction)
            return 0.0;
        auto [found, inserted] = character_widths.try_emplace(c, 0);
        if (inserted)
            found->second = shaped_width(dc, measuring.get(), base_font, wxString(character));
        return found->second * fraction;
    };
    auto line_measure = [&](const Break& a, const Break& b) {
        const int last = b.word - (b.cut == 0 ? 1 : 0);
        const int gaps = last - a.word;
        double value = 0;
        if (last == a.word)
            value = measure(a.word, a.cut, b.cut ? b.cut : int(words[last].length()), b.hyphen);
        else {
            value = measure(a.word, a.cut, int(words[a.word].length()), false);
            value += sums[last] - sums[a.word + 1];
            value += measure(last, 0, b.cut ? b.cut : int(words[last].length()), b.hyphen);
        }
        const double left =
            (a.cut == 0 && !labels[a.word].empty()) ? 0 : protrusion(words[a.word].Mid(a.cut), true);
        const auto tail =
            words[last].Mid(last == a.word ? a.cut : 0,
                            (b.cut ? b.cut : int(words[last].length())) - (last == a.word ? a.cut : 0)) +
            (b.hyphen ? "-" : "");
        const double right = protrusion(tail, false);
        return std::tuple{value + gaps * natural_space - left - right, gaps, left, right};
    };
    std::vector<std::array<State, 4>> states;
    int final_fitness = 1;
    for (int pass = 0; pass < 3; ++pass) {
        states.assign(breaks.size(), {});
        states[0][1].cost = 0;
        for (int j = 1; j < int(breaks.size()); ++j) {
            if (pass == 0 && breaks[j].hyphen)
                continue;
            for (int i = j - 1; i >= 0; --i) {
                const auto [natural, gaps, left, right] = line_measure(breaks[i], breaks[j]);
                const double difference = width - natural;
                if (natural > width + gaps * natural_space / 3.0) {
                    if (breaks[j].word - breaks[i].word > 1)
                        break;
                    if (pass < 2)
                        continue;
                }
                if (pass == 0 && breaks[i].hyphen)
                    continue;
                const bool last = j == int(breaks.size()) - 1;
                const double capacity =
                    std::max(1.0, gaps * natural_space * (difference < 0 ? 1.0 / 3.0 : 0.5) +
                                      (pass == 2 ? width * 0.08 : 0));
                double ratio = difference / capacity;
                if (last && difference >= 0)
                    ratio = 0;
                const double badness = 100 * std::pow(std::abs(ratio), 3);
                if (pass < 2 && (ratio < -1 || badness > (pass == 0 ? 100 : 200)))
                    continue;
                const int fitness = ratio < -0.5 ? 0 : ratio <= 0.5 ? 1 : ratio <= 1 ? 2 : 3;
                for (int f = 0; f < 4; ++f)
                    if (std::isfinite(states[i][f].cost)) {
                        double cost = states[i][f].cost + std::pow(10 + std::min(10000.0, badness), 2);
                        if (breaks[j].hyphen)
                            cost += 2500;
                        if (breaks[i].hyphen && breaks[j].hyphen)
                            cost += 10000;
                        if (last && breaks[i].hyphen)
                            cost += 5000;
                        if (std::abs(f - fitness) > 1)
                            cost += 10000;
                        // Emergency stretching remains bounded on the page.
                        if (!last && gaps && difference / gaps > natural_space)
                            cost += 1e7;
                        if (cost < states[j][fitness].cost)
                            states[j][fitness] = {cost, i, f};
                    }
            }
        }
        final_fitness = int(std::min_element(states.back().begin(), states.back().end(),
                                             [](auto& a, auto& b) {
                                                 return a.cost < b.cost;
                                             }) -
                            states.back().begin());
        if (std::isfinite(states.back()[final_fitness].cost))
            break;
    }
    std::vector<std::pair<int, int>> path;
    for (int j = int(breaks.size()) - 1, f = final_fitness; j > 0;) {
        const auto state = states[j][f];
        if (state.previous < 0)
            throw std::runtime_error("No paragraph break path");
        path.emplace_back(state.previous, j);
        j = state.previous;
        f = state.fitness;
    }
    std::reverse(path.begin(), path.end());
    result.demerits = states.back()[final_fitness].cost;
    for (const auto& [i, j] : path) {
        const auto& a = breaks[i];
        const auto& b = breaks[j];
        const auto [natural, gaps, left, right] = line_measure(a, b);
        TextLine line;
        line.hyphenated = b.hyphen;
        line.left_protrusion = left;
        line.right_protrusion = right;
        const bool last = j == int(breaks.size()) - 1;
        line.justified = !last && gaps > 0;
        line.space = line.justified ? natural_space + (width - natural) / gaps : natural_space;
        double x = -left;
        const int final_word = b.word - (b.cut == 0 ? 1 : 0);
        for (int w = a.word; w <= final_word; ++w) {
            const int start = w == a.word ? a.cut : 0;
            const int end = w == final_word && b.cut ? b.cut : int(words[w].length());
            const bool hyphen = w == final_word && b.hyphen;
            auto run = words[w].Mid(start, end - start) + (hyphen ? "-" : "");
            const auto extent = measure(w, start, end, hyphen);
            const double prefix = start == 0 ? prefixes[w] : 0;
            if (prefix > 0)
                line.runs.push_back({labels[w], x, prefix, tags[w], true});
            line.runs.push_back({run, x + prefix, extent - prefix, tags[w], false});
            x += extent + (w < final_word ? line.space : 0);
        }
        line.width = x - right;
        result.lines.push_back(std::move(line));
    }
    return result;
}
void draw_paragraph(wxDC& dc, const TextLayout& layout, int x, int y,
                    const std::function<std::optional<wxColour>(const TextRun&)>& colour) {
    if (layout.line_height <= 0)
        return;
    const auto base = dc.GetFont();
    const auto ink = dc.GetTextForeground();
    const int bottom = dc.GetSize().y;
    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::CreateFromUnknownDC(dc));
    if (gc) {
        wxRealPoint dpi;
        gc->GetDPI(&dpi.x, &dpi.y);
        const auto first = std::min(layout.lines.size(), std::size_t(std::max(0, -y / layout.line_height)));
        const auto end =
            std::min(layout.lines.size(),
                     std::size_t(std::max(0, (bottom - y + layout.line_height - 1) / layout.line_height)));
        draw_paragraph(*gc, layout, base, dpi, ink, x, y, first, end, colour);
        return;
    }
    for (const auto& line : layout.lines) {
        if (y + layout.line_height > 0 && y < bottom)
            for (const auto& run : line.runs) {
                const auto chosen = colour ? colour(run) : std::nullopt;
                const auto text_colour = chosen ? *chosen : ink;
                const auto parts = shaped_parts(run.marker ? marker_font(base) : base, run.text);
                double part_x = x + run.x;
                for (std::size_t i = 0; i < parts.size(); ++i) {
                    dc.SetFont(parts[i].font);
                    dc.SetTextForeground(text_colour);
                    dc.DrawText(parts[i].text, int(std::lround(part_x)), y);
                    if (i + 1 < parts.size())
                        part_x += dc.GetTextExtent(parts[i].text).x;
                }
            }
        y += layout.line_height;
    }
    dc.SetFont(base);
    dc.SetTextForeground(ink);
}
void draw_paragraph(wxGraphicsContext& gc, const TextLayout& layout, const wxFont& base,
                    const wxRealPoint& dpi, const wxColour& ink, int x, int y, std::size_t first,
                    std::size_t end, const std::function<std::optional<wxColour>(const TextRun&)>& colour) {
    // GDI+ builds a new font each time one is set, so set it only when the font or colour changes.
    wxFont font;
    wxColour font_colour;
    for (auto line = first; line < std::min(end, layout.lines.size()); ++line) {
        const int line_y = y + int(line) * layout.line_height;
        for (const auto& run : layout.lines[line].runs) {
            const auto chosen = colour ? colour(run) : std::nullopt;
            const auto text_colour = chosen ? *chosen : ink;
            const auto parts = shaped_parts(run.marker ? marker_font(base) : base, run.text);
            double part_x = x + run.x;
            for (std::size_t i = 0; i < parts.size(); ++i) {
                // The layout places whole runs; measure a part only to place the next one.
                const bool more = i + 1 < parts.size();
                if (font != parts[i].font || font_colour != text_colour) {
                    font = parts[i].font;
                    font_colour = text_colour;
                    gc.SetFont(gc.GetRenderer()->CreateFontAtDPI(font, dpi, font_colour));
                }
                gc.DrawText(parts[i].text, part_x, line_y);
                double width = 0;
                if (more)
                    gc.GetTextExtent(parts[i].text, &width, nullptr);
                part_x += width;
            }
        }
    }
}
} // namespace ortho
