#pragma once

#include <QStringList>

// Commands that need no display, for the terminal and for agents:
//   focus daemon | status | list | start | pause | resume | end | skill install
// Each takes --json where it prints, exits 0 or 1, and writes errors to stderr.
namespace Cli {

// True when the arguments name a command-line command rather than the window.
bool handles(const QStringList &arguments);
int run(const QStringList &arguments);

} // namespace Cli
