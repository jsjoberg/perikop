# Use Perikop

Perikop is a native Orthodox lectionary reader prototype for Windows, macOS, and Linux.
The interface uses Swedish labels.
It bundles Swedish, Greek, and English Scripture, including Swedish apocrypha and the Greek Septuagint.
The application needs no network connection after installation.

## Installation

- macOS: open the DMG and drag Perikop to Applications.
- Windows 10 or later, x86-64: run the installer. It installs Perikop in Program Files and adds a Start menu shortcut.
- Linux: make the AppImage executable, then run it.

The macOS application has an ad-hoc signature only.
On first launch, open **System Settings → Privacy & Security** and select **Open Anyway**.
The Windows installer has no signature, so SmartScreen shows a warning.

For a source build, use the [build guide](building.md#run).

## Daily readings and calendar

The day page shows the date, its readings, and three reading plans.
The **Kalender** menu moves between days and opens a native date picker.
The picker's Current date button selects the current date before confirmation.

The application selects the current local civil date at startup.
Only explicit actions change this date. Midnight, sleep, and theme changes do not change it.
The selected date does not persist between launches.

New calendar mode uses the North American Antiochian Greek reading rules.
Old calendar mode applies those rules to Julian fixed dates as a comparison mode.
Julian conversion calculates the date difference for each century.
See the [lectionary guide](lectionary.md) for the calculation rules and reference sources.

## Reader navigation

Select a reading to open its chapter context.
Use the mouse wheel, arrow keys, Page Up, Page Down, Home, or End to move through the text.
The reader preserves macOS trackpad precision and momentum.
The brass margin line marks the selected passage.
Prose flows across verses. Small inline numbers preserve verse navigation.

The toolbar has four symbols on each side of the address field.
On the left, Back returns to the day page. **Lyssna**, **Pausa**, and **Stoppa** control read-aloud.
Controls that do not apply are dimmed. The day page dims the reader controls.

Click the address field to go to another passage.
For a reading in several parts, the field is a menu of the parts.
The symbols on the right select word study, Swedish, Greek, or English for the right column.
Click the selected symbol again to close the column.
The text control adds Greek, English, or all three languages to the selected main edition.
Narrow windows use aligned blocks.

## Bible browser

The Bible action opens a native book, chapter, verse, and edition selector.
Books outside the Hebrew Bible have an asterisk.
Books without text in the left pane's language are greyed out. A note under the grid names them.
The browser lists only books available in the selected edition.
The World English Bible includes deuterocanonical books and appears as a separate edition.

Select **Hela boken** or **Hela kapitlet**, before the numbers, to open a whole book or chapter.
**Lyssna** then reads all of it.
Each source retains its wording. An explicit alignment links verses across their numbering systems.
Verses without a counterpart show a message.
See the [versification guide](versification.md) for the source coordinates and displayed Septuagint numbering.

## Word study

Select **Visa → Höger spalt → Ordstudium** to open the lookup panel.
Click a word to see its pronunciation and dictionary entries.
In the New Testament, the panel also shows the verse's Strong's entries.
The [word-study guide](word-study.md) describes the dictionaries, alignment, and data limits.

## Reading plans

The day page offers three plans:

- The New Testament in 30 parts.
- The Masoretic canon in 30 parts.
- More books from the Septuagint in 14 parts.

The last plan contains the books outside the Hebrew Bible that Perikop has in Swedish.
It excludes First Esdras, Third Maccabees, and Psalm 151 because Perikop has no Swedish text for them.
The Masoretic plan keeps the Hebrew Bible's book order and uses Septuagint chapter numbers.
Its parts have about the same length.

Plans count parts, not days.
Select a numbered part to open it.
A part with several books has a section menu in the address field.

After the voice finishes a part, Perikop asks whether to mark it as read.
It also asks after you scroll to the end of a part and leave it.
The day's readings use the same progress controls.
**Börja om** clears one plan after confirmation.

## Swedish read-aloud

**Lyssna** reads Swedish with Alice or Björn from the local Kokoro model.
Select the voice in **Uppläsning → Alice** or **Björn**.
The choice applies from the next reading and survives restarts.
With Greek or English in the left pane, **Lyssna** is disabled.
The voice uses common C++ code on all target platforms.

**Pausa**, **Fortsätt**, and **Stoppa** control playback.
The address field shows loading, buffering, the spoken introduction, and the current verse.
A line along its lower edge shows progress.

Playback starts after the first audio chunk.
The engine generates later chunks during playback, with a buffer of at most two chunks.
Initial model loading and first-chunk synthesis take time.
If synthesis cannot keep pace, playback waits for the next chunk.
A separate SQLite cache accelerates repeat readings.

**Lyssna** opens the passage and follows the spoken text with a soft highlight and a moving margin marker.
Select **Visa → Markera texten som läses upp** to turn the highlight off.
The margin marker stays, and the text still follows the voice.

Manual scrolling releases automatic following.
A translucent **Följ uppläsningen** button then floats above the text.
Select this button to return to the current spoken text.
Without playback, **Till läsningen** appears in the same place after the passage leaves the view.
Pause freezes the marker and scrolling. Stop clears the marker and keeps the page position.

Verse boundaries follow audio playback.
Movement between lines within a verse is an estimate, because the model supplies no word timestamps.
The voices remain a preview pending listening review and remaining platform checks.
Swedish voice quality and Windows, Linux, and Intel Mac speech execution still need independent validation.
See the [Kokoro voice guide](kokoro-voices.md) and [speech selection record](speech-selection.md) for measurements and limits.

## Pronunciation corrections

Open **Uppläsning → Granska svenskt uttal…** to review the prepared Swedish word list.
The tool offers word and verse previews, local corrections, review decisions, and TSV export.
Saved corrections apply to subsequent Swedish playback.
Display text and speech text remain separate.
For example, a correction can change `Melkisedek` to `Melki-sedek` for speech only.
The [pronunciation review guide](pronunciation-review.md) describes the workflow and data limits.

## Appearance and source information

Theme choices are System, Light, and Dark.
The user database stores the theme, calendar choice, parallel language, and font size.
The application bundles Literata, IBM Plex Sans, and a fallback font for Hebrew headings.
The blue and yellow Orthodox cross appears in the window and the macOS Dock.

**Om Perikop** shows the version and every source the program uses, with links.
On macOS, it is in the application menu. Elsewhere, it is in **Hjälp**.
