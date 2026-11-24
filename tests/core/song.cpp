// Core tests: song folders, listing and JSON round trip.
#include "test.hpp"

  // folders: saved only when set, standard ones first in course order, custom A-Z, unsorted last
TEST(folders) {
    Song s = importAbc(JIG).song;
    Song f = s;
    assert(!toJson(f).contains("folder"));
    f.folder = "Intermediate";
    assert(fromJson(toJson(f)).folder == "Intermediate");
    std::vector<std::string> fs_ = {"", "Zebra", "Advanced", "Celtic", "Lessons", "Beginner"};
    std::sort(fs_.begin(), fs_.end(), folderBefore);
    assert((fs_ == std::vector<std::string>{"Lessons", "Beginner", "Advanced", "Celtic", "Zebra", ""}));
}

  // listing and JSON round trip through disk
TEST(listing_and_json_round_trip) {
  Song s = importAbc(JIG).song;
  s.id = "test-jig";
  saveSong(s);
  auto back = loadSong("test-jig");
  assert(back && toJson(*back) == toJson(s));
  assert(listing(s).find("XXO OOO") != std::string::npos);
  assert(!loadSong("../etc"));
  assert(listSongs() == std::vector<std::string>{"test-jig"});
}
