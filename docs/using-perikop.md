# Use Perikop

Perikop is a native Orthodox lectionary reader prototype for Windows, macOS, and Linux.
The interface uses Swedish labels.
It bundles Swedish, Greek, and English Scripture, including Swedish apocrypha and the Greek Septuagint.
The application needs no network connection after installation.

## Installation

- macOS 13.4 or later, Intel or Apple Silicon: open the DMG and drag Perikop to Applications.
- Windows 10 or later, x86-64: run the installer. It installs Perikop in Program Files and adds a Start menu shortcut.
- Linux x86-64 with glibc 2.34 or later: make the AppImage executable, then run it.

x86-64 packages require an `x86-64-v2` processor, which includes SSE4.2 and POPCNT support.
The Core i3 ThinkPad X220i is within this CPU target. Execution on that hardware still needs validation.

The macOS application has an ad-hoc signature only.
On first launch, open **System Settings → Privacy & Security** and select **Open Anyway**.
The Windows installer has no signature, so SmartScreen shows a warning.

For a source build, use the [build guide](building.md#run).

## Daily readings and calendar

The day page shows the date, the reading-cycle day and week, service readings, and three reading plans.
The date controls, month calendar, and day heading are centered above the readings.
Press **Läs** beside a reading to open it.
Point to a reading for a short preview of its beginning. An ellipsis marks omitted text.
Service labels identify Vespers, Matins, the Hours, and other readings when they apply.
Saints' readings appear beside the ordinary daily readings.
Composite liturgical readings show their citations without a **Läs** button. Their adapted text is not in the Bible corpus.
The arrows beside the date move to the previous or next day.
The calendar marks the current local date with a blue outline.
Violet circles mark Orthros readings, gold diamonds mark Vespers readings, and green squares mark other service readings.
Other services include the Hours and the Great Blessing of Waters.
A day can have several service markers. Ordinary Epistle and Gospel readings need no marker.
The calendar starts on January 1, 2019.
Point to a date for its services and reading counts, including any Antiochian alternatives.
The markers follow the selected calendar style and reading order.
**Kalender → Idag**, or **Ctrl+T**, selects the current local date.
Click the date, or select **Kalender → Välj datum**, to open the month calendar below the date.
Click a day to show its readings. Arrows on either side of the month name change the month; arrows around the year jump one year.
Click the month name or the year to show a grid of months or of twelve years in place of the days. Click a month or year to go there.
The grids mark the current month and year like today. In the year grid, the year arrows show the previous or next twelve years.
Year jumps keep the month and day, using February 28 when February 29 is unavailable.
With the keyboard, the arrow keys move the day, Page Up and Page Down change the month, Shift+Page Up and Shift+Page Down change the year, and Home moves to today.
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

Click a verse to highlight it. Click the same verse again to close the highlight.
In parallel view, either language can open or close the highlight. Both columns highlight the corresponding verse.
In **Bara svenska**, source buttons appear above the selected verse and move with it during scrolling.
Greek, English, and word-study views hide these buttons.
They disappear when the verse leaves the screen.
**SFB15**, **SKB**, **SRB**, and **B2026** open the verse in your browser.
Links use each source's verse numbering. Buttons without a verified verse link are disabled.
Opening an online source needs an internet connection.

The toolbar has four symbols left of the address field, and the column symbols and ⋮ to its right.
On the left, Back returns from the reader to the day page.
**Lyssna**, **Pausa**, and **Stoppa** control read-aloud.
The calendar, Bible browser, and **Om Perikop** page use the main window.
Back returns to the previous view and preserves the reader position, selection, and word study.
The address field shows the current view. Click it on the day or About page to open the Bible browser.
In the Bible browser, click the address field again to close it and return to the previous view.
Back and Escape move to the previous step.
Playback and column controls stay dimmed while these pages are open.
Leaving a reading pauses its audio. Returning to it keeps the audio paused until you select **Fortsätt**.
Opening another reading stops the previous audio session.
Playback controls are disabled on the day page and other pages while audio is paused.
Controls that do not apply are dimmed. The day page dims the reader controls.

Click the address field to go to another passage.
For a reading in several parts, the field is a menu of the parts.
The left column is always the Swedish text.
The symbols on the right select word study, Greek, or English for the right column.
Click the selected symbol again to close the column.
Narrow windows use aligned blocks.

The ⋮ button at the right end of the toolbar opens a menu with every menu command, as in Chrome.
It opens inside the window, aligned with the button's right edge.
On Windows and Linux the menu bar is hidden. Press and release Alt to show it, and again to hide it.
The keyboard shortcuts work either way. macOS keeps its own menu bar.
**Visa → Normal textstorlek** (Ctrl+0, ⌘0 on macOS) returns the text to the default size.

## Bible browser

Select **Bibel → Gå till bibelställe** to open the book, chapter, and verse grids.
Select a book to unfold its chapters directly beneath its row. Select a chapter to unfold its verses beneath it.
Book tiles keep their name tooltips. Chapter tooltips show short text previews, and verse tooltips show the complete verse.
Previews use Swedish where available, otherwise English or Greek with a language label.
Select an open book or chapter again to fold it.
The page has a vertical scrollbar. The normal browser selects one continuous range.
The address field shows the current range.
The selection starts with the complete book. The first chapter selection changes the range to the complete chapter.
Another chapter selection extends the range to include both chapters and all chapters between them.

The first verse selection within a complete chapter starts a range at that verse.
Later verse selections change the end of the range.

For example, select John 3. Then select verse 3. Then select verse 5.
The range is John 3:3–3:5.

Then select chapter 5. The range extends from John 3:3 to the end of chapter 5.
Then select verse 5 in chapter 5. The range changes to John 3:3–5:5.
Select **Öppna urval** to open the range.

Books without Swedish text, such as 1 Esdras, open too, for their Greek and English text.
Their tooltip says **Svenska saknas**. The Swedish column shows "Saknas i denna utgåva".
Back moves from verses to chapters, then books, then the previous view.

To read several passages as one reading, select a range and then **Lägg till**.
The range becomes Del 1, and the address field shows it as for a reading in several parts.
Select the next range, from the same or another book, and **Lägg till** again.
**Öppna urval** opens the added parts, and the current range as the last part, as one reading.
**Lyssna** then plays the parts in order.
**Rensa** removes the added parts. The address field previews the selected range or the last added part.
After you open the selection, use the address field to choose a part of the reading.

Each source retains its wording. An explicit alignment links verses across their numbering systems.
Verses without a counterpart show a message.
See the [versification guide](versification.md) for the source coordinates and displayed Septuagint numbering.

## Word study

Select **Visa → Svenska och ordstudium** to open the lookup panel.
Click a Swedish word to see its pronunciation and dictionary entries.
Only that word is highlighted. Click it again to close the highlight and lookup.
Verse source buttons appear only in **Bara svenska**.
The [word-study guide](word-study.md) describes the dictionaries and data limits.

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
Its tooltip lists the passage ranges, one per line. The next plan part on the day page uses the same tooltip.
A part with several books has a section menu in the address field.

After the voice finishes a part, Perikop asks whether to mark it as read.
It also asks after you scroll to the end of a part and leave it.
The day's readings use the same progress controls.
**Börja om** clears one plan after confirmation.
Tab moves between plan parts and toolbar controls. Enter or Space activates the focused control.
The focus outline identifies the selected control.

## Swedish read-aloud

**Lyssna** reads Swedish with Alice or Björn from the local Kokoro model.
Select the voice in **Uppläsning → Alice** or **Björn**.
The choice applies from the next reading and survives restarts.
The voice uses common C++ code on all target platforms.

**Pausa**, **Fortsätt**, and **Stoppa** control playback.
Each reading starts directly with its first verse.
The address field shows loading, buffering, and the current verse.
A line along its lower edge shows progress.

Playback starts after enough audio is ready for the estimated synthesis speed.
Fast synthesis needs a short lead. Slower synthesis can prepare more of the reading before playback.
The rolling buffer limit ranges from 30 to 600 seconds of source audio.
Pause also suspends preparation between chunks.
Initial model loading and first-chunk synthesis take time.
If synthesis cannot keep pace, playback waits for the next chunk.
Select **Uppläsning → Förbered ljud för vald dag** to generate audio for the selected calendar day's readings and alternatives before listening.
To prepare tomorrow in the evening, select tomorrow's date first, start preparation, and wait until **Ljudet är förberett** appears before putting the computer to sleep.
The prepared audio is then available when you wake the computer in the morning. Preparation targets the selected date, even when a different Bible passage is open.
Preparation uses the selected voice and pronunciation corrections. It produces no sound and does not mark readings as read.
The address field shows preparation progress. **Stoppa** cancels unfinished preparation.
Starting playback, changing days, or changing the voice cancels unfinished preparation. Completed audio remains available.
Prepared audio survives restarts and date changes. The cache retains up to 256 MiB and replaces older audio when needed.
Voice and pronunciation changes use separate cache entries. Playback speed changes reuse the same audio.
Preparation covers the day's readings that open in the reader. Reading-plan parts are prepared during their normal playback.

A separate SQLite cache accelerates repeat readings.
If the cache fails, speech continues without it and the status shows **utan ljudcache**.
If personal pronunciation data fails, the reader uses the bundled pronunciations and preserves the database.

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
