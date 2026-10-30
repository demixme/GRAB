# Localisation

GRAB's UI can be translated. This folder holds one translation file per language in the
**JUCE `LocalisedStrings`** format. They're embedded into the binary at build time and the right
one is chosen automatically from the system language (`src/gui/Main.cpp` → `setupLocalisation()`).

Current languages: **French** (`grab_fr.txt`), **Italian** (`grab_it.txt`), **German**
(`grab_de.txt`), **Spanish** (`grab_es.txt`). English is the source language — no file needed
(untranslated strings fall back to the English in the code).

This is a **scaffold**: the mechanism is wired up end-to-end and the main top-bar controls are
translated as a demonstration. Most of the app's text (help dialogs, panels, status messages) is
not wrapped yet — contributions very welcome.

## File format

```
language: French
countries: fr be ca ch

"Save to device" = "Enregistrer sur l'appareil"
"Autosave" = "Enregistrement auto"
```

- The left side is the **exact English string** as it appears in the code (inside `TRANS("…")`).
- The right side is the translation. UTF-8 accents are fine here — these are data files, not source,
  so they don't follow the ASCII-only rule the C++ does.
- A missing key falls back to English, so partial translations are OK.

## Adding a string to translate

1. In the C++, wrap the user-facing string: `button.setButtonText (TRANS ("Reload from device"));`
   (plain labels: `TRANS ("…")`; it returns a `juce::String`).
2. Add the English key + translation to each `grab_<lang>.txt` file.

## Adding a new language

1. Copy a file to `grab_<xx>.txt` (e.g. `grab_pt.txt`) and translate the values.
2. List it in `CMakeLists.txt` under `juce_add_binary_data(GRABTranslations …)`.
3. Add a branch for it in `setupLocalisation()` in `src/gui/Main.cpp`.

## Testing a language without changing your OS

Temporarily hard-code the 2-letter code in `setupLocalisation()` (e.g. force `"fr"`), build, and run.
