//===- Options.h -- Command line options ------------------------//

#ifndef OPTIONS_H_
#define OPTIONS_H_

namespace SVF
{

/// Carries around command line options. To be used a singleton.
class Options
{
public:
    /// Return (singleton) Options instance with all options set.
    static const Options *get(void);

private:
    /// Fills all fields from llvm::cl::opt.
    Options(void);

public:

private:
    static const Options *options;
};

};  // namespace SVF

#endif  // ifdef OPTIONS_H_
