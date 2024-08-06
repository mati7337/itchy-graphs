import itch_lib
import os
import json
import traceback
import requests
import requests_cache
import re

class Itch_downloader:
    def __init__(self):
        self.sess = requests_cache.CachedSession('itch_cache')

        self.output_dir = "output"

        self.itch = itch_lib.Itch(session=self.sess)

        # Downloading user comments gives more games
        self.users_queue = []
        # Downloading game comments gives more users
        self.games_queue = ["https://chasefox.itch.io/carrot-the-first-seed"]

        # Keep track of all known nodes to prevent duplicates
        self.users_known = set(self.users_queue)
        self.games_known = set(self.games_queue)

    def get_all_game_comments(self, author, name):
        # Download comments from all pages from a given game
        comments = []
        total_comments = 1 # it's updated later
        current_after = 0

        while len(comments) < total_comments:
            print(f"after={current_after}")
            print(author, name)
            page = self.itch.get_game_comments_page(author, name, after=current_after)

            if not "total" in page:
                # Happens when there's only a single page or page is out of bounds
                # This also happens when comments don't start from id=1, but from a
                # later id e.g.
                # https://rewindgames.itch.io/tanuki-sunset/comments?after=x
                comments += page["comments"]
                return comments

            current_after += 1 + page["to"] - page["from"]
            comments += page["comments"]
            total_comments = page["total"]

            print(f"{page['from']}-{page['to']}/{page['total']}")

        return comments

    def game_info_from_url(self, url):
        match = re.search("://([^./]*)\\.itch\\.io/([^/]*)", url)

        if match:
            return match.group(1), match.group(2)
        else:
            return None, None

    def download_game_comments(self):
        for game_i, game in enumerate(self.games_queue):
            game_author, game_name = self.game_info_from_url(game)
            print(f"Downloading game {game_author}/{game_name} "
                  f"({game_i+1}/{len(self.games_queue)})")

            try:
                comments = self.get_all_game_comments(game_author, game_name)
            except requests.exceptions.HTTPError as e:
                if e.response.status_code == 404:
                    continue
                else:
                    raise e from None

            print(f"{len(comments)} comments")

            game_raw = f"{game_author}_{game_name}"
            game_safe = game_raw.replace("/", "") \
                                .replace("\\", "") \
                                .replace(".", "_dot_")
            with open(f"{self.output_dir}/games/{game_safe}.json", "w") as fl:
                json.dump({
                    "game_name": game_name,
                    "game_author": game_author,
                    "comments": comments}, fl)

            for comment in comments:
                comment_author = comment["post_author_id"]

                if not comment_author in self.users_known:
                    self.users_queue.append(comment_author)
                    self.users_known.add(comment_author)

    def download_user_comments(self):
        for user_i, user in enumerate(self.users_queue):
            print(f"Downloading user {user} ({user_i+1}/{len(self.users_queue)})")

            try:
                comments = self.itch.get_profile_comments_page(user)
            except requests.exceptions.HTTPError as e:
                if e.response.status_code == 404:
                    continue
                else:
                    raise e from None

            user_safe = user.replace("/", "").replace("\\", "").replace(".", "_dot_")
            with open(f"{self.output_dir}/users/{user_safe}.json", "w") as fl:
                json.dump({"user": user, "comments": comments}, fl)

            for comment in comments:
                url = comment["comments_url"]

                # TODO: add a better way to check if it's a game, but this works for now
                if not url.endswith("/comments"):
                    # URLs to game comments seem to always end with /comments
                    continue

                game_author, game_name = self.game_info_from_url(url)

                if not (game_author and game_name):
                    # If the regex doesn't find both then it's safe to assume
                    # that it's not a game
                    continue

                game_url = self.itch.get_game_url(game_author, game_name)

                if not game_url in self.games_known:
                    self.games_queue.append(game_url)
                    self.games_known.add(game_url)

    def download_commenters(self):
        max_recursion = 3
        recursion = 1

        os.makedirs(f"{self.output_dir}/games", exist_ok=True)
        os.makedirs(f"{self.output_dir}/users", exist_ok=True)

        while (max_recursion == None) or (recursion <= max_recursion):
            print(f"Recursion no. {recursion}")
            print()

            # Queues exhausted
            if len(self.games_queue) + len(self.users_queue) == 0:
                print("Queues exhausted! Exiting!")
                break

            # First get user comments from known games
            self.download_game_comments()

            # Going through games_queue only adds users so can clean it in one big swoop
            self.games_queue.clear()

            print()

            # Then check which other games they commented on
            self.download_user_comments()

            # Same as with games_queue
            self.users_queue.clear()

            recursion += 1

def main():
    downloader = Itch_downloader()
    downloader.download_commenters()

if __name__ == "__main__":
    main()
