---
title: Bundled lessons are installed once per id; edits to an existing lesson don't reach users who have it
tags: [lessons, install, data, songs]
---

# Lessons install once, by id

## Context
Adding or changing files in `lessons/`.

## Rule
`installLessons()` copies each `lessons/<id>.json` into the user's song folder only if `<id>` is not yet listed in `lessons-installed.txt` in the data folder. A lesson the user deleted stays deleted, and editing an already-installed lesson changes nothing for existing users.

## Why
Users may delete or change lessons; re-copying them on every start would undo that.

## How to apply
New lesson: add a new file with a new id. Fixing a lesson: changing the file only helps new users; to push a fix to everyone, give it a new id (and say so in the PR). Every bundled lesson must load and be playable: `tests/test_core.cpp` checks this.
