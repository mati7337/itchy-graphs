#include <iostream>
#include <chrono>
#include <rapidjson/document.h>
#include <argparse/argparse.hpp>
#include "commenters.h"

namespace chrono = std::chrono;

int main(int argc, char* argv[])
{
    argparse::ArgumentParser program(
            "make_graph", "", argparse::default_arguments::help);

    program.add_argument("input")
           .help("The itch.io comments download path");
    program.add_argument("-e", "--edge-threshold")
           .help("Edge threshold, edges with weights lower than this will be ignored")
           .default_value(0)
           .scan<'g', float>();
    program.add_argument("-n", "--node-threshold")
           .help("Node threshold, nodes (games) with less than this amount of"
                 "commenters will be ignored")
           .default_value(0)
           .scan<'i', int>();
    program.add_argument("-m", "--method")
           .help("Methods to calculate the similarity:\n"
                 "jaccard - intersection(A, B) / sum(A, B)\n"
                 "overlap_coefficient - intersection(A, B) / size(A, B)")
           .default_value(std::string{"jaccard"})
           .choices("jaccard", "overlap_coefficient");

    try
    {
        program.parse_args(argc, argv);
    } catch (const std::exception& err)
    {
        std::cerr << err.what() << "\n";
        std::cerr << program;
        return 1;
    }

    std::string itch_path = program.get<std::string>("input");

    Commenters commenters;

    auto time_start = chrono::steady_clock::now();
    std::cerr << "Loading commenters\n";
    commenters.load_commenters(itch_path.c_str());

    auto time_comments_loaded = chrono::steady_clock::now();
    std::cerr << "Creating graph\n";
    commenters.create_graph(
            program.get<int>("--node-threshold"),
            program.get<float>("--edge-threshold"),
            commenters.method_from_str(program.get<std::string>("--method"))
    );

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
