#include "application/application.hpp"
#include "program.hpp"
using namespace enchantment_tweaks::app;

int main() {
  try {

    Backend backend;

    exitProgram(ExitStatus::OK);

  } catch (const TerminationException &e) {
    std::cout << "Program terminated due to request: " << e.what();
    std::exit(e.getExitCode());
  }

  return 0;
}