# City Lab C++ v0.4

v0.4 ist ein deutlich groesserer Schritt vom Technik-Prototypen Richtung echtes City-Builder-Spiel.

## Neu in v0.4

### Welt & Grafik
- 92x92 Karte mit Huegeln, Fluss, See, Ufer und Fels-/Hochlandflaechen
- animierte Wasseroberflaeche
- prozedurale Baeume und Felsen
- Strassen mit Gehwegen, Randsteinen, Mittelmarkierungen und Strassenlampen
- groessere Grundstuecke (1x1, 2x1, 2x2)
- deutlich mehr Gebaeudeformen und visuelle Variationen
- Wohnhaeuser, Mehrfamilienhaeuser und Wohnblocks
- Shops, Supermarkt-/Ladenformen und Buerogebaeude
- Lagerhallen, Fabriken, Silos, Tanks, Docks und Schornsteine
- Fassaden-, Dach-, Detail- und Farbvarianten werden kombiniert

### Gameplay & Simulation
- dynamische Nachfrage fuer Wohnen, Gewerbe und Industrie
- Einwohner, Jobs, besetzte Jobs und Arbeitslosigkeit
- Zufriedenheit und Grundstueckswert
- Industrieverschmutzung senkt Wohnattraktivitaet
- Parks erhoehen Attraktivitaet und Grundstueckswert
- Gebaeude ziehen schrittweise Bewohner/Arbeitskraefte an
- Gebaeude koennen bei guten Bedingungen auf Level 2 und 3 wachsen
- Stromversorgung mit Kapazitaet und Reichweite
- Wasserversorgung mit Kapazitaet und Reichweite
- Gebaeude entstehen nur in versorgten, erschlossenen Zonen
- monatliche Steuereinnahmen und Unterhaltskosten
- unterschiedliche Simulationsgeschwindigkeiten: Pause / 1x / 2x / 3x
- Datum mit Tag, Monat und Jahr

## Steuerung
- WASD: Kamera bewegen
- Rechte Maustaste ziehen: Kamera drehen
- Mausrad: Zoom
- SPACE: Pause / weiter
- 1: Strasse
- 2: Wohnen
- 3: Gewerbe
- 4: Industrie
- 5/B: Bulldozer
- 6: Kraftwerk
- 7: Wasserturm
- 8: Park

Die Tool- und Geschwindigkeitsbuttons koennen auch mit der Maus angeklickt werden.

## Sinnvoller Start
1. Starterstrasse erweitern.
2. Kraftwerk neben einer Strasse platzieren.
3. Wasserturm in der Naehe platzieren.
4. Wohn-, Gewerbe- und Industriezonen entlang der Strassen ziehen.
5. Park in ein Wohnquartier setzen.
6. Nachfrage, Strom/Wasser, Budget und Arbeitslosigkeit beobachten.

## GitHub Pages
Der bestehende GitHub-Actions-Workflow kompiliert das C++-Projekt mit Emscripten/WebAssembly und deployed es zu GitHub Pages.
