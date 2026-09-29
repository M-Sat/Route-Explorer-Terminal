# Route Explorer

Route Explorer is a command-line C++ program for exploring routes through country and state/province adjacency graphs. Enter a country/state or its representative city, and the program reports both the route with the fewest borders and the route with the shortest total great-circle distance.

## Screenshot
<img width="1472" height="663" alt="image" src="https://github.com/user-attachments/assets/e21cd286-6006-4cd2-8681-07059bf04bdb" />

## Features

- Accepts either a location name or its representative city name as input.
- Uses breadth-first search (BFS) to find the minimum number of borders and list every route that achieves that minimum.
- Uses Dijkstra's algorithm to find a route with the minimum summed distance in kilometers.
- Loads its graph and representative city names from a CSV file.

## Repository Contents

- `re33.cpp` - C++ implementation of CSV loading, input validation, BFS, Dijkstra, and result display.
- `eu.csv` - Europe route graph.
- `us.csv` - contiguous United States route graph.
- `na.csv` - North America route graph, including the contiguous United States, Canada, and Alaska.
- `generatecsvs.py` - Generates the three CSV files from the location coordinates and adjacency lists in the script.

Each CSV row has this format, with no header:

```text
location1,location2,city1,city2,distance_km
```

The distance is the great-circle distance between the representative cities, rounded to the nearest kilometer. It is not a road or driving distance. Each connection is listed in both directions, so the graph can be traversed either way.

## Requirements

- A C++ compiler with C++20 support, such as GCC (`g++`).
- Python 3 to regenerate the CSV files. The generator uses only Python's standard library.

## Build and Run

Open a terminal in the `route-explorer` directory, where `re33.cpp` and the CSV files are located. With GCC, build and run the program with:

```powershell
g++ -std=c++20 -Wall -Wextra re33.cpp -o re33.exe
.\re33.exe
```

The program currently has `eu.csv` fixed as its input file. Run it from the directory containing `eu.csv`; the US and North America CSVs are included but are not selectable at runtime. To use another dataset, change `CSV_FILENAME` in `re33.cpp` to `us.csv` or `na.csv`, then rebuild.

When prompted, enter a country/state/province name or its representative city. Names must match the CSV spelling and capitalization.

## Regenerate the CSV Files

From the `route-explorer` directory, run:

```powershell
python generatecsvs.py
```

This writes `eu.csv`, `us.csv`, and `na.csv` in the current directory, replacing files with those names if they already exist.

## Executable

`re33.exe` is a compiled Windows build artifact, not a source file. It does not need to be included in the repository: users can compile `re33.cpp` themselves using the command above. The executable is also platform-specific, so a Windows `.exe` would not work for users on other operating systems.
