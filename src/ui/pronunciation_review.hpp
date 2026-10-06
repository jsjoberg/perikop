#pragma once
#include "storage/database.hpp"
#include "speech/speech.hpp"
#include <wx/window.h>
namespace ortho {
void show_pronunciation_review(wxWindow*,const CorpusDb&,PronunciationReviewDb&,SpeechEngine&,const std::filesystem::path&);
}
