#pragma once

#include "speech/speech.hpp"
#include <functional>

namespace ortho {
using PronunciationLookup = std::function<std::vector<Pronunciation>(const std::string& language)>;
using SpeechSourceLookup = std::function<std::string(const Reading&, const std::string& book, int chapter)>;

// Build verse utterances in displayed order with framing cues.
// The optional chapter lookup follows the displayed edition; otherwise use the reading's edition.
std::vector<SpeechUtterance> reading_speech(const CorpusDb& corpus, const std::vector<Reading>& readings,
                                            const PronunciationLookup& lexicon,
                                            const SpeechSourceLookup& source = {});
// Keep original cues and the complete current verse, including merged verses.
// Returns the share of the queue skipped, where the progress bar resumes.
double resume_speech(std::vector<SpeechUtterance>&, const ReadingPosition&, const CorpusDb* corpus = nullptr);
} // namespace ortho
