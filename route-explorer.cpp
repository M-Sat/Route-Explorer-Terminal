#include <iostream> // console input/output
#include <fstream> // ifstream: reading the CSV file from disk
#include <sstream> // stringstream: splitting one CSV line into comma separated fields
#include <string> // string, getline, stoi: text handling and text -> int conversion
#include <unordered_map> // hash maps: graph (adjacency list), capitals, distances, parents (average O(1) lookup)
#include <unordered_set> // hash set: "visited" set used by BFS
#include <queue> // queue (for BFS) and priority_queue (for Dijkstra)
#include <algorithm> // reverse: flips a path that was built backwards
#include <vector> // dynamic arrays: neighbor lists, paths

using namespace std; 

const string CSV_FILENAME = "eu.csv"; 

/*
    Edge
    ----
    Describes ONE directed connection going out of some country (the source country is the key
    under which this Edge is stored in the graph map, so it is not stored in the struct itself).
    Because the CSV is bidirectional, every border appears twice (A->B and B->A), which gives an
    undirected graph made of two directed edges per border.

    Fields:
      neighbor        - name of the country on the other side of the border (the node we can move to)
      neighborCapital - capital of that neighbor (loaded from the CSV, currently not used by the algorithms)
      distance        - road/straight distance in km between the two capitals; this is the edge weight
                        used by Dijkstra (BFS ignores it and treats every edge as cost 1)
*/
struct Edge {
    string neighbor; // country reached by following this edge
    string neighborCapital; // capital of that neighbor country
    int distance; // edge weight in km between the two capitals
}; 

/*
    Reads the CSV file and builds the two data structures the rest of the program depends on:
      1) graph    - an adjacency list: key = country name, value = vector of Edges leaving that country.
                    An adjacency list is the right choice for a sparse graph like EU borders, because
                    it stores only the borders that exist and lets us iterate over a node's neighbors quickly.
      2) capitals - maps country name -> its capital name, used later for printing and for resolving
                    user input that is given as a capital.

    Parameters (both passed by non-const reference, so the function fills the caller's maps):
      graph    - output, adjacency list to be filled
      capitals - output, country -> capital map to be filled
*/
int createGraph(
    unordered_map<string, vector<Edge>>& graph, 
    unordered_map<string, string>& capitals 
) {
    ifstream file(CSV_FILENAME); // open the CSV for reading
    if (!file.is_open()) { // check whether opening failed 
        cout << "Error opening file." << endl; 
        return 1; 
    } 

    string line; // buffer holding the current raw line of the file

    while (getline(file, line)) { // read the file line by line; loop ends at end-of-file 
        stringstream ss(line); // wrap the line in a stream so we can split it by commas with getline

        string country1; // source country (the node the edge starts from)
        string country2; // destination country (the neighbor)
        string capital1; // capital of the source country
        string capital2; // capital of the destination country
        string km; // distance as text for now, converted to int below

        getline(ss, country1, ','); // read characters up to the first comma 
        getline(ss, country2, ','); 
        getline(ss, capital1, ','); 
        getline(ss, capital2, ','); 
        getline(ss, km, ','); 

        Edge edge; // create the edge object that will describe country1 -> country2
        edge.neighbor = country2; // the edge leads to country2
        edge.neighborCapital = capital2; // remember that neighbor's capital
        edge.distance = stoi(km); // convert the distance text to an int 
        graph[country1].push_back(edge); // append the edge
        capitals[country1] = capital1; // store the capital of country1 
    }

    return 0;
}

/*
    inputExists
    Validates what the user typed. The input is valid if it is either the exact name of a country
    (a key in the graph) or the exact name of a capital (a value in the capitals map).

    Parameters:
      graph    - adjacency list; only its keys are used here (fast O(1) hash lookup for countries)
      capitals - country -> capital map; searched by value, which needs a linear scan because the map is keyed by country
      input    - the text the user typed

    All parameters are const references: no copying (efficient) and the function promises not to modify them.

    Returns: true if input is a known country or capital, otherwise false.
*/
bool inputExists(
    const unordered_map<string, vector<Edge>>& graph, // read-only view of the graph 
    const unordered_map<string, string>& capitals, // read-only view of the country -> capital map
    const string& input // read-only view of the user's input text
) { 
    if (graph.find(input) != graph.end() ){ // Check if input is a country; find() returns end() when the key is missing
        return true; 
    } 
    for (const auto& [country, capital] : capitals) { // Check if input is a capital; loop over all pairs using structured bindings
        if (capital == input) { 
            return true; 
        } 
    } 
    return false; // neither a country nor a capital
}

/*
    resolveLocation
    Converts the user's input into a country name, because the graph is keyed by country.
    If the user typed a country, it is returned unchanged. If the user typed a capital, the country
    that owns that capital is returned. This lets the rest of the program (BFS, Dijkstra) work only
    with country names and not care about how the user entered the location.

    Parameters:
      capitals - country -> capital map (keys are countries, values are capitals)
      input    - the text the user typed 

    Returns: the country name, or an empty string "" if nothing matched (should not happen after validation).
*/
string resolveLocation(
    const unordered_map<string, string>& capitals, // read-only view of the country -> capital map
    const string& input // read-only view of the user's input text
) { 
    if (capitals.find(input) != capitals.end()) { // is the input a key of capitals, i.e. a country name?
        return input; // yes, it is already a country, return it as it is
    } 
    for (const auto& [country, capital] : capitals) { // otherwise scan every (country, capital) pair
        if (capital == input) { // does this pair's capital equal the input?
            return country; // yes, so the country owning that capital is the answer
        } 
    } 
    return ""; 
} 

/*
    bfs  (Breadth-First Search)
    Finds the minimum number of borders crossed between start and finish, ignoring kilometers
    (every edge counts as 1). BFS explores the graph in layers: first all nodes 1 border away,
    then 2 borders away, and so on, using a FIFO queue. The first time a node is reached is
    therefore guaranteed to be via a path with the fewest edges.

    Unlike a normal BFS that stores a single parent per node, this version stores a VECTOR of parents
    per node: every node that reaches it with the same shortest distance. That way ALL shortest
    paths (by border count) can be rebuilt later, not just one.

    Time complexity: O(V + E). Space: O(V + E) in the worst case for the parents lists.

    Parameters:
      graph    - adjacency list (read-only)
      start    - country where the search begins
      finish   - country we want to reach
      distance - OUTPUT: distance[node] = number of borders from start to node (also acts as a marker of reached nodes)
      parents  - OUTPUT: parents[node] = ALL nodes that precede it on some shortest path from start

    Returns: true if finish was reached, false if it is unreachable.
*/
bool bfs(
    const unordered_map<string, vector<Edge>>& graph, // read-only adjacency list
    const string& start, // starting country
    const string& finish, // target country
    unordered_map<string, int>& distance, // output: border count from start to each reached country
    unordered_map<string, vector<string>>& parents // output: all shortest-path predecessors of each country
) {
    queue<string> q; // FIFO queue of countries waiting to be processed 
    unordered_set<string> visited; // countries already discovered, prevents processing a node twice / infinite loops
    q.push(start); // begin with the start country in the queue
    visited.insert(start); // mark start as discovered
    distance[start] = 0; // start is 0 borders away from itself
 
    while (!q.empty()) { // keep going until there is nothing left to explore
        string current = q.front(); // take the oldest country in the queue 
        q.pop(); // remove it from the queue

        if (current == finish) { // reached the target
            return true; // safe to stop: all parents of finish were recorded while its predecessors were processed
        } 

        for (const auto& neighbor : graph.at(current)) { // loop over all Edges leaving current (.at() because graph is const, operator[] would not compile)
            if (visited.find(neighbor.neighbor) == visited.end()) { // first time we see this neighbor
                distance[neighbor.neighbor] = distance[current] + 1; // it is one border farther than current
                parents[neighbor.neighbor].push_back(current); // current is its first known predecessor
                visited.insert(neighbor.neighbor); // mark as discovered so it's not added to the queue again
                q.push(neighbor.neighbor); // schedule it for exploration
            } else if (distance[neighbor.neighbor] == distance[current] + 1) { // already discovered, but is this another equally short route to it?
                parents[neighbor.neighbor].push_back(current); // yes, so record current as an additional parent (this is what enables "all shortest paths")
            } 
        } 
    } 
 
    return false; // queue emptied without meeting finish: no path exists
}

/*
    dijkstra
    Finds the path with the smallest total distance in kilometers between start and finish.
    Works on graphs with non-negative edge weights. It repeatedly picks the not-yet-finalized node
    with the smallest known distance (using a min-heap / priority queue) and tries to improve
    the distances of its neighbors.

    This implementation uses "lazy deletion": instead of updating a node's priority inside the heap
    (which std::priority_queue can't do), a new, better entry is pushed and outdated entries are
    skipped when popped (the "currentDistance > distance[current]" check).

    Only one shortest path is stored (a single parent per node), unlike the BFS version.

    Time complexity: O((V + E) log V) with a binary heap.

    Parameters:
      graph    - adjacency list (read-only), edge.distance is the weight
      start    - country where the search begins
      finish   - country we want to reach
      distance - OUTPUT: distance[node] = best known km from start; a node missing from the map means "infinity / not reached yet"
      parent   - OUTPUT: parent[node] = previous country on the best path (used to rebuild the path)

    Returns: true if finish was reached, false if it is unreachable.
*/
bool dijkstra(
    const unordered_map<string, vector<Edge>>& graph, // read-only adjacency list with weights
    const string& start, // starting country
    const string& finish, // target country
    unordered_map<string, int>& distance, // output: best km from start to each reached country
    unordered_map<string, string>& parent // output: predecessor on the best path
) {
    priority_queue<pair<int, string>, vector<pair<int, string>>, greater<pair<int, string>>> pq; // min-heap of (distance, country); "greater" flips the default max-heap so the smallest distance is on top; pairs compare by distance first

    distance.clear(); // make sure no old data is left from earlier use
    parent.clear(); // same for the parent map

    distance[start] = 0; // start is 0 km from itself
    pq.emplace(0, start); // push (0, start) constructing the pair in place

    while (!pq.empty()) { // continue while there are candidate nodes
        auto [currentDistance, current] = pq.top(); // read the closest candidate; structured binding unpacks the pair (copies, so it's safe after pop)
        pq.pop(); // remove it from the heap

        if (currentDistance > distance[current]) { // this entry is outdated: a shorter route to current was already found and processed
            continue; // skip it (this is the "lazy deletion" trick)
        }

        if (current == finish) { // the closest unprocessed node is the target, so its distance is final
            return true; // Dijkstra guarantees no shorter path can exist, we can stop early
        } 

        for (const auto& edge : graph.at(current)) { // examine every border leaving current
            string neighbor = edge.neighbor; // name of the country on the other side
            int newDistance = currentDistance + edge.distance; // total km if we reach the neighbor through current

            if (distance.find(neighbor) == distance.end() || // neighbor never reached before (treated as infinite distance) ...
                newDistance < distance[neighbor]) { // ... or this route is shorter than the best one known

                distance[neighbor] = newDistance; // relaxation: save the improved distance
                parent[neighbor] = current; // remember we got here from current (needed to reconstruct the path)

                pq.emplace(newDistance, neighbor); // push the improved candidate; the older, worse entry stays in the heap but will be skipped later
            } 
        } 
    } 

    return false; // heap emptied and finish was never popped: unreachable
} 

/*
    findBfsPaths
    Rebuilds all shortest (by border count) paths from the "parents" structure produced by bfs().
    It walks backwards from finish to start using recursion with backtracking: at each node it
    follows every parent in turn. Whenever the recursion reaches start, the path collected so far
    (which is in reverse order, finish ... start) is copied, reversed to the correct direction
    (start ... finish) and saved in allPaths.

    The shared vector "path" acts as a stack: push_back when entering a node, pop_back when leaving,
    so the same vector can be reused for every branch without copying it at every call.

    Parameters:
      current  - node being visited right now (the first call passes finish)
      start    - the node where recursion stops (the base case)
      parents  - all predecessors of each node, produced by bfs()
      path     - working stack holding the nodes from finish back to current (modified during recursion, must be empty at the first call)
      allPaths - OUTPUT: every complete path in start -> finish order
*/
void findBfsPaths(
    const string& current, // node currently being expanded
    const string& start, // stopping node
    const unordered_map<string, vector<string>>& parents, // predecessor lists from bfs()
    vector<string>& path, // shared working stack (by reference so all recursion levels use the same one)
    vector<vector<string>>& allPaths // output collection of finished paths
) { 
    path.push_back(current); // add current to the path being built (going backwards)

    if (current == start) { // base case: we walked all the way back to the start
        vector<string> completePath = path; // copy the finished (still reversed) path, because path will keep changing
        reverse(completePath.begin(), completePath.end()); // flip it so it reads start -> finish
        allPaths.push_back(completePath); // store this path in the results
    }
    else { // recursive case: keep walking backwards
        for (const string& parent : parents.at(current)) { // try each predecessor; more than one means several shortest paths branch here
            findBfsPaths(parent, start, parents, path, allPaths); // recurse into that predecessor
        } 
    } 

    path.pop_back(); // BACKTRACK: remove current so the next branch starts from a clean state
} 

/*
    findDijkstraPath
    Rebuilds the single shortest-distance path from the "parent" map produced by dijkstra().
    Starting at finish, it repeatedly jumps to the parent until it arrives at start, collecting
    the nodes in reverse order, and finally reverses the vector so it reads start -> finish.

    Parameters:
      start  - where the path begins
      finish - where the path ends (the walk begins here)
      parent - predecessor of every node on the path, produced by dijkstra()
      path   - OUTPUT: the nodes in order from start to finish (expected empty on entry)
*/
void findDijkstraPath(
    const string& start, // path begins here
    const string& finish, // walk backwards from here
    const unordered_map<string, string>& parent, // predecessor map from dijkstra()
    vector<string>& path // output path
) { 
    string current = finish; // begin at the destination

    while (current != start){ // walk backwards until we reach the start
        path.push_back(current); // save the current node
        current = parent.at(current); // jump to its predecessor (.at() because parent is const)
    } 

    path.push_back(start); // the loop stops before adding start, so add it manually
    
    reverse(path.begin(), path.end()); // the path was collected backwards, flip it to start -> finish
} 

/*
    printBfsResults
    Prints the BFS answer: the minimum number of borders crossed and every path that achieves it.
    Paths are rebuilt with findBfsPaths(), then printed as "Path N: A -> B -> C".

    Parameters:
      start    - starting country
      finish   - target country
      distance - border counts from bfs() (only distance[finish] is printed)
      parents  - predecessor lists from bfs(), used to rebuild the paths
*/
void printBfsResults(
    const string& start, // starting country
    const string& finish, // target country
    const unordered_map<string, int>& distance, // border counts computed by bfs()
    const unordered_map<string, vector<string>>& parents // shortest-path predecessors computed by bfs()
) { 
    vector<string> path; // temporary stack needed by the recursive function
    vector<vector<string>> allPaths; // will hold every shortest path (each path is a vector of country names)
    findBfsPaths(finish, start, parents, path, allPaths); // fill allPaths by walking backwards from finish to start

    cout << "BFS:" << endl << "Lowest number of borders crossed: " << distance.at(finish) << "." << endl; // header + number of borders (.at() since distance is const)

    for (size_t i = 0; i < allPaths.size(); i++) { // go through each found path; size_t matches the type returned by size()
        cout << "Path " << i + 1 << ": "; // human-friendly numbering starting at 1
        for (size_t j = 0; j < allPaths[i].size(); j++) { // go through each country on this path
            cout << allPaths[i][j]; // print the country name
            if (j < allPaths[i].size() - 1) { // is it not the last country?
                cout << " -> "; // separator only between countries, not after the last one
            } 
        } 
        cout << endl; // finish the line for this path
    } 
} 

/*
    printDijkstraResults
    Prints the Dijkstra answer: the shortest total distance in km and the route as
    "Capital (Country) -> Capital (Country) -> ...".
    The route is rebuilt with findDijkstraPath(); the capitals map is used to show each country's capital.

    Parameters:
      start    - starting country
      finish   - target country
      distance - km distances from dijkstra() (only distance[finish] is printed)
      parent   - predecessor map from dijkstra(), used to rebuild the path
      capitals - country -> capital map, for pretty printing
*/
void printDijkstraResults(
    const string& start, // starting country
    const string& finish, // target country
    const unordered_map<string, int>& distance, // km distances computed by dijkstra()
    const unordered_map<string, string>& parent, // predecessor map computed by dijkstra()
    const unordered_map<string, string>& capitals // country -> capital lookup for output formatting
) { 
    vector<string> path; // will receive the ordered list of countries
    findDijkstraPath(start, finish, parent, path); // rebuild the route from the parent map

    cout << "Dijkstra:" << endl << "Shortest distance: " << distance.at(finish) << " km." << endl << "Path: "; // header, total km and the beginning of the path line
    for (size_t i = 0; i < path.size(); i++) { // go through each country on the route
        cout << capitals.at(path[i]) << " (" << path[i] << ")"; // print "Capital (Country)"

        if (i < path.size() - 1) { // not the last element?
            cout << " -> "; // arrow between stops
        } 
    } 
    cout << endl; // end the path line
}

/*
    main
    Program entry point and the "controller" of the whole program. Steps:
      1. Load the CSV into the graph and capitals structures.
      2. Ask the user for a start and a finish (country or capital name).
      3. Validate the input and convert both to country names.
      4. Handle the trivial case where start and finish are the same place.
      5. Run BFS (fewest borders, all such paths) and print the result.
      6. Run Dijkstra (fewest kilometers, one path) and print the result.
*/
int main() {
    string start, finish; // raw text the user types for the start and the finish
    unordered_map<string, vector<Edge>> graph; // adjacency list: country -> edges to its neighbors
    unordered_map<string, string> capitals; // country -> capital
    unordered_map<string, int> bfsDistance; // BFS result: number of borders to each country
    unordered_map<string, int> dijkstraDistance; // Dijkstra result: km to each country
    unordered_map<string, vector<string>> bfsParents; // BFS result: all shortest-path predecessors
    unordered_map<string, string> dijkstraParent; // Dijkstra result: single best predecessor

    if (createGraph(graph, capitals) != 0) { // load the data; a non-zero result means the file failed to open
        return 1;
    }
 
    cout << "Choose a start country/capital: " << endl; // prompt for the start
    getline(cin, start); // read the whole line (getline instead of >> so names with spaces work)
    cout << "Choose a finish country/capital" << endl; // prompt for the finish
    getline(cin, finish); // read the whole line for the finish

    bool validStart = inputExists(graph, capitals, start); // is the start a known country or capital?
    bool validFinish = inputExists(graph, capitals, finish); // is the finish a known country or capital?

    if (!validStart || !validFinish) { // at least one of them is unknown
        cout << "Invalid input." << endl; 
        return 0; 
    }
 
    string startCountry = resolveLocation(capitals, start); // convert start to a country name
    string finishCountry = resolveLocation(capitals, finish); // convert finish to a country name
 
    if (startCountry == finishCountry) { // both inputs point to the same country
        cout << "Start and finish are the same." << endl; 
        cout << "Distance: 0" << endl; 
        cout << "Path 1: " << capitals.at(startCountry) << " (" << startCountry << ")" << endl; // print the single-node path in the same format as the normal output
        return 0; 
    } 

    cout << "Start: " << capitals.at(startCountry) << " (" << startCountry << ")" << endl; // confirm the resolved start as "Capital (Country)"
    cout << "Finish: " << capitals.at(finishCountry) << " (" << finishCountry << ")" << endl; // confirm the resolved finish

    if (bfs(graph, startCountry, finishCountry, bfsDistance, bfsParents)) { // run BFS; true means finish is reachable
        printBfsResults(startCountry, finishCountry, bfsDistance, bfsParents); // print border count and all shortest paths
    } else { cout << "No path found." << endl; } // BFS could not reach finish

    dijkstraDistance.clear(); 
    dijkstraParent.clear(); 

    if (dijkstra(graph, startCountry, finishCountry, dijkstraDistance, dijkstraParent)) { // run Dijkstra; true means finish is reachable
        printDijkstraResults(startCountry, finishCountry, dijkstraDistance, dijkstraParent, capitals); // print the km distance and the route
    } else { cout << "No path found." << endl; } // Dijkstra could not reach finish
 
    return 0; 
}