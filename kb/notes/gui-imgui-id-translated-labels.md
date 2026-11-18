---
title: A translated ImGui label needs "###id" (L() in gui_main.cpp), not "##id", or the widget's ID changes with the language
tags: [gui, imgui, i18n, widget-id, labels]
---

# ImGui IDs of translated labels

## Context
Any ImGui widget, window or tree node in `src/gui_main.cpp` whose visible label is translated or comes from data (folder names, technique names).

## Rule
Build the label as `translation + "###" + stable id`; `L("Folder")` does this with the English text as the id. Hard-coded ids work the same: `"###play"` for the Play/Stop button, `"###mic"`, `"###" + folder` for song-list folders, `"###" + t.id` for technique checkboxes.

## Why
ImGui hashes the whole label into the ID, including the part after `##`, which is only hidden. So `"Tocar##play"` and `"Play##play"` are different widgets; only `###` makes everything before it ignored for the ID. With a changing ID, switching language (or the Play/Stop label, or a folder's song count) resets state such as a collapsing header's open state, the Settings window position, or an active slider drag. Two English strings that translate to the same text would also collide.

## How to apply
Use `L("English")` for every translated widget label; use `tr()` only for plain text, tooltips and slider format strings, which are not IDs. `SeparatorText` takes no ID, so `tr()` is enough there. When a label mixes data and a count, put the stable part after `###`.
