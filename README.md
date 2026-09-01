
# Cities Prototype C++ v0.1

Ein echter C++-3D-Prototyp mit raylib, kompiliert als WebAssembly.

## Features

- echte 3D-Kamera
- WASD-Bewegung
- Q/E-Kameradrehung
- Zoom mit Mausrad
- Straßen per Ziehen
- einfache Wohn-/Gewerbe-/Industriegebäude
- Geld, Einwohner und Jobs
- automatischer Build über GitHub Actions
- Deployment auf GitHub Pages

## Steuerung

- `WASD`: Kamera bewegen
- `Q / E`: Kamera drehen
- `Mausrad`: Zoom
- `1`: Straße
- `2`: Wohnen
- `3`: Gewerbe
- `4`: Industrie
- `Leertaste`: Pause
- Linksklick/Ziehen: bauen

## GitHub Pages

Unter `Settings -> Pages` muss als Source `GitHub Actions` gewählt sein.
Nach jedem Push auf `main` wird der C++-Code automatisch zu WebAssembly kompiliert und veröffentlicht.
