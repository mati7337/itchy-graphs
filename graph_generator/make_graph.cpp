#include <iostream>
#include <chrono>
#include <rapidjson/document.h>
#include "commenters.h"

namespace chrono = std::chrono;

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage:\n";
        std::cerr << " " << argv[0] << " ITCH_DOWNLOAD_PATH\n";
        return -1;
    }

    std::string itch_path = argv[1];

    Commenters commenters;

    auto time_start = chrono::steady_clock::now();
    std::cerr << "Loading commenters\n";
    commenters.load_commenters(itch_path.c_str());

    auto time_comments_loaded = chrono::steady_clock::now();
    std::cerr << "Creating graph\n";
    commenters.create_graph();

    auto time_end = chrono::steady_clock::now();

    std::cerr << "Loading comments: "
              << (chrono::duration_cast<chrono::milliseconds>(
                          time_comments_loaded - time_start).count() / 1000.0)
              << "s\n";

    std::cerr << "Calculating edges: "
              << (chrono::duration_cast<chrono::milliseconds>(
                          time_end - time_comments_loaded).count() / 1000.0)
              << "s\n";
}
