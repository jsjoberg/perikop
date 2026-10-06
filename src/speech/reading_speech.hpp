#pragma once

#include "speech/speech.hpp"
#include <functional>

namespace ortho {
using PronunciationLookup = std::function<std::vector<Pronunciation>(const std::string& language)>;

// Build utterances in displayed order, including introductions and framing cues.
// Readings already carry the caller's chosen language and edition.
std::vector<SpeechUtterance> reading_speech(const CorpusDb& corpus, const std::vector<Reading>& readings,
                                            const PronunciationLookup& lexicon);
} // namespace ortho
