#pragma once
#include "speech/speech.hpp"
#include "storage/database.hpp"
#include <wx/window.h>
namespace ortho {
void show_pronunciation_review(wxWindow*, const CorpusDb&, PronunciationReviewDb&, SpeechEngine&,
                               const std::filesystem::path&);
}
