# Domarbokens tryckvittnen

Oförändrade bildfiler som faktiskt öppnades vid Codex granskning 2026-10-31. Ingen bild är OCR-text.

- EFS 1927: Project Runebergs original-TIFF, tryckta s. 333–369. Hela Domarboken bildjämfördes; Josua på första sidan och Rut på sista sidan tillgodoräknas inte.
- EFS Stockholm 1926: Internet Archives JPEG-bilder, tryckta s. 305–337, blad n316–n348, identifierare `bibelnellerdenhe0000unse`. Bilderna användes som andra fysiskt tryckta vittne för rättelser och behållna varianter; snäva marginaler i skanningen är inte skäl att fylla i osedd text.

[manifest.json](manifest.json) anger exakt URL, lokal fil och SHA-256 för alla 70 bilder. JPEG:erna är de byte som respektive `/page/nN.jpg` returnerade, inte hashvärden för JP2-originalen. TIFF-filerna är de ursprungliga nedladdade Runeberg-filerna. Granskaren läste även utsnitt av svårlästa ställen; utsnitt har inte ersatt originalen.

[Rapport och kvarvarande varianter](../sv1917-judges.md). Test: `uv run --locked tests/swedish_judges_test.py`.
