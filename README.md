itchy-graphs
=========

This project aims to visualize communities on itch.io by looking at the overlap of players between different games.

It does this by scraping game comments and then calculating the intersection of commenters between all game pairs. Each game is represented as a node, and edge weights are how much commenters from one game overlap with the other. It uses the fact that if multiple games share a large part of the playerbase then they're probably similar.

It's split into 2 parts:

### downloader

Downloader is a python program that downloads comments left under games and saves them to .json files.
It follows this algorithm:

- Discover new users by downloading game comments
- Discover new games by downloading users comment pages
- Repeat the above steps some specified amount of times

This quickly causes a snowball effect and by the 3rd iteration there's hundreds of thousands of users comment pages.

The output folder contains 2 subfolders:

- games/ - comments left under a given game
- users/ - comments left by a given user

User pages don't contain every comment left by a user, they only show ~50 newest comments.

Requests are cached using requests-cache.

Usage:

```shell
cd downloader
python -m venv venv
source venv/bin/activate
pip install -r requirements.txt
python itch_downloader.py
```

### graph_generator

Graph generator is a C++ program used to generate graphs from the comments downloaded by the downloader. It outputs the graph to stdout in the [graphviz dot language](https://graphviz.org/doc/info/lang.html). Weight of an edge is the [Jaccard index](https://en.wikipedia.org/wiki/Jaccard_index) between two sets containing commenters from these games.

RapidJSON library is used for loading the json files.

Usage:

```shell
mkdir graph_generator/build
cd graph_generator/build
cmake ..
cmake --build .
./make_graph ITCH_DOWNLOAD_PATH > GRAPH_OUTPUT_PATH
```

The output can then be used by e.g. [gephi](https://gephi.org/).
