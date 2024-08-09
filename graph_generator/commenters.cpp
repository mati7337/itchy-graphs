#include "commenters.h"

#include <iostream>
#include <filesystem>
#include <stdexcept>
#include <fstream>
#include <regex>

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

namespace fs = std::filesystem;
namespace rj = rapidjson;

static const std::map<std::string,
                      Commenters::Similarity_method> method_from_str_map = {
    {"jaccard", Commenters::JACCARD},
    {"overlap_coefficient", Commenters::OVERLAP_COEFFICIENT}
};

Commenters::Similarity_method Commenters::method_from_str(const std::string& method_str)
{
    return method_from_str_map.find(method_str)->second;
}

void Commenters::add_commenter(const char* game, const char* commenter)
{
    int commenter_id;
    if (this->commenter_names.find(commenter) == this->commenter_names.end())
    {
        commenter_id = this->commenter_names.size();
        this->commenter_names[commenter] = commenter_id;
    } else
    {
        commenter_id = this->commenter_names[commenter];
    }

    std::unordered_set<int>& game_commenters = this->commenters[game];

    game_commenters.insert(commenter_id);
}

struct Game Commenters::parse_game_url(const std::string& game_url)
{
    const std::regex url_regex("://([^./]*)\\.itch\\.io/([^/]*)");

    std::smatch url_match;
    std::regex_search(game_url, url_match, url_regex);

    if (url_match.size() != 3)
    {
        throw std::invalid_argument("Invalid game url");
    }

    struct Game game;
    game.author = url_match[1];
    game.name = url_match[2];
    return game;
}

void Commenters::load_commenters_users_json(const char* path)
{
    std::ifstream ifs(path);
    std::string raw_json((std::istreambuf_iterator<char>(ifs)),
            std::istreambuf_iterator<char>());

    rj::Document document;
    document.Parse(raw_json.c_str());
    if (document.HasParseError())
    {
        throw std::invalid_argument(rj::GetParseError_En(document.GetParseError()));
    }

    assert(document.IsObject());

    assert(document.HasMember("user"));
    assert(document["user"].IsString());

    std::string user = document["user"].GetString();

    assert(document.HasMember("comments"));
    assert(document["comments"].IsArray());

    for (auto& comment : document["comments"].GetArray())
    {
        assert(comment.IsObject());

        assert(comment.HasMember("comments_url"));
        assert(comment["comments_url"].IsString());

        std::string comments_url = comment["comments_url"].GetString();

        // Not a game
        if (!comments_url.ends_with("/comments"))
            continue;

        struct Game game = this->parse_game_url(comments_url);

        std::string game_str = std::string(game.author)
                               + ".itch.io/"
                               + std::string(game.name);

        this->add_commenter(game_str.c_str(), user.c_str());
    }
}

void Commenters::load_commenters_games_json(const char* path)
{
    std::ifstream ifs(path);
    std::string raw_json((std::istreambuf_iterator<char>(ifs)),
            std::istreambuf_iterator<char>());

    rj::Document document;
    document.Parse(raw_json.c_str());
    if (document.HasParseError())
    {
        throw std::invalid_argument(rj::GetParseError_En(document.GetParseError()));
    }

    assert(document.IsObject());

    assert(document.HasMember("comments"));
    assert(document["comments"].IsArray());

    assert(document.HasMember("game_author"));
    assert(document["game_author"].IsString());

    std::string game_author = document["game_author"].GetString();

    assert(document.HasMember("game_name"));
    assert(document["game_name"].IsString());

    std::string game_name = document["game_name"].GetString();

    std::string game_str = game_author + ".itch.io/" + game_name;

    for (auto& comment : document["comments"].GetArray())
    {
        assert(comment.IsObject());

        assert(comment.HasMember("post_author_id"));
        assert(comment["post_author_id"].IsString());

        std::string user = comment["post_author_id"].GetString();

        this->add_commenter(game_str.c_str(), user.c_str());
    }
}

void Commenters::load_commenters(const char* path)
{
    // Loads comments both from user pages and game pages
    const std::string users_path = std::string(path) + "/users";
    const std::string games_path = std::string(path) + "/games";

    // Sanity check
    if (!fs::is_directory(users_path))
    {
        throw std::invalid_argument("No users directory in input path");
    }
    if (!fs::is_directory(games_path))
    {
        throw std::invalid_argument("No games directory in input path");
    }

    // Games
    std::cerr << "Loading comments from game pages\n";
    for (const fs::directory_entry& dir_entry :
            fs::recursive_directory_iterator(games_path))
    {
        if (!dir_entry.is_regular_file())
            continue;

        this->load_commenters_games_json(dir_entry.path().c_str());
    }

    // Users
    std::cerr << "Loading comments from user pages\n";
    for (const fs::directory_entry& dir_entry :
            fs::recursive_directory_iterator(users_path))
    {
        if (!dir_entry.is_regular_file())
            continue;

        this->load_commenters_users_json(dir_entry.path().c_str());
    }
}

int Commenters::commenters_intersection(std::unordered_set<int>& game_a,
                                        std::unordered_set<int>& game_b)
{
    // This seems to be the fastest way (at least for my data) to count
    // intersecting elements of 2 sets.
    //
    // Looping through the smaller set and checking if the bigger set
    // contains them seems to take 1.4 times faster than ignoring sizes
    // and looping through game_a
    //
    // For this use case unordered_set is way faster than set because
    // searching in set is O(log n) but in unordered_set it's O(1)

    int counter = 0;

    std::unordered_set<int>* game_big;
    std::unordered_set<int>* game_small;

    if (game_a.size() > game_b.size())
    {
        game_big = &game_a;
        game_small = &game_b;
    }
    else
    {
        game_big = &game_b;
        game_small = &game_a;
    }

    for (const int& commenter : *game_small)
    {
        if (game_big->contains(commenter))
        {
            counter ++;
        }
    }

    return counter;
}

void Commenters::create_graph(int node_threshold,
                              float edge_threshold,
                              Similarity_method method)
{
    // Creates and prints a graph to stdout
    std::cout << "graph itch {\n";

    // Helper vector to filter out games that have too few commenters
    std::vector<std::string> games_to_consider;

    for(const auto& [game, commenters] : this->commenters)
    {
        if (commenters.size() < node_threshold)
            continue;

        // Double-quotes can be escaped using \", but game names shouldn't
        // contain them anyway, so just throw an exception
        if (game.find('"') != std::string::npos)
            throw std::invalid_argument("Game name contains a double-quote character!");

        games_to_consider.push_back(game);
    }

    size_t total_inner_iters = games_to_consider.size()
                               * (games_to_consider.size()-1)
                               / 2;
    size_t inner_iters = 0;

    for (int i = 0; i < games_to_consider.size(); i++)
    {
        for (int j = i+1; j < games_to_consider.size(); j++)
        {
            std::string& game_i = games_to_consider[i];
            std::string& game_j = games_to_consider[j];

            float progress = (float)inner_iters / total_inner_iters * 100.0f;
            if (inner_iters % 500000 == 0)
                std::cerr << "Progress: " << progress << "%\n";
            inner_iters++;

            int set_intersection = this->commenters_intersection(
                    this->commenters[game_i],
                    this->commenters[game_j]);

            float weight;
            if (method == Similarity_method::JACCARD)
            {
                int set_union = this->commenters[game_i].size()
                                + this->commenters[game_j].size()
                                - set_intersection;
                weight = (float)set_intersection / set_union;
            } else if (method == Similarity_method::OVERLAP_COEFFICIENT)
            {
                int min_commenters = std::min(this->commenters[game_i].size(),
                                              this->commenters[game_j].size());
                weight = (float)set_intersection / min_commenters;
            }

            if (weight < edge_threshold)
                continue;

            std::cout << "\t\"" << game_i << "\" -- \"" << game_j << "\""
                      << "[weight=" << weight << "]\n";
        }
    }

    std::cout << "}\n";
}
