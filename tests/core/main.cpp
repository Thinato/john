// Runs the core tests: john-tests [name...]
#include "test.hpp"

fs::path home;

std::vector<Test>& tests() {
  static std::vector<Test> all;
  return all;
}

int main(int argc, char** argv) {
  for (auto& t : tests()) {
    if (argc > 1 && std::find(argv + 1, argv + argc, std::string(t.name)) == argv + argc) continue;
    home = fs::temp_directory_path() / ("john-test-" + std::to_string(std::random_device{}()));
    setenv("JOHN_HOME", home.string().c_str(), 1);
    reloadInstruments();  // the catalog cache may still hold the previous test's custom instruments
    t.run();
    fs::remove_all(home);
  }
  puts("core tests passed");
}
