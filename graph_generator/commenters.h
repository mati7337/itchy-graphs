#pragma once

#include <unordered_set>
#include <map>
#include <string>

struct Game
{
    std::string author;
    std::string name;
};

class Commenters
{
    public:
        void load_commenters(const char* path);
        struct Game parse_game_url(const std::string& game_url);
        void add_commenter(const char* game, const char* commenter);
        void create_graph();
        int commenters_intersection(std::unordered_set<int>& game_a,
                                    std::unordered_set<int>& game_b);

    private:
        void load_commenters_users_json(const char* path);
        void load_commenters_games_json(const char* path);
        std::map<std::string, std::unordered_set<int>> commenters;
        std::map<std::string, int> commenter_names;
};
