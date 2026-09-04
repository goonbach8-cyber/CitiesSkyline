City Lab v0.4.2 black-screen fix

Replace ONLY:
  src/main.cpp

with the src/main.cpp contained in this ZIP.

Keep all other v0.4.1 files unchanged.
The fix moves the large City object from the WebAssembly stack into static storage.
