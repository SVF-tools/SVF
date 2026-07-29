---
title: "Readme"
tags:
  - svf
  - documentation
  - wiki
  - setup
  - build
  - pointer-analysis
  - value-flow
  - saber
  - llvm
  - testing
  - docker
---

<img src="./docs/images/svf_logo_2.png" width="15%"><img src="./docs/images/svf_logo_3.png" width="85%">


## News
* <b>SVF now supports [LLVM-21](https://github.com/SVF-tools/SVF/pull/1815) (Contributed by [cjsrxzdyzds](https://github.com/cjsrxzdyzds)). </b>
* <b>SVF now supports new [build system](https://github.com/SVF-tools/SVF/pull/1703) (Thank [Johannes](https://github.com/Johanmyst) for his help!). </b>
* <b> [SVF-Python](https://github.com/SVF-tools/SVF-Python) is now available, enabling developers to write static analyzers in Python by leveraging the SVF library (Contributed by [Jiawei Wang](https://github.com/bjjwwang)). </b>
* <b>New course [Software Security Analysis](https://github.com/SVF-tools/Software-Security-Analysis) for learning code analysis and verification with SVF for fun and expertise! </b>
* <b>SVF now supports LLVM-16.0.0 with opaque pointers (Contributed by [Xiao Cheng](https://github.com/jumormt)). </b>
* <b>Modernize SVF's CMake (Contributed by [Johannes](https://github.com/Johanmyst)). </b>
* <b>SVF now supports LLVM-13.0.0 (Thank [Shengjie Xu](https://github.com/xushengj) for his help!). </b>
* <b>[Object clustering](wiki/SVF.wiki/Object-Clustering.md) published in our [OOPSLA paper](https://yuleisui.github.io/publications/oopsla21.pdf) is now available in SVF </b>
* <b>[Hash-Consed Points-To Sets](wiki/SVF.wiki/Hash-Consed-Points-To-Sets.md) published in our [SAS paper](https://yuleisui.github.io/publications/sas21.pdf) is now available in SVF </b>
* <b> Learning or teaching Software Analysis? Check out [SVF-Teaching](https://github.com/SVF-tools/SVF-Teaching)! </b>
* <b>SVF now supports LLVM-12.0.0 (Thank [Xiyu Yang](https://github.com/sherlly/) for her help!). </b>
* <b>[VSFS](wiki/SVF.wiki/VSFS.md) published in our [CGO paper](https://yuleisui.github.io/publications/cgo21.pdf) is now available in SVF </b>
* <b>[TypeClone](wiki/SVF.wiki/TypeClone.md) published in our [ECOOP paper](https://yuleisui.github.io/publications/ecoop20.pdf) is now available in SVF </b>
* <b>SVF now uses a single script for its build. Just type [`source ./build.sh`](build.sh) in your terminal, that's it!</b>
* <b>SVF now supports LLVM-10.0.0! </b>
* <b>We thank [bsauce](https://github.com/bsauce) for writing a user manual of SVF ([link1](https://www.jianshu.com/p/068a08ec749c) and [link2](https://www.jianshu.com/p/777c30d4240e)) in Chinese </b>
* <b>SVF now supports LLVM-9.0.0 (Thank [Byoungyoung Lee](https://github.com/SVF-tools/SVF/issues/142) for his help!). </b>
* <b>SVF now supports a set of [field-sensitive pointer analyses](https://yuleisui.github.io/publications/sas2019a.pdf). </b>
* <b>[Use SVF as an external lib](https://github.com/SVF-tools/SVF-example) for your own project (Contributed by [Hongxu Chen](https://github.com/HongxuChen)). </b>
* <b>SVF now supports LLVM-7.0.0. </b>
* <b>SVF now supports Docker. [Try SVF in Docker](wiki/SVF.wiki/Try-SVF-in-Docker.md)! </b>
* <b>SVF now supports [LLVM-6.0.0](https://github.com/svf-tools/SVF/pull/38) (Contributed by [Jack Anthony](https://github.com/jackanth)). </b>
* <b>SVF now supports [LLVM-4.0.0](https://github.com/svf-tools/SVF/pull/23) (Contributed by Jared Carlson. Thank [Jared](https://github.com/jcarlson23) and [Will](https://github.com/dtzWill) for their in-depth [discussions](https://github.com/svf-tools/SVF/pull/18) about updating SVF!) </b>
* <b>SVF now supports analysis for C++ programs.</b>
<br />

## Documentation

<br />

<b>SVF</b> is a static value-flow analysis tool for LLVM-based languages. <b>SVF</b> ([CC'16](https://yuleisui.github.io/publications/cc16.pdf)) is able to perform
* [AE](svf/include/AE) (<b>abstract execution</b>): cross-domain execution ([ICSE'24](https://yuleisui.github.io/publications/icse24a.pdf)), recursion analysis ([ECOOP'25](https://yuleisui.github.io/publications/ecoop25.pdf)) typestate analysis ([FSE'24](https://yuleisui.github.io/publications/fse24a.pdf));
* [WPA](svf/include/WPA) (<b>whole program analysis</b>): field-sensitive ([SAS'19](https://yuleisui.github.io/publications/sas2019a.pdf)), flow-sensitive ([CGO'21](https://yuleisui.github.io/publications/cgo21.pdf), [OOPSLA'21](https://yuleisui.github.io/publications/oopsla21.pdf)) analysis;
* [DDA](svf/include/DDA) (<b>demand-driven analysis</b>): flow-sensitive, context-sensitive points-to analysis ([FSE'16](https://yuleisui.github.io/publications/fse16.pdf), [TSE'18](https://yuleisui.github.io/publications/tse18.pdf));
* [MSSA](svf/include/MSSA) (<b>memory SSA form construction</b>): memory regions, side-effects, SSA form ([JSS'18](https://yuleisui.github.io/publications/jss18.pdf));
* [SABER](svf/include/SABER) (<b>memory error checking</b>): memory leaks and double-frees ([ISSTA'12](https://yuleisui.github.io/publications/issta12.pdf), [TSE'14](https://yuleisui.github.io/publications/tse14.pdf), [ICSE'18](https://yuleisui.github.io/publications/icse18a.pdf));
* [MTA](svf/include/MTA) (<b>analysis of multithreaded programs</b>): value-flows for multithreaded programs ([CGO'16](https://yuleisui.github.io/publications/cgo16.pdf));
* [CFL](svf/include/CFL) (<b>context-free-reachability analysis</b>): standard CFL solver, graph and grammar ([OOPSLA'22](https://yuleisui.github.io/publications/oopsla22.pdf), [PLDI'23](https://yuleisui.github.io/publications/pldi23.pdf));
* [SVFIR](svf/include/SVFIR) and [MemoryModel](svf/include/MemoryModel) (<b>SVFIR</b>): SVFIR, memory abstraction and points-to data structure ([SAS'21](https://yuleisui.github.io/publications/sas21.pdf));
* [Graphs](svf/include/Graphs): <b> generating a variety of graphs</b>, including call graph, ICFG, class hierarchy graph, constraint graph, value-flow graph for static analyses and code embedding ([OOPSLA'20](https://yuleisui.github.io/publications/oopsla20.pdf), [TOSEM'21](https://yuleisui.github.io/publications/tosem21.pdf))

<p>We release the SVF source code with the hope of benefiting the open-source community. You are kindly requested to acknowledge usage of the tool by referring to or citing relevant publications above. </p>

<b>SVF</b>'s doxygen document is available [here](https://svf-tools.github.io/SVF-doxygen/html).

<br />

| About SVF       | Setup  Guide         | User Guide  | Developer Guide  |
| ------------- |:-------------:| -----:|-----:|
| ![About](docs/images/help.png)| ![Setup](docs/images/tools.png)  | ![User](docs/images/users.png)  |  ![Developer](docs/images/database.png) 
| Introducing SVF -- [what it does](wiki/SVF.wiki/About.md#what-is-svf) and [how we design it](wiki/SVF.wiki/SVF-Design.md#svf-design)      | A step by step [setup guide](wiki/SVF.wiki/Setup-Guide.md#getting-started) to build SVF | Command-line options to [run SVF](wiki/SVF.wiki/User-Guide.md#quick-start), get [analysis outputs](wiki/SVF.wiki/User-Guide.md#analysis-outputs), and test SVF with [an example](wiki/SVF.wiki/Analyze-a-Simple-C-Program.md) or [PTABen](https://github.com/SVF-tools/PTABen) | Detailed [technical documentation](wiki/SVF.wiki/Technical-documentation.md) and how to [write your own analyses](wiki/SVF.wiki/Write-your-own-analysis-in-SVF.md) in SVF or [use SVF as a lib](https://github.com/SVF-tools/SVF-example) for your tool, and the [course](https://github.com/SVF-tools/Software-Security-Analysis) on SVF  |

<br />


