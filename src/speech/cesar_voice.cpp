#include "speech/cesar_voice.hpp"
#include "speech/klatt.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <stdexcept>
#include <string_view>
namespace ortho {
namespace {
// rsynth's sound elements that Cesar uses, unchanged: name, rank, stressed and unstressed
// frames, vowel, then per parameter {steady, fixed, proportion, external, internal}.
constexpr KlattElement rsynth[] = {
    {"Q",
     29,
     6,
     6,
     false,
     {{{270, 135, 50, 3, 3},
       {490, 0, 100, 3, 3},
       {1480, 0, 100, 3, 3},
       {2500, 0, 100, 3, 3},
       {60, 0, 100, 3, 3},
       {90, 0, 100, 3, 3},
       {150, 0, 100, 3, 3},
       {-30, -10.5, 100, 3, 0},
       {-30, -10.5, 100, 3, 0},
       {-30, -10.5, 100, 3, 0},
       {-30, -10.5, 100, 3, 0},
       {-30, -10.5, 100, 3, 0},
       {-30, 0, 100, 3, 0},
       {-30, 0, 100, 3, 0},
       {-30, 0, 100, 3, 0},
       {0, 0, 50, 0, 0},
       {0, 0, 50, 0, 0},
       {0, 0, 50, 0, 0},
       {0, 0, 50, 0, 0}}}},
    {"P",
     23,
     8,
     8,
     false,
     {{{270, 135, 50, 0, 0},
       {190, 110, 50, 2, 2},
       {760, 350, 50, 2, 2},
       {2500, 0, 100, 0, 2},
       {60, 30, 50, 2, 2},
       {90, 45, 50, 2, 2},
       {150, 0, 100, 0, 2},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {0, 0, 50, 0, 0},
       {0, 0, 50, 0, 0},
       {60, 30, 50, 0, 0},
       {60, 30, 50, 0, 0}}}},
    {"PY",
     29,
     1,
     1,
     false,
     {{{270, 135, 50, 0, 0},
       {190, 0, 100, 0, 0},
       {760, 0, 100, 0, 0},
       {2500, 0, 100, 0, 0},
       {60, 0, 100, 0, 0},
       {90, 0, 100, 0, 0},
       {150, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {24.5, 0, 100, 0, 0},
       {49, 0, 100, 0, 0},
       {43.75, 0, 100, 0, 0},
       {38.5, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {0, 0, 50, 0, 0},
       {0, 0, 50, 0, 0},
       {60, 30, 50, 0, 0},
       {60, 30, 50, 0, 0}}}},
    {"PZ",
     23,
     2,
     2,
     false,
     {{{270, 135, 50, 0, 0},
       {190, 110, 50, 2, 2},
       {760, 350, 50, 2, 2},
       {2500, 0, 100, 2, 2},
       {60, 30, 50, 2, 2},
       {90, 45, 50, 2, 2},
       {150, 0, 100, 2, 2},
       {-30, 0, 100, 0, 0},
       {24.5, 0, 100, 0, 0},
       {38.5, 0, 100, 0, 0},
       {33.25, 0, 100, 0, 0},
       {28, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {0, 0, 50, 0, 0},
       {0, 0, 50, 0, 0},
       {60, 30, 50, 0, 0},
       {60, 30, 50, 0, 0}}}},
    {"T",
     23,
     6,
     6,
     false,
     {{{270, 135, 50, 0, 0},
       {190, 110, 50, 2, 2},
       {1780, 950, 50, 2, 2},
       {2680, 2680, 0, 0, 2},
       {60, 30, 50, 2, 2},
       {90, 45, 50, 2, 2},
       {150, 150, 0, 0, 2},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {0, 0, 50, 0, 0},
       {0, 0, 50, 0, 0},
       {60, 30, 50, 0, 0},
       {60, 30, 50, 0, 0}}}},
    {"TY",
     29,
     1,
     1,
     false,
     {{{270, 135, 50, 0, 0},
       {190, 0, 100, 0, 0},
       {1780, 0, 100, 0, 0},
       {2680, 0, 100, 0, 0},
       {60, 0, 100, 0, 0},
       {90, 0, 100, 0, 0},
       {150, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {38.5, 0, 100, 0, 0},
       {50.75, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {0, 0, 50, 0, 0},
       {0, 0, 50, 0, 0},
       {60, 30, 50, 0, 0},
       {60, 30, 50, 0, 0}}}},
    {"TZ",
     23,
     2,
     2,
     false,
     {{{270, 135, 50, 0, 0},
       {190, 110, 50, 2, 1},
       {1780, 950, 50, 2, 1},
       {2680, 2680, 0, 2, 0},
       {60, 30, 50, 2, 1},
       {90, 45, 50, 2, 1},
       {150, 150, 0, 2, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {28, 0, 100, 0, 0},
       {40.25, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {0, 0, 50, 0, 0},
       {0, 0, 50, 0, 0},
       {60, 30, 50, 0, 0},
       {60, 30, 50, 0, 0}}}},
    {"K",
     23,
     8,
     8,
     false,
     {{{270, 135, 50, 0, 0},
       {190, 110, 50, 3, 3},
       {1480, 1550, 50, 3, 3},
       {2620, 1580, 50, 3, 3},
       {60, 30, 50, 3, 3},
       {90, 45, 50, 3, 3},
       {150, 75, 50, 3, 3},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {0, 0, 50, 0, 0},
       {0, 0, 50, 0, 0},
       {60, 30, 50, 0, 0},
       {60, 30, 50, 0, 0}}}},
    {"KY",
     29,
     1,
     1,
     false,
     {{{270, 135, 50, 0, 0},
       {190, 0, 100, 0, 0},
       {1480, 0, 100, 0, 0},
       {2620, 0, 100, 0, 0},
       {60, 0, 100, 0, 0},
       {90, 0, 100, 0, 0},
       {150, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {50.75, 0, 100, 0, 0},
       {50.75, 0, 100, 0, 0},
       {29.75, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {0, 0, 50, 0, 0},
       {0, 0, 50, 0, 0},
       {60, 30, 50, 0, 0},
       {60, 30, 50, 0, 0}}}},
    {"KZ",
     23,
     4,
     4,
     false,
     {{{270, 135, 50, 0, 0},
       {190, 110, 50, 3, 3},
       {1480, 1550, 50, 3, 3},
       {2620, 1580, 50, 3, 3},
       {60, 30, 50, 3, 3},
       {90, 45, 50, 3, 3},
       {150, 75, 50, 3, 3},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {40.25, 0, 100, 0, 0},
       {40.25, 0, 100, 0, 0},
       {19.25, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {0, 0, 50, 0, 0},
       {0, 0, 50, 0, 0},
       {60, 30, 50, 0, 0},
       {60, 30, 50, 0, 0}}}},
    {"B",
     26,
     12,
     12,
     false,
     {{{270, 135, 50, 0, 0},
       {190, 110, 50, 2, 2},
       {760, 350, 50, 2, 2},
       {2500, 0, 100, 0, 2},
       {60, 30, 50, 2, 2},
       {90, 45, 50, 2, 2},
       {150, 0, 100, 0, 2},
       {-30, 0, 100, 0, 0},
       {24.5, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {62, 31, 50, 0, 0},
       {62, 31, 50, 0, 0},
       {0, 0, 50, 0, 0},
       {0, 0, 50, 0, 0}}}},
    {"BY",
     29,
     1,
     1,
     false,
     {{{270, 135, 50, 0, 0},
       {190, 0, 100, 0, 0},
       {760, 0, 100, 0, 0},
       {2500, 0, 100, 0, 0},
       {60, 0, 100, 0, 0},
       {90, 0, 100, 0, 0},
       {150, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {24.5, 0, 100, 0, 0},
       {49, 0, 100, 0, 0},
       {43.25, 0, 100, 0, 0},
       {38.5, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {62, 31, 50, 0, 0},
       {62, 31, 50, 0, 0},
       {0, 0, 50, 0, 0},
       {0, 0, 50, 0, 0}}}},
    {"BZ",
     26,
     0,
     0,
     false,
     {{{270, 135, 50, 0, 0},
       {190, 110, 50, 2, 0},
       {760, 350, 50, 2, 0},
       {2500, 0, 100, 0, 0},
       {60, 30, 50, 2, 0},
       {90, 45, 50, 2, 0},
       {150, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {62, 31, 50, 0, 0},
       {62, 31, 50, 0, 0},
       {0, 0, 50, 0, 0},
       {0, 0, 50, 0, 0}}}},
    {"D",
     26,
     8,
     8,
     false,
     {{{270, 135, 50, 0, 0},
       {190, 110, 50, 2, 2},
       {1780, 950, 50, 2, 2},
       {2680, 2680, 0, 2, 2},
       {60, 30, 50, 2, 2},
       {90, 45, 50, 2, 2},
       {150, 150, 0, 2, 2},
       {-30, 0, 100, 0, 0},
       {31.5, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {62, 31, 50, 0, 0},
       {62, 31, 50, 0, 0},
       {0, 0, 50, 0, 0},
       {0, 0, 50, 0, 0}}}},
    {"DY",
     29,
     1,
     1,
     false,
     {{{270, 135, 50, 0, 0},
       {190, 0, 100, 0, 0},
       {1780, 0, 100, 0, 0},
       {2680, 0, 100, 0, 0},
       {60, 0, 100, 0, 0},
       {90, 0, 100, 0, 0},
       {150, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {38.5, 0, 100, 0, 0},
       {38.5, 0, 100, 0, 0},
       {35, 0, 100, 0, 0},
       {45.5, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {62, 31, 50, 0, 0},
       {62, 31, 50, 0, 0},
       {0, 0, 50, 0, 0},
       {0, 0, 50, 0, 0}}}},
    {"DZ",
     26,
     1,
     1,
     false,
     {{{270, 135, 50, 0, 0},
       {190, 110, 50, 2, 0},
       {1780, 950, 50, 2, 0},
       {2680, 2680, 0, 2, 0},
       {60, 30, 50, 2, 0},
       {90, 45, 50, 2, 0},
       {150, 150, 0, 2, 0},
       {-30, 0, 100, 0, 0},
       {38.5, 0, 100, 0, 0},
       {28, 0, 100, 0, 0},
       {24.5, 0, 100, 0, 0},
       {35, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {62, 31, 50, 0, 0},
       {62, 31, 50, 0, 0},
       {0, 0, 50, 0, 0},
       {0, 0, 50, 0, 0}}}},
    {"G",
     26,
     12,
     12,
     false,
     {{{270, 135, 50, 0, 0},
       {190, 110, 50, 3, 3},
       {1480, 1550, 50, 3, 3},
       {2620, 1580, 50, 3, 3},
       {60, 30, 50, 3, 3},
       {90, 45, 50, 3, 3},
       {150, 75, 50, 3, 3},
       {-30, 0, 100, 0, 0},
       {35, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {62, 31, 50, 0, 0},
       {62, 31, 50, 0, 0},
       {0, 0, 50, 0, 0},
       {0, 0, 50, 0, 0}}}},
    {"GY",
     29,
     1,
     1,
     false,
     {{{270, 135, 50, 0, 0},
       {190, 0, 100, 0, 0},
       {1480, 0, 100, 0, 0},
       {2620, 0, 100, 0, 0},
       {60, 0, 100, 0, 0},
       {90, 0, 100, 0, 0},
       {150, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {35, 0, 100, 0, 0},
       {45.5, 0, 100, 0, 0},
       {40.25, 0, 100, 0, 0},
       {24.5, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {62, 31, 50, 0, 0},
       {62, 31, 50, 0, 0},
       {0, 0, 50, 0, 0},
       {0, 0, 50, 0, 0}}}},
    {"GZ",
     26,
     2,
     2,
     false,
     {{{270, 135, 50, 0, 0},
       {190, 110, 50, 3, 2},
       {1480, 1550, 50, 3, 2},
       {2620, 1580, 50, 3, 2},
       {60, 30, 50, 3, 2},
       {90, 45, 50, 3, 2},
       {150, 75, 50, 3, 2},
       {-30, 0, 100, 0, 0},
       {35, 0, 100, 0, 0},
       {35, 0, 100, 0, 0},
       {29.75, 0, 100, 0, 0},
       {14, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {62, 31, 50, 0, 0},
       {62, 31, 50, 0, 0},
       {0, 0, 50, 0, 0},
       {0, 0, 50, 0, 0}}}},
    {"M",
     15,
     8,
     8,
     false,
     {{{360, 360, 0, 3, 0},
       {480, 480, 0, 3, 0},
       {1000, 350, 50, 3, 0},
       {2200, 0, 100, 5, 0},
       {40, 20, 50, 3, 0},
       {175, 87, 50, 3, 0},
       {120, 0, 100, 5, 0},
       {42, 21, 50, 3, 0},
       {26, -10, 100, 3, 0},
       {30, -10, 100, 3, 0},
       {33, -10, 100, 3, 0},
       {-30, -10, 100, 3, 0},
       {-30, 0, 100, 3, 0},
       {-30, 0, 100, 3, 0},
       {-30, 0, 100, 3, 0},
       {62, 31, 50, 2, 0},
       {62, 31, 50, 2, 0},
       {0, 0, 50, 2, 0},
       {0, 0, 50, 2, 0}}}},
    {"N",
     15,
     8,
     8,
     false,
     {{{450, 450, 0, 3, 0},
       {480, 480, 0, 3, 0},
       {1780, 950, 50, 3, 3},
       {2620, 2680, 0, 3, 0},
       {40, 20, 50, 3, 0},
       {300, 150, 50, 3, 3},
       {260, 130, 50, 3, 0},
       {42, 21, 50, 3, 0},
       {35, -10, 100, 3, 0},
       {35, -10, 100, 3, 0},
       {35, -10, 100, 3, 0},
       {20, -10, 100, 3, 0},
       {-30, 0, 100, 3, 0},
       {-30, 0, 100, 3, 0},
       {-30, 0, 100, 3, 0},
       {62, 31, 50, 2, 0},
       {62, 31, 50, 2, 0},
       {0, 0, 50, 2, 0},
       {0, 0, 50, 2, 0}}}},
    {"NG",
     15,
     8,
     8,
     false,
     {{{360, 360, 0, 3, 0},
       {480, 480, 0, 3, 0},
       {820, 1550, 50, 5, 3},
       {2800, 1580, 50, 3, 3},
       {160, 80, 0, 5, 0},
       {150, 75, 50, 5, 3},
       {100, 50, 50, 3, 0},
       {42, 21, 50, 3, 3},
       {20, 0, 100, 3, 0},
       {30, 0, 100, 3, 0},
       {35, 0, 100, 3, 0},
       {0, 0, 100, 3, 0},
       {-30, 0, 100, 3, 0},
       {-30, 0, 100, 3, 0},
       {-30, 0, 100, 3, 0},
       {52, 26, 50, 2, 0},
       {56, 28, 50, 2, 0},
       {0, 0, 50, 2, 0},
       {0, 0, 50, 2, 0}}}},
    {"F",
     18,
     12,
     12,
     false,
     {{{270, 135, 50, 0, 0},
       {400, 170, 50, 3, 2},
       {1420, 350, 50, 3, 2},
       {2560, 980, 50, 3, 2},
       {60, 30, 50, 3, 2},
       {90, 45, 50, 3, 2},
       {150, 75, 50, 3, 2},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {0, 0, 100, 0, 0},
       {0, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {54, 27, 50, 0, 0},
       {0, 0, 50, 0, 0},
       {0, 0, 50, 0, 0},
       {32, 16, 50, 0, 0},
       {54, 30, 50, 0, 0}}}},
    {"S",
     18,
     12,
     12,
     false,
     {{{270, 135, 50, 0, 0},
       {400, 170, 50, 3, 2},
       {1720, 950, 50, 3, 2},
       {2620, 0, 100, 3, 2},
       {200, 100, 50, 3, 2},
       {96, 48, 50, 3, 2},
       {220, 0, 100, 3, 2},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {28, 0, 100, 0, 0},
       {28, 0, 100, 0, 0},
       {40.25, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {0, 0, 50, 0, 0},
       {0, 0, 50, 0, 0},
       {32, 16, 50, 0, 0},
       {60, 30, 50, 0, 0}}}},
    {"SH",
     18,
     12,
     12,
     false,
     {{{270, 135, 50, 0, 0},
       {400, 170, 50, 3, 2},
       {2200, 1190, 50, 3, 2},
       {2560, 0, 100, 3, 2},
       {60, 30, 50, 3, 2},
       {90, 45, 50, 3, 2},
       {150, 0, 100, 3, 2},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {31.5, 0, 100, 0, 0},
       {42, 0, 100, 0, 0},
       {31.5, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {0, 0, 50, 0, 0},
       {0, 0, 50, 0, 0},
       {60, 30, 50, 0, 0},
       {60, 30, 50, 0, 0}}}},
    {"X",
     18,
     12,
     12,
     false,
     {{{270, 135, 50, 0, 0},
       {190, 110, 50, 3, 3},
       {1480, 1550, 50, 3, 3},
       {2620, 1580, 50, 3, 3},
       {60, 30, 50, 3, 3},
       {90, 45, 50, 3, 3},
       {150, 75, 50, 3, 3},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {40.25, 0, 100, 0, 0},
       {40.25, 0, 100, 0, 0},
       {19.25, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {0, 0, 50, 0, 0},
       {0, 0, 50, 0, 0},
       {60, 30, 50, 0, 0},
       {60, 30, 50, 0, 0}}}},
    {"H",
     9,
     10,
     10,
     false,
     {{{270, 135, 50, 0, 0},
       {490, 0, 100, 0, 7},
       {1480, 0, 100, 0, 7},
       {2500, 0, 100, 0, 7},
       {60, 0, 100, 0, 7},
       {90, 0, 100, 0, 7},
       {150, 0, 100, 0, 7},
       {-30, 0, 100, 0, 7},
       {35, -14, 100, 0, 7},
       {36.75, -14, 100, 0, 7},
       {26.25, -7, 100, 0, 7},
       {22.75, -3.5, 100, 0, 7},
       {-30, 0, 100, 0, 7},
       {-30, 0, 100, 0, 7},
       {-30, 0, 100, 0, 7},
       {0, 0, 50, 0, 0},
       {0, 0, 50, 0, 0},
       {60, 30, 50, 0, 0},
       {60, 30, 50, 0, 0}}}},
    {"V",
     20,
     4,
     4,
     false,
     {{{270, 135, 50, 0, 0},
       {280, 170, 50, 3, 2},
       {1420, 350, 50, 3, 2},
       {2560, 980, 50, 3, 2},
       {60, 30, 50, 3, 2},
       {90, 45, 50, 3, 2},
       {150, 75, 50, 3, 2},
       {-30, 0, 100, 0, 0},
       {29.75, 0, 100, 0, 0},
       {40.25, 0, 100, 0, 0},
       {36.75, 0, 100, 0, 0},
       {33.25, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {62, 31, 50, 0, 0},
       {62, 31, 50, 0, 0},
       {0, 0, 50, 0, 0},
       {0, 0, 50, 0, 0}}}},
    {"QQ",
     30,
     0,
     0,
     false,
     {{{270, 135, 50, 0, 0},
       {280, 0, 100, 0, 0},
       {1420, 0, 100, 0, 0},
       {2560, 0, 100, 0, 0},
       {60, 0, 100, 0, 0},
       {90, 0, 100, 0, 0},
       {150, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {29.75, 0, 100, 0, 0},
       {40.25, 0, 100, 0, 0},
       {36.75, 0, 100, 0, 0},
       {33.25, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {62, 31, 50, 0, 0},
       {62, 31, 50, 0, 0},
       {0, 0, 50, 0, 0},
       {0, 0, 50, 0, 0}}}},
    {"L",
     11,
     8,
     8,
     false,
     {{{270, 135, 50, 0, 0},
       {460, 230, 50, 6, 0},
       {1480, 710, 50, 6, 0},
       {2500, 1220, 50, 6, 0},
       {60, 30, 50, 6, 0},
       {90, 45, 50, 6, 0},
       {150, 75, 50, 6, 0},
       {-30, 0, 100, 0, 0},
       {36.75, 0, 100, 0, 0},
       {26.25, 0, 100, 0, 0},
       {26.25, 0, 100, 0, 0},
       {21, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {-30, 0, 100, 0, 0},
       {62, 31, 50, 0, 0},
       {62, 31, 50, 0, 0},
       {0, 0, 50, 0, 0},
       {0, 0, 50, 0, 0}}}},
    {"R",
     10,
     11,
     11,
     false,
     {{{270, 135, 50, 0, 0},
       {490, 0, 100, 0, 5},
       {1180, 590, 50, 5, 5},
       {1600, 740, 50, 5, 5},
       {60, 0, 100, 0, 5},
       {90, 45, 50, 5, 5},
       {150, 75, 50, 5, 5},
       {-30, 0, 100, 5, 5},
       {42, 21, 50, 5, 5},
       {35, 17.5, 50, 5, 5},
       {35, 17.5, 50, 5, 5},
       {-30, 0, 50, 5, 5},
       {-30, -15, 50, 5, 5},
       {-30, -15, 50, 5, 5},
       {-30, -15, 50, 5, 5},
       {62, 31, 50, 0, 0},
       {62, 31, 50, 0, 0},
       {0, 0, 50, 0, 0},
       {0, 0, 50, 0, 0}}}},
    {"Y",
     10,
     7,
     7,
     false,
     {{{270, 135, 50, 0, 0},
       {250, 110, 50, 4, 4},
       {2500, 1190, 50, 4, 4},
       {2980, 1460, 50, 4, 4},
       {60, 30, 50, 4, 4},
       {90, 45, 50, 4, 4},
       {150, 75, 50, 4, 4},
       {-30, 0, 100, 4, 4},
       {50.75, 24.5, 50, 4, 4},
       {33.25, 17.5, 50, 4, 4},
       {38.5, 17.5, 50, 4, 4},
       {31.5, 14, 50, 4, 4},
       {-30, -15, 50, 4, 4},
       {-30, -15, 50, 4, 4},
       {-30, -15, 50, 4, 4},
       {62, 31, 50, 0, 0},
       {16, 8, 50, 0, 0},
       {0, 0, 50, 0, 0},
       {0, 0, 50, 0, 0}}}},
    {"EE",
     2,
     11,
     7,
     true,
     {{{270, 135, 50, 0, 0},
       {250, 110, 50, 4, 4},
       {2320, 1190, 50, 4, 4},
       {3200, 1580, 50, 4, 4},
       {60, 30, 50, 4, 4},
       {90, 45, 50, 4, 4},
       {150, 75, 50, 4, 4},
       {-30, 0, 100, 4, 4},
       {50.75, 24.5, 50, 4, 4},
       {33.25, 17.5, 50, 4, 4},
       {36.75, 17.5, 50, 4, 4},
       {31.5, 14, 50, 4, 4},
       {-30, -15, 50, 4, 4},
       {-30, -15, 50, 4, 4},
       {-30, -15, 50, 4, 4},
       {62, 31, 50, 0, 0},
       {16, 8, 50, 0, 0},
       {0, 0, 50, 0, 0},
       {0, 0, 50, 0, 0}}}},
    {"I",
     2,
     8,
     6,
     true,
     {{{270, 135, 50, 0, 0},
       {400, 170, 50, 4, 4},
       {2080, 1070, 50, 4, 4},
       {2560, 1340, 50, 4, 4},
       {60, 30, 50, 4, 4},
       {90, 45, 50, 4, 4},
       {150, 75, 50, 4, 4},
       {-30, 0, 100, 4, 4},
       {50.75, 24.5, 50, 4, 4},
       {36.75, 17.5, 50, 4, 4},
       {35, 17.5, 50, 4, 4},
       {29.75, 14, 50, 4, 4},
       {-30, -15, 50, 4, 4},
       {-30, -15, 50, 4, 4},
       {-30, -15, 50, 4, 4},
       {62, 31, 50, 0, 0},
       {16, 8, 50, 0, 0},
       {0, 0, 50, 0, 0},
       {0, 0, 50, 0, 0}}}},
    {"E",
     2,
     8,
     4,
     true,
     {{{270, 135, 50, 0, 0},
       {640, 350, 50, 4, 4},
       {2020, 1070, 50, 4, 4},
       {2500, 1220, 50, 4, 4},
       {60, 30, 50, 4, 4},
       {90, 45, 50, 4, 4},
       {150, 75, 50, 4, 4},
       {-30, 0, 100, 4, 4},
       {50.75, 24.5, 50, 4, 4},
       {42, 21, 50, 4, 4},
       {38.5, 17.5, 50, 4, 4},
       {31.5, 14, 50, 4, 4},
       {-30, -15, 50, 4, 4},
       {-30, -15, 50, 4, 4},
       {-30, -15, 50, 4, 4},
       {62, 31, 50, 0, 0},
       {16, 8, 50, 0, 0},
       {0, 0, 50, 0, 0},
       {0, 0, 50, 0, 0}}}},
    {"AA",
     2,
     10,
     5,
     true,
     {{{270, 135, 50, 0, 0},
       {790, 410, 50, 4, 4},
       {1780, 950, 50, 4, 4},
       {2500, 1220, 50, 4, 4},
       {130, 65, 50, 4, 4},
       {90, 45, 50, 4, 4},
       {150, 75, 50, 4, 4},
       {-30, 0, 100, 4, 4},
       {50.75, 24.5, 50, 4, 4},
       {47.25, 24.5, 50, 4, 4},
       {38.5, 17.5, 50, 4, 4},
       {31.5, 14, 50, 4, 4},
       {-30, -15, 50, 4, 4},
       {-30, -15, 50, 4, 4},
       {-30, -15, 50, 4, 4},
       {62, 31, 50, 0, 0},
       {16, 8, 50, 0, 0},
       {0, 0, 50, 0, 0},
       {0, 0, 50, 0, 0}}}},
    {"AR",
     2,
     15,
     15,
     true,
     {{{270, 135, 50, 0, 0},
       {790, 410, 50, 4, 4},
       {880, 470, 50, 4, 4},
       {2500, 1220, 50, 4, 4},
       {60, 30, 50, 4, 4},
       {90, 45, 50, 4, 4},
       {150, 75, 50, 4, 4},
       {-30, 0, 100, 4, 4},
       {50.75, 24.5, 50, 4, 4},
       {49, 24.5, 50, 4, 4},
       {29.75, 14, 50, 4, 4},
       {22.75, 10.5, 50, 4, 4},
       {-30, -15, 50, 4, 4},
       {-30, -15, 50, 4, 4},
       {-30, -15, 50, 4, 4},
       {62, 31, 50, 0, 0},
       {16, 8, 50, 0, 0},
       {0, 0, 50, 0, 0},
       {0, 0, 50, 0, 0}}}},
    {"UU",
     2,
     14,
     9,
     true,
     {{{270, 135, 50, 0, 0},
       {250, 110, 50, 4, 4},
       {880, 470, 50, 4, 4},
       {2200, 1100, 50, 4, 4},
       {60, 30, 50, 4, 4},
       {90, 45, 50, 4, 4},
       {150, 75, 50, 4, 4},
       {-30, 0, 100, 4, 4},
       {50.75, 24.5, 50, 4, 4},
       {38.5, 17.5, 50, 4, 4},
       {17.5, 7, 50, 4, 4},
       {10.5, 3.5, 50, 4, 4},
       {-30, -15, 50, 4, 4},
       {-30, -15, 50, 4, 4},
       {-30, -15, 50, 4, 4},
       {62, 31, 50, 0, 0},
       {16, 8, 50, 0, 0},
       {0, 0, 50, 0, 0},
       {0, 0, 50, 0, 0}}}},
    {"OO",
     2,
     6,
     4,
     true,
     {{{270, 135, 50, 0, 0},
       {370, 170, 50, 4, 4},
       {1000, 470, 50, 4, 4},
       {2500, 1220, 50, 4, 4},
       {60, 30, 50, 4, 4},
       {90, 45, 50, 4, 4},
       {150, 75, 50, 4, 4},
       {-30, 0, 100, 4, 4},
       {50.75, 24.5, 50, 4, 4},
       {42, 21, 50, 4, 4},
       {28, 14, 50, 4, 4},
       {22.75, 10.5, 50, 4, 4},
       {-30, -15, 50, 4, 4},
       {-30, -15, 50, 4, 4},
       {-30, -15, 50, 4, 4},
       {62, 31, 50, 0, 0},
       {16, 8, 50, 0, 0},
       {0, 0, 50, 0, 0},
       {0, 0, 50, 0, 0}}}},
    {"O",
     2,
     9,
     6,
     true,
     {{{270, 135, 50, 0, 0},
       {610, 290, 50, 4, 4},
       {880, 470, 50, 4, 4},
       {2500, 1220, 50, 4, 4},
       {60, 30, 50, 4, 4},
       {90, 45, 50, 4, 4},
       {150, 75, 50, 4, 4},
       {-30, 0, 100, 4, 4},
       {50.75, 24.5, 50, 4, 4},
       {47.25, 24.5, 50, 4, 4},
       {22.75, 10.5, 50, 4, 4},
       {15.75, 7, 50, 4, 4},
       {-30, -15, 50, 4, 4},
       {-30, -15, 50, 4, 4},
       {-30, -15, 50, 4, 4},
       {62, 31, 50, 0, 0},
       {16, 8, 50, 0, 0},
       {0, 0, 50, 0, 0},
       {0, 0, 50, 0, 0}}}},
    {"A",
     2,
     4,
     4,
     true,
     {{{270, 135, 50, 0, 0},
       {490, 230, 50, 4, 4},
       {1480, 710, 50, 4, 4},
       {2500, 1220, 50, 4, 4},
       {60, 30, 50, 4, 4},
       {90, 45, 50, 4, 4},
       {150, 75, 50, 4, 4},
       {-30, 0, 100, 4, 4},
       {50.75, 24.5, 50, 4, 4},
       {50.75, 24.5, 50, 4, 4},
       {33.25, 17.5, 50, 4, 4},
       {26.25, 14, 50, 4, 4},
       {-30, -15, 50, 4, 4},
       {-30, -15, 50, 4, 4},
       {-30, -15, 50, 4, 4},
       {62, 31, 50, 0, 0},
       {16, 8, 50, 0, 0},
       {0, 0, 50, 0, 0},
       {0, 0, 50, 0, 0}}}},
};
enum Length { long_vowel, short_vowel, schwa };
struct Vowel {
    const char* ipa;
    const char* donor;
    float f1, f2, f3;
    Length length;
};
// Swedish vowel qualities (male, approximate formants in Hz) on rsynth vowel elements.
constexpr Vowel vowels[] = {
    {"iː", "EE", 255, 2190, 3150, long_vowel}, {"i", "EE", 255, 2190, 3150, short_vowel},
    {"ɪ", "I", 340, 2000, 2650, short_vowel},  {"yː", "EE", 260, 1950, 2400, long_vowel},
    {"y", "EE", 260, 1950, 2400, short_vowel}, {"ʏ", "I", 330, 1800, 2300, short_vowel},
    {"ʉː", "EE", 290, 1650, 2150, long_vowel}, {"ɨː", "EE", 290, 1650, 2150, long_vowel},
    {"ɵ", "OO", 390, 1300, 2250, short_vowel}, {"eː", "EE", 345, 2200, 2800, long_vowel},
    {"e", "E", 480, 1800, 2500, short_vowel},  {"ɛː", "E", 490, 1900, 2550, long_vowel},
    {"ɛ", "E", 500, 1750, 2500, short_vowel},  {"øː", "EE", 360, 1650, 2200, long_vowel},
    {"ø", "EE", 360, 1650, 2200, short_vowel}, {"œ", "E", 520, 1350, 2300, short_vowel},
    {"œː", "E", 520, 1350, 2300, long_vowel},  {"æ", "AA", 650, 1650, 2450, short_vowel},
    {"æː", "AA", 650, 1650, 2450, long_vowel}, {"a", "AA", 650, 1350, 2450, short_vowel},
    {"ɑː", "AR", 600, 950, 2550, long_vowel},  {"ɑ", "AR", 600, 950, 2550, short_vowel},
    {"oː", "UU", 390, 720, 2400, long_vowel},  {"o", "UU", 390, 720, 2400, short_vowel},
    {"ɔ", "O", 450, 800, 2450, short_vowel},   {"uː", "UU", 300, 620, 2250, long_vowel},
    {"u", "UU", 300, 620, 2250, short_vowel},  {"ʊ", "OO", 360, 750, 2300, short_vowel},
    {"ə", "E", 460, 1650, 2500, schwa},
};
struct Consonant {
    const char* ipa;
    std::array<const char*, 3> parts;
    // Björn's average duration in ms, and after a short stressed vowel (Swedish long consonant).
    float ms, long_ms;
};
// Swedish-only sounds use elements derived in Inventory: retroflex "r…", "tj" and the tapped "tap".
// Durations are Kokoro's predictions for Björn over 403 corpus verses; ɧ and ɭ, which Kokoro
// merges with ʂ and l, take theirs, and ʧ (a few loanwords) is estimated.
constexpr Consonant consonants[] = {
    {"p", {"P", "PY", "PZ"}, 46, 70},
    {"t", {"T", "TY", "TZ"}, 48, 57},
    {"k", {"K", "KY", "KZ"}, 53, 70},
    {"b", {"B", "BY", "BZ"}, 47, 65},
    {"d", {"D", "DY", "DZ"}, 41, 54},
    {"ɡ", {"G", "GY", "GZ"}, 44, 64},
    {"ʈ", {"rT", "TY", "rTZ"}, 60, 76},
    {"ɖ", {"rD", "DY", "rDZ"}, 67, 85},
    {"m", {"M"}, 39, 54},
    {"n", {"N"}, 45, 62},
    {"ɳ", {"rN"}, 71, 90},
    {"ŋ", {"NG"}, 63, 82},
    {"f", {"F"}, 49, 75},
    {"v", {"V", "QQ", "V"}, 39, 50},
    {"s", {"S"}, 59, 81},
    {"ʂ", {"rSH"}, 64, 88},
    {"ɕ", {"tj"}, 53, 70},
    {"ɧ", {"X"}, 64, 88},
    {"h", {"H"}, 40, 40},
    {"l", {"L"}, 49, 57},
    {"ɭ", {"rL"}, 49, 57},
    {"r", {"tap"}, 42, 75},
    {"j", {"Y"}, 42, 42},
    {"ʧ", {"T", "TY", "SH"}, 80, 100},
};
constexpr float vowel_breathiness = 16;
void set(KlattElement& element, int parameter, float steady, float fixed) {
    element.targets[parameter].steady = steady;
    element.targets[parameter].fixed = fixed;
}
// rsynth's elements plus Cesar's Swedish ones, built once.
class Inventory {
    std::map<std::string, KlattElement, std::less<>> elements_;

public:
    Inventory() {
        for (const auto& element : rsynth)
            elements_.emplace(element.name, element);
        // rsynth was made for 11 kHz audio and never uses formants 5 and 6 (4.4 and 6.2 kHz here).
        // An s hisses there, and t and d bursts are sharp there; without them s sounds like sh.
        auto& s = elements_.at("S");
        set(s, klatt_a2, -30, 0);
        set(s, klatt_a3, 20, 0);
        set(s, klatt_a4, 34, 0);
        set(s, klatt_a5, 50, 0);
        set(s, klatt_a6, 60, 0);
        for (const char* burst : {"TY", "DY"}) {
            set(elements_.at(burst), klatt_a5, 48, 0);
            set(elements_.at(burst), klatt_a6, 48, 0);
        }
        // A Swedish h is clearer than rsynth's (fewer misheard words in the intelligibility check).
        auto& h = elements_.at("H");
        for (int a = klatt_a1; a <= klatt_a4; ++a)
            h.targets[a].steady += 8;
        h.targets[klatt_asp].steady += 8;
        // sh-type frication was measured 5 dB strong around 2-3 kHz.
        for (const char* hush : {"SH", "X"})
            elements_.at(hush).targets[klatt_a3].steady -= 5;
        // Voicing that stops between two parameter updates clicks; fade it over 20 ms instead.
        for (auto& [name, element] : elements_)
            if (!element.vowel && element.targets[klatt_av].steady == 0) {
                element.targets[klatt_av].internal = std::max(element.targets[klatt_av].internal, 2);
                element.targets[klatt_av].external = std::max(element.targets[klatt_av].external, 2);
            }
        // rsynth's nasals and l sit 15-27 dB under the vowels; natural speech has 6-12 dB.
        for (const char* sonorant : {"M", "N", "NG", "L"})
            for (int a = klatt_a1; a <= klatt_a4; ++a)
                elements_.at(sonorant).targets[a].steady += sonorant[0] == 'M' ? 12 : 6;
        for (const auto& vowel : vowels) {
            auto element = at(vowel.donor);
            element.name = vowel.ipa;
            const float formants[] = {vowel.f1, vowel.f2, vowel.f3};
            // The donors' consonant loci lie at half the vowel's formant.
            for (int k = 0; k < 3; ++k)
                set(element, klatt_f1 + k, formants[k], formants[k] / 2);
            // Breath noise during the open phase fills the gaps between glottal pulses; rsynth's
            // near-zero value leaves each pulse as a separate high-frequency snap.
            set(element, klatt_avc, vowel_breathiness, vowel_breathiness / 2);
            // rsynth leaves the upper formants of vowels silent; at 24 kHz they carry presence.
            set(element, klatt_a5, element.targets[klatt_a4].steady - 4, element.targets[klatt_a4].fixed);
            set(element, klatt_a6, element.targets[klatt_a4].steady - 12, element.targets[klatt_a4].fixed);
            elements_.insert_or_assign(vowel.ipa, element);
        }
        // Retroflex ʈ ɖ ɳ ɭ ʂ: the alveolars with the F3 locus lowered to about 1900 Hz, which also
        // colours the neighbouring vowels over ~40 ms, and F2 slightly lower.
        for (const char* name : {"T", "TZ", "D", "DZ", "N", "L", "SH"}) {
            auto element = at(name);
            set(element, klatt_f2, 1600, element.targets[klatt_f2].fixed * 0.9f);
            // A boundary is fixed + proportion% of the neighbour; aim at 1900 Hz beside a 2500 Hz F3.
            const float share = element.targets[klatt_f3].proportion / 100.0f;
            set(element, klatt_f3, 1900, share >= 1 ? 0 : 1900 - share * 2500);
            if (share < 1)
                element.targets[klatt_f3].external = 4;
            element.name = "retroflex";
            elements_.insert_or_assign(std::string("r") + name, element);
        }
        // ɕ (tj): brighter and fronter than ʂ, with frication weighted towards F4 and F5.
        auto tj = at("SH");
        set(tj, klatt_f2, 2400, 1300);
        set(tj, klatt_f3, 3000, 0);
        set(tj, klatt_a3, 28, 0);
        set(tj, klatt_a4, 42, 0);
        set(tj, klatt_a5, 52, 0);
        set(tj, klatt_a6, 42, 0);
        elements_.insert_or_assign("tj", tj);
        // r: a 30 ms voiced tap, a brief shallow dip in an l, as in Swedish; rsynth's approximant
        // sounds American.
        auto tap = at("L");
        set(tap, klatt_f1, 300, 150);
        set(tap, klatt_f3, 2500, 1250);
        for (int a = klatt_a1; a <= klatt_a4; ++a)
            set(tap, a, tap.targets[a].steady - 2, tap.targets[a].fixed);
        // l's 60 ms transitions would smear the tap into a long dip.
        for (auto& target : tap.targets) {
            target.external = std::min(target.external, 1);
            target.internal = std::min(target.internal, 1);
        }
        elements_.insert_or_assign("tap", tap);
        // A Swedish l is clear (high F2), not the dark English l of rsynth.
        set(elements_.at("L"), klatt_f2, 1800, 900);
        // The branch gains are retuned for vowels; consonants keep the spectra they had at rsynth's.
        for (auto& [name, element] : elements_)
            if (!element.vowel)
                for (int i = 0; i < 6; ++i)
                    element.targets[klatt_a1 + i].steady -=
                        20 * std::log10(klatt_branch_gains[i] / rsynth_branch_gains[i]);
    }
    const KlattElement& at(std::string_view name) const {
        const auto found = elements_.find(name);
        if (found == elements_.end())
            throw std::logic_error("Cesar has no sound element " + std::string(name));
        return found->second;
    }
};
const Inventory& inventory() {
    static const Inventory instance;
    return instance;
}
// Timing in ms from Björn (Kokoro's predictions over 403 corpus verses). Vowels by length, stress
// (none, secondary, primary) and whether more syllables follow in the word: "herde" draws out
// its stressed e, a stressed one-syllable "min" does not.
constexpr float vowel_ms_table[3][3][2] = {
    // {more syllables follow, last syllable of the word}
    {{95, 69}, {134, 138}, {142, 97}}, // long
    {{62, 57}, {112, 122}, {115, 86}}, // short
    {{61, 50}, {61, 50}, {61, 50}},    // schwa
};
constexpr float comma_pause = 100, sentence_pause = 470, word_end = 25;
// The last syllable before a pause is drawn out (Kokoro's figures include the pause itself).
constexpr float phrase_final = 1.4f;
// Björn's pace felt rushed for Cesar; read about 12% slower.
constexpr float tempo = 1.12f;
// Melody (male voice, Hz).
constexpr float phrase_top = 128, phrase_bottom = 104, lowest = 85;
// Swedish word accents: accent 1 peaks early in the stressed syllable; accent 2 (a stressed
// syllable with more to follow in the word, as in "herde") falls through it and rises again on
// the next syllable, the two-peaked melody Swedish is known for.
constexpr float primary_accent = 26, accent2_rise = 18, secondary_accent = 10, accent_width = 15;
constexpr float sentence_fall = 18, continuation_rise = 8, question_rise = 30;
constexpr float level = 0.9f;
float vowel_ms(Length length, int stress, bool last_syllable) {
    return vowel_ms_table[length][stress == 3 ? 2 : stress == 2 ? 1 : 0][last_syllable ? 1 : 0];
}
// One UTF-8 symbol per phone, with a following length mark attached.
std::vector<std::string> split(const std::string& ipa) {
    std::vector<std::string> result;
    for (size_t at = 0; at < ipa.size();) {
        const auto lead = static_cast<unsigned char>(ipa[at]);
        const size_t size = lead < 0x80 ? 1 : lead < 0xe0 ? 2 : lead < 0xf0 ? 3 : 4;
        auto symbol = ipa.substr(at, size);
        at += size;
        if (symbol == "ː" && !result.empty())
            result.back() += symbol;
        else
            result.push_back(std::move(symbol));
    }
    return result;
}
bool silent(const std::string& symbol) {
    return symbol == " " || symbol == "ˈ" || symbol == "ˌ" || symbol == "-" ||
           (symbol.size() == 1 && std::strchr(",.!?;:", symbol[0]));
}
const Vowel* vowel(std::string_view symbol) {
    for (const auto& candidate : vowels)
        if (symbol == candidate.ipa)
            return &candidate;
    return nullptr;
}
const Consonant* consonant(std::string_view symbol) {
    for (const auto& candidate : consonants)
        if (symbol == candidate.ipa)
            return &candidate;
    return nullptr;
}
// One synthesized sound, or a pause (element Q) ending a phrase.
struct Unit {
    const KlattElement* element;
    float frames;
    int stress = 0;
    bool vowel = false;
    char pause = 0; // '.', ',', '?', or 'e' at the end of the text
    bool accent2 = false;
};
struct Phrase {
    float begin, end;
    char ending;
};
struct Accent {
    float center, height;
};
// Phones to sounds with Björn's timing; pauses keep their place.
std::vector<Unit> units(const std::string& ipa) {
    const auto& sounds = inventory();
    std::vector<Unit> result;
    int stress = 0;
    bool after_short_stressed = false;
    const auto symbols = split(ipa);
    // True when no other vowel follows before the word ends.
    const auto last_syllable = [&](size_t at) {
        for (size_t k = at + 1; k < symbols.size() && !silent(symbols[k]); ++k)
            if (vowel(symbols[k]))
                return false;
        return true;
    };
    for (size_t at = 0; at < symbols.size(); ++at) {
        const auto& symbol = symbols[at];
        if (symbol == "ˈ" || symbol == "ˌ") {
            stress = symbol == "ˈ" ? 3 : 2;
        } else if (symbol == " ") {
            if (!result.empty() && !result.back().pause)
                result.back().frames += word_end / 10;
        } else if (silent(symbol)) {
            const char kind = symbol == "?" ? '?' : symbol == "." || symbol == "!" ? '.' : ',';
            result.push_back(
                {&sounds.at("Q"), (kind == ',' ? comma_pause : sentence_pause) / 10, 0, false, kind});
        } else if (const auto* v = vowel(symbol)) {
            result.push_back({&sounds.at(v->ipa), vowel_ms(v->length, stress, last_syllable(at)) / 10, stress,
                              true, 0, stress == 3 && !last_syllable(at)});
            after_short_stressed = stress && v->length == short_vowel;
            stress = 0;
        } else if (const auto* c = consonant(symbol)) {
            // The parts (closure, burst, aspiration…) share the consonant's time as rsynth proportions them.
            float own = 0;
            for (const char* part : c->parts)
                if (part)
                    own += (sounds.at(part).stressed_frames + sounds.at(part).unstressed_frames) / 2.0f;
            const float total = (after_short_stressed ? c->long_ms : c->ms) / 10;
            for (const char* part : c->parts)
                if (part) {
                    const auto& element = sounds.at(part);
                    const float share = (element.stressed_frames + element.unstressed_frames) / 2.0f;
                    result.push_back({&element, own > 0 ? total * share / own : 0});
                }
            after_short_stressed = false;
        }
    }
    return result;
}
} // namespace
std::vector<float> cesar_speech(const std::string& ipa) {
    auto sounds = units(ipa);
    // Text without speakable phones reads as a short pause, as with the neural voices.
    if (std::none_of(sounds.begin(), sounds.end(), [](const Unit& unit) {
            return !unit.pause;
        }))
        return std::vector<float>(klatt_sample_rate / 5, 0.0f);
    sounds.push_back({&inventory().at("Q"), 10, 0, false, 'e'});
    for (auto& unit : sounds)
        unit.frames *= tempo;
    // Draw out the last vowel before each pause and what follows it.
    for (size_t i = 0; i < sounds.size(); ++i)
        if (sounds[i].pause) {
            size_t from = i;
            while (from > 0 && !sounds[from - 1].pause && !sounds[from].vowel)
                --from;
            for (size_t k = from; k < i; ++k)
                sounds[k].frames *= phrase_final;
        }
    std::vector<KlattSegment> segments;
    std::vector<Phrase> phrases;
    std::vector<Accent> accents;
    float now = 0, phrase_begin = 0;
    bool accent2_pending = false;
    for (const auto& unit : sounds) {
        if (unit.pause) {
            if (now > phrase_begin)
                phrases.push_back({phrase_begin, now, unit.pause});
        } else if (unit.vowel && unit.stress == 3) {
            accents.push_back({now + unit.frames * 0.1f, primary_accent});
        } else if (unit.vowel && unit.stress == 2) {
            accents.push_back({now + unit.frames * 0.3f, secondary_accent});
        } else if (unit.vowel && accent2_pending) {
            accents.push_back({now + unit.frames * 0.5f, accent2_rise});
        }
        if (unit.vowel)
            accent2_pending = unit.accent2;
        segments.push_back({unit.element, unit.frames});
        now += unit.frames;
        if (unit.pause)
            phrase_begin = now;
    }
    // Each phrase starts high and declines; stressed syllables add a peak; the phrase ends with a
    // fall (sentence), a slight rise (clause) or a clear rise (question).
    const auto pitch = [&](float t) {
        const auto phrase = std::find_if(phrases.begin(), phrases.end(), [t](const Phrase& p) {
            return t < p.end;
        });
        if (phrase == phrases.end())
            return phrase_bottom;
        const float length = std::max(1.0f, phrase->end - phrase->begin);
        const float progress = std::clamp((t - phrase->begin) / length, 0.0f, 1.0f);
        float f0 = phrase_top - (phrase_top - phrase_bottom) * progress;
        float peak = 0;
        for (const auto& accent : accents)
            peak = std::max(peak,
                            accent.height * std::max(0.0f, 1 - std::abs(t - accent.center) / accent_width));
        f0 += peak;
        const float to_end = phrase->end - t;
        const float ending = phrase->ending == '?'   ? question_rise
                             : phrase->ending == ',' ? continuation_rise
                                                     : -sentence_fall;
        if (to_end < 20)
            f0 += ending * (1 - to_end / 20);
        return std::max(lowest, f0);
    };
    auto samples = klatt_synthesize(segments, pitch);
    for (auto& sample : samples)
        sample = std::clamp(sample * level, -1.0f, 1.0f);
    return samples;
}
std::vector<std::string> cesar_unknown_phones(const std::string& ipa) {
    std::vector<std::string> unknown;
    for (const auto& symbol : split(ipa))
        if (!silent(symbol) && !vowel(symbol) && !consonant(symbol) &&
            std::find(unknown.begin(), unknown.end(), symbol) == unknown.end())
            unknown.push_back(symbol);
    return unknown;
}
} // namespace ortho
