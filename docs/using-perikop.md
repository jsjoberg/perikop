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

The day page shows the date, fasting information, commemorations, service readings, and three reading plans.
Press **Läs** beside a reading to open it.
Service labels identify Vespers, Matins, the Hours, and other readings when they apply.
Saints' readings appear beside the ordinary daily readings.
Fasting information identifies the period, permitted exceptions, and foods to abstain from.
Commemorations use Swedish names where available. Other names retain the source's English text.
Composite liturgical readings show their citations without a **Läs** button. Their adapted text is not in the Bible corpus.
The arrows beside the date move to the previous or next day.
The **Idag** button selects the current local date. This button always works and needs no background timer.
Click the date, or select **Kalender → Välj datum**, to open the month calendar below the date.
Click a day to show its readings. The arrows beside the month name change the month.
With the keyboard, the arrow keys move the day, Page Up and Page Down change the month, and Home moves to today.
Return selects the day, and Escape closes the calendar.

The application selects the current local civil date at startup.
Only explicit actions change this date. Midnight, sleep, and theme changes do not change it.
The selected date does not persist between launches.

Select the reading order in the **Kalender** menu:

- **Grekiska läsordningen** uses the Greek order. When Antiochian readings differ, the day page shows both, with church names and an explanation.
- **Slaviska läsordningen** follows the Russian and OCA order.

Shared Greek and Antiochian readings appear once. Press **Läs** beside either variant to open it.
Separate Serbian and Georgian variants need additional source tables.
Existing Antiochian settings now select the Greek family.

**Gregoriansk / reviderad juliansk** uses Revised Julian fixed dates. **Juliansk** uses Julian fixed dates.
Revised Julian dates match Gregorian dates through February 2800.
Each reading order works with both calendars. The application keeps both choices between launches.
Julian conversion calculates the date difference for each century.
See the [lectionary guide](lectionary.md) for the calculation rules and reference sources.

## Reader navigation

Select a reading to open its chapter context.
Use the mouse wheel, arrow keys, Page Up, Page Down, Home, or End to move through the text.
The reader preserves macOS trackpad precision and momentum.
The brass margin line marks the selected passage.
Prose flows across verses. Small inline numbers preserve verse navigation.

The toolbar has four symbols on each side of the address field.
On the left, Back returns from the reader to the day page.
**Lyssna**, **Pausa**, and **Stoppa** control read-aloud.
The calendar, Bible browser, and **Om Perikop** page use the main window.
Back returns to the previous view and preserves the reader position, selection, word study, and playback state.
The address field shows the current view. Click it on the day or About page to open the Bible browser.
Back then returns to that page. Escape also returns to the previous step.
Playback and column controls stay dimmed while these pages are open.
Audio continues while a page is open.
Controls that do not apply are dimmed. The day page dims the reader controls.

Click the address field to go to another passage.
For a reading in several parts, the field is a menu of the parts.
The symbols on the right select word study, Swedish, Greek, or English for the right column.
Click the selected symbol again to close the column.
The text control adds Greek, English, or all three languages to the selected main edition.
Narrow windows use aligned blocks.

## Bible browser

Select **Bibel → Gå till bibelställe** to open the book, chapter, and verse grids.
Select a book to unfold its chapters directly beneath its row. Select a chapter to unfold its verses beneath it.
Select an open book or chapter again to fold it.
The page has a vertical scrollbar. Select a verse to open it immediately.
**Hela boken** and **Hela kapitlet** open complete books and chapters.
Books without text in the left column language are greyed out.
Back moves from verses to chapters, then books, then the previous view.

To prepare a custom reading, enable **Flera intervall**:

1. Select a book and chapter.
2. Select the start verse.
3. Select the end verse.
   For a range across chapters, use Back to select another chapter in the same book.
4. Select **Lägg till intervall**. For one verse, omit the end verse.
5. Add more ranges in the order for playback. Ranges can come from different books.
6. To remove a range, select it in the list and press **Ta bort**.
7. Select **Öppna urval** to show the reading.
8. Select **Lyssna** to play all ranges in list order.

**Lägg till hela boken** and **Lägg till hela kapitlet** add complete books and chapters to the list.
The address field shows a menu of the selected parts.

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

- **Nya testamentet**: the New Testament in 30 parts.
- **Gamla testamentet**: the books of the Hebrew Bible in 30 parts.
- **Septuagintas övriga böcker**: the books outside the Hebrew Bible in 14 parts.

The Old Testament plan keeps the Hebrew Bible's book order and uses Septuagint chapter numbers.
Esther and Daniel include their Greek additions. Its parts have about the same length.
The last plan contains the books outside the Hebrew Bible that Perikop has in Swedish.
It excludes First Esdras, Third Maccabees, and Psalm 151 because Perikop has no Swedish text for them.

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

Drag across verses in the left column to mark a range.
Hold Ctrl (Windows/Linux) or Command (macOS) while you drag to add separate ranges.
**Läs markering** plays the marked ranges in Scripture order and combines overlapping ranges.
A click without a modifier clears the selection.

**Lyssna** opens the passage and follows the spoken text with a moving margin marker.
The text highlight is off by default.
Select **Visa → Markera texten som läses upp** to turn the highlight on.
Perikop saves this choice. Existing saved choices remain in effect.
The margin marker and automatic following work with either choice.

Manual scrolling releases automatic following.
A translucent **Följ uppläsningen** button then floats above the text.
Select this button to return to the current spoken text.
Without playback, **Till läsningen** appears in the same place after the passage leaves the view.
Pause freezes the marker and scrolling. Stop clears the marker and keeps the page position.
Pause also suspends audio preparation after the current chunk finishes.

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
