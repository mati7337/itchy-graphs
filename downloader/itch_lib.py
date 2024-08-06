import requests
import traceback
import time
import json
import re
import time
from bs4 import BeautifulSoup

class Itch:
    def __init__(self, session=requests.Session(), timeout=10, sleep=1):
        self.sess = session
        self.timeout = timeout
        self.sleep = sleep

    def _get(self, url, params=None):
        while 1:
            try:
                rq = self.sess.get(url, params=params, timeout=self.timeout)
                break
            except (requests.exceptions.ConnectTimeout,
                    requests.exceptions.ConnectionError,
                    requests.exceptions.ReadTimeout):
                traceback.print_exc()
                print("Retrying in 60s...")
                time.sleep(60) # TODO: make configurable
                continue

        rq.raise_for_status()

        #TODO check if it's using requests-cache or not
        if not rq.from_cache:
            print("\033[31mNEW\033[0m")
            time.sleep(self.sleep)

        # For some reason the requests lib guesses some other encoding
        return rq.content.decode("utf-8")

    def get_game_url(self, author, name):
        return f"https://{author}.itch.io/{name}"

    def get_profile_url(self, profile):
        return f"https://itch.io/profile/{profile}"

    def parse_community_post(self, bs_post):
        # bs_post: div with community_post class
        post = json.loads(bs_post["data-post"])
        post["post_author_url"] = bs_post.select_one(".post_author").a["href"]
        post["post_author_id"] = post["post_author_url"].split("/")[-1]
        post["post_author"] = bs_post.select_one(".post_author").a.text
        post["post_date"] = bs_post.select_one(".post_date")["title"]

        for vote_type in ["upvotes", "downvotes"]:
            if bs_votes := bs_post.select_one(f".{vote_type}"):
                #votes_filtered = "".join(filter(lambda x: x.isdigit(), bs_votes.text))
                votes_str = bs_votes.text[2:-1] # "(+n)" -> "n"
                post[vote_type] = int(votes_str)

        post["body"] = bs_post.select_one(".post_body").text

        return post

    def parse_community_post_list(self, bs_post_list):
        # bs_: div with community_post class
        posts = []

        for bs_post in bs_post_list.children:
            post_classes = bs_post.get("class", [])
            if "community_post" in post_classes:
                if "is_deleted" in post_classes:
                    # Deleted posts screw up the parsing
                    # Downloading them is kinda useless anyways so just skip them
                    continue
                if "is_suspended" in post_classes:
                    # Comments by suspended users still contain the body and user id
                    # TODO: still download comment's content and maybe somehow look up
                    # the username by id?
                    continue

                post = self.parse_community_post(bs_post)

                posts.append(post)
            #TODO: replies

        return posts

    def get_game_comments_page(self, author, name, before=None, after=None):
        # Itch.io displays 40 comments before/after a given comment number
        game_url = self.get_game_url(author, name)

        params = {}
        if before != None:
            params["before"] = before
        elif after != None: # Specifying both is dumb
            params["after"] = after

        html = self._get(f"{game_url}/comments", params=params)

        bs = BeautifulSoup(html, "html.parser")
        bs_posts = bs.select_one(".community_post_list_widget")

        output = {
            "comments": self.parse_community_post_list(bs_posts)
        }

        # Get info about the page
        bs_page_label = bs.select_one(".page_label")

        if bs_page_label:
            # Remove commas from numbers so it's easier to parse
            page_label_text = bs_page_label.text.replace(",", "")
            page_label_match = re.search("([0-9]*) to ([0-9]*) of ([0-9]*)",
                                         page_label_text)

            output["from"] = int(page_label_match.group(1))
            output["to"] = int(page_label_match.group(2))
            output["total"] = int(page_label_match.group(3))


        return output

    def parse_topic_post_row(self, bs_post_row):
        # Post rows are used on user profiles in the recent posts section
        # They additionally contain info where it was posted
        bs_topic = bs_post_row.select_one(".topic_title")
        bs_post_list = bs_post_row.select_one(".community_post_list_widget")

        post_list = self.parse_community_post_list(bs_post_list)

        #TODO better handling of non game comments
        comments_url = bs_topic.a["href"]

        return {
            "posts": post_list,
            "comments_url": comments_url
        }

    def get_profile_comments_page(self, profile):
        profile_url = self.get_profile_url(profile)

        html = self._get(profile_url)

        bs = BeautifulSoup(html, "lxml")
        bs_recent = bs.select_one(".recent_posts")

        comments = []

        for bs_post_row in bs_recent.children:
            if "topic_post_row" in bs_post_row.get("class", []):
                topic_post = self.parse_topic_post_row(bs_post_row)
                comments.append(topic_post)

        return comments
