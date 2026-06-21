# DuplicateFinder

[![Windows Build](https://github.com/bartekordek/DuplicateFinder/actions/workflows/cmake-windows.yml/badge.svg)](https://github.com/bartekordek/DuplicateFinder/actions/workflows/cmake-windows.yml)
[![Linux Build](https://github.com/bartekordek/DuplicateFinder/actions/workflows/cmake.yml/badge.svg)](https://github.com/bartekordek/DuplicateFinder/actions/workflows/cmake.yml)

A fast and simple **duplicate file detection tool** that scans directories and identifies duplicate files using content-based hashing.

Useful for cleaning up storage, organizing large file collections, and finding redundant data.

---

## ✨ Features

- 🔍 Detect duplicate files based on content (not just names)
- ⚡ Fast scanning using hashing MD5
- 📁 Works on entire directory trees
- 🧠 Collision-aware grouping of identical files
- 🧹 Helps free disk space safely
- 🖥 Cross-platform (Windows / Linux / macOS)

---

## 📦 How it works

DuplicateFinder scans files in a target directory and:

1. Reads file metadata (size, path)
2. Groups candidates by file size (fast pre-filter)
3. Computes hashes for actual comparison
4. Clusters identical files into duplicate groups

No magic. Just brute-force correctness dressed as efficiency.

---

## 🚀 Getting Started

### Clone the repository

```bash
git clone https://github.com/bartekordek/DuplicateFinder.git
cd DuplicateFinder
```

### Build (CMake)

```bash
cmake -S . -B build
cmake --build build
```

---

## ▶️ Usage

Run the tool:

```bash
DuplicateFinder.exe
```

1. Select directories you want to scan.
2. Click start.
3. See what is duplicated.

---

## 📜 License

Specify your license here (MIT / GPL / etc.)

---

## 🤝 Contributing

Pull requests are welcome.

