#include <Util/CommandLine.h>

std::map<std::string, OptionBase *> OptionBase::options;
std::vector<std::string> OptionBase::helpNames = { "help", "h", "-help" };
