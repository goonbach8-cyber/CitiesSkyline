# So laedst du v0.4 korrekt hoch

1. `.github/workflows/deploy.yml` im Repo **behalten**. Die Version in diesem Paket ist identisch kompatibel.
2. Im bisherigen `src/`-Ordner alle alten v0.3-Dateien loeschen.
3. Den kompletten neuen `src/`-Ordner hochladen.
4. `CMakeLists.txt` im Repo durch die neue Version ersetzen. Das ist wichtig, weil v0.4 die neue Datei `BuildingRenderer.cpp` kompiliert.
5. `web/shell.html` durch die neue Version ersetzen.
6. `README.md` kann ebenfalls ersetzt werden.
7. Die Dateien muessen direkt im Repo liegen, also z.B. `src/main.cpp` und NICHT `CitiesSkyline-cpp-v0.4/src/main.cpp`.
8. Nach dem Commit startet GitHub Actions automatisch.

Bei einem Fehler im Build: den ersten roten Fehlerblock aus `Configure` oder `Build` schicken.
