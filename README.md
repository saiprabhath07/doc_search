# Digital Document Search System

![Language](https://img.shields.io/badge/language-C-blue) ![Subject](https://img.shields.io/badge/subject-Advanced%20Data%20Structures-green) ![Interface](https://img.shields.io/badge/interface-menu%20driven-orange)

A menu-driven document search engine written in **C**, built as a project-based learning mini project for **Advanced Data Structures**. Documents are indexed with advanced data structures so they can be searched by keyword, prefix, phrase or title.

## Project details

| | |
|---|---|
| **Institution** | Geethanjali College of Engineering and Technology (UGC Autonomous), Cheeryal, Keesara, Medchal-Malkajgiri Dist., Telangana - 501301 |
| **Department** | Artificial Intelligence and Machine Learning (AIML) |
| **Class** | B.Tech 2nd Year, AIML-A |
| **Academic year** | 2026-27 |
| **Subject** | Advanced Data Structures (25CS21001) |
| **Project guide** | Dr. H. Venkata Ramana Rao, Associate Professor |
| **Language** | C |

## Team

| Name | Roll No. |
|---|---|
| K. Akshaya | 26R15A6604 |
| P. Shanmuka Sai Prabhath | 26R15A6605 |
| R. Malavika | 26R15A6606 |
| V. Chaithanya | 26R15A6607 |

## Features

- Add documents by typing them in or importing a `.txt` file
- **Keyword search** (all words must match) with results ranked by relevance
- **Prefix search / autocomplete** on every indexed word
- **Phrase search** inside document text using Brute Force, KMP or Boyer-Moore
- **Algorithm comparison** table showing matches, comparisons and time
- **Title search** with exact and prefix matching
- Documents sorted by title on listing
- Persistent storage in a binary file, loaded again on the next run
- Clean terminal UI with tables, colours and highlighted matches

## Data structures and algorithms used

| Concept | Where it is used |
|---|---|
| **Standard Trie** | Inverted index (word -> documents with frequency); prefix search and autocomplete |
| **AVL Tree** | Self-balancing title index; sorted listing, title search, deletion |
| **Brute Force, KMP, Boyer-Moore** | Phrase search inside document text |
| **Merge Sort** | Ranking keyword results by relevance score |
| **File organization** | Binary record file (`docs.dat`), text file import |

## Getting started

Requirements: any C compiler (GCC recommended).

```bash
git clone https://github.com/saiprabhath07/doc_search.git
cd doc_search
gcc doc_search.c -o doc_search
./doc_search          # on Windows: doc_search.exe
```

On the first run choose **12** to load sample documents, then try the search options.

## Menu

```
DOCUMENTS
  1  Add document
  2  Import from text file
  3  List all documents
  4  View a document
  5  Delete a document
SEARCH
  6  Keyword search
  7  Prefix search / autocomplete
  8  Phrase search
  9  Compare algorithms
 10  Search by title
SYSTEM
 11  Statistics
 12  Load sample documents
 13  Save to file
  0  Save and exit
```

## Sample output

Algorithm comparison for the pattern `search` over the sample documents:

```
+--------------+---------+-------------+----------+----------------------+
| Algorithm    | Matches | Comparisons | Time ms  | Work done            |
+--------------+---------+-------------+----------+----------------------+
| Brute Force  | 6       | 1310        | 1.70     | #################### |
| KMP          | 6       | 1299        | 1.90     | ###################  |
| Boyer-Moore  | 6       | 277         | 1.98     | ####                 |
+--------------+---------+-------------+----------+----------------------+
```

Boyer-Moore skips characters using the bad character rule, so it needs far fewer comparisons.

## How it works

1. Adding a document stores it in memory, inserts its **title** into the AVL tree and inserts every **word** into the trie.
2. Each trie node that ends a word keeps a linked list of postings (`document id`, `frequency`).
3. **Keyword search** looks up each word in the trie, keeps documents containing all of them, adds up the frequencies as a score, and sorts with **merge sort**.
4. **Phrase search** lowercases the text and pattern and runs the chosen matching algorithm on each document.
5. On exit the documents are written to `docs.dat`; the trie and AVL tree are rebuilt when the program starts.

### Complexity

| Operation | Time |
|---|---|
| Trie insert / word lookup | O(L), L = word length |
| AVL insert / delete / search | O(log n) |
| Brute Force matching | O(n * m) |
| KMP matching | O(n + m) |
| Boyer-Moore matching | O(n / m) best, O(n * m) worst |
| Merge sort of k results | O(k log k) |

### Binary file layout (`docs.dat`)

```
[nextId][docCount]
then for each document: [id][title: 100 bytes][length][content bytes]
```

## Documentation

The `docs/` folder contains the full PBL report (`ADS_PBL_Report_Digital_Document_Search_System.docx`) and the presentation (`ADS_PBL_Presentation.pptx`).

## Limitations

- Up to 1000 documents, titles up to 99 characters, indexed words up to 63 characters
- Only letters and digits are indexed
- Deleting a document rebuilds the whole trie

## Possible improvements

- B+ tree index on disk instead of rebuilding indexes on start
- Stop-word removal and stemming
- Fuzzy search and spell suggestions

## Acknowledgement

We thank our guide **Dr. H. Venkata Ramana Rao**, Associate Professor, Department of AIML, Geethanjali College of Engineering and Technology, for his guidance throughout this project.
