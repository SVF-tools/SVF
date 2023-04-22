## News
* <b>SVF now supports LLVM-13.0.0 (Thank [Shengjie Xu](https://github.com/xushengj) for his help!). </b>
* <b>[Object clustering](https://github.com/SVF-tools/SVF/wiki/Object-Clustering) published in our [OOPSLA paper](https://yuleisui.github.io/publications/oopsla21.pdf) is now available in SVF </b>
* <b>[Hash-Consed Points-To Sets](https://github.com/SVF-tools/SVF/wiki/Hash-Consed-Points-To-Sets) published in our [SAS paper](https://yuleisui.github.io/publications/sas21.pdf) is now available in SVF </b>
* <b> Learning or teaching Software Analysis? Check out [SVF-Teaching](https://github.com/SVF-tools/SVF-Teaching)! </b>
* <b>SVF now supports LLVM-12.0.0 (Thank [Xiyu Yang](https://github.com/sherlly/) for her help!). </b>
* <b>[VSFS](https://github.com/SVF-tools/SVF/wiki/VSFS) published in our [CGO paper](https://yuleisui.github.io/publications/cgo21.pdf) is now available in SVF </b>
* <b>[TypeClone](https://github.com/SVF-tools/SVF/wiki/TypeClone) published in our [ECOOP paper](https://yuleisui.github.io/publications/ecoop20.pdf) is now available in SVF </b>
* <b>SVF now uses a single script for its build. Just type [`source ./build.sh`](https://github.com/SVF-tools/SVF/blob/master/build.sh) in your terminal, that's it!</b>
* <b>SVF now supports LLVM-10.0.0! </b>
* <b>We thank [bsauce](https://github.com/bsauce) for writing a user manual of SVF ([link1](https://www.jianshu.com/p/068a08ec749c) and [link2](https://www.jianshu.com/p/777c30d4240e)) in Chinese </b>
* <b>SVF now supports LLVM-9.0.0 (Thank [Byoungyoung Lee](https://github.com/SVF-tools/SVF/issues/142) for his help!). </b>
* <b>SVF now supports a set of [field-sensitive pointer analyses](https://yuleisui.github.io/publications/sas2019a.pdf). </b>
* <b>[Use SVF as an external lib](https://github.com/SVF-tools/SVF-example) for your own project (Contributed by [Hongxu Chen](https://github.com/HongxuChen)). </b>
* <b>SVF now supports LLVM-7.0.0. </b>
* <b>SVF now supports Docker. [Try SVF in Docker](https://github.com/SVF-tools/SVF/wiki/Try-SVF-in-Docker)! </b>
* <b>SVF now supports [LLVM-6.0.0](https://github.com/svf-tools/SVF/pull/38) (Contributed by [Jack Anthony](https://github.com/jackanth)). </b>
* <b>SVF now supports [LLVM-4.0.0](https://github.com/svf-tools/SVF/pull/23) (Contributed by Jared Carlson. Thank [Jared](https://github.com/jcarlson23) and [Will](https://github.com/dtzWill) for their in-depth [discussions](https://github.com/svf-tools/SVF/pull/18) about updating SVF!) </b>
* <b>SVF now supports analysis for C++ programs.</b>
<br />

## Documentation

<b>SVF</b> is a static value-flow analysis tool for LLVM-based languages. <b>SVF</b> ([CC'16](https://yuleisui.github.io/publications/cc16.pdf)) is able to perform
* [WPA](https://github.com/SVF-tools/SVF/tree/master/svf/include/WPA) (<b>whole program analysis</b>): field-sensitive ([SAS'19](https://yuleisui.github.io/publications/sas2019a.pdf)), flow-sensitive ([CGO'21](https://yuleisui.github.io/publications/cgo21.pdf), [OOPSLA'21](https://yuleisui.github.io/publications/oopsla21.pdf)) analysis;
* [DDA](https://github.com/SVF-tools/SVF/tree/master/svf/include/DDA) (<b>demand-driven analysis</b>): flow-sensitive, context-sensitive points-to analysis ([FSE'16](https://yuleisui.github.io/publications/fse16.pdf), [TSE'18](https://yuleisui.github.io/publications/tse18.pdf));
* [MSSA](https://github.com/SVF-tools/SVF/tree/master/svf/include/MSSA) (<b>memory SSA form construction</b>): memory regions, side-effects, SSA form ([JSS'18](https://yuleisui.github.io/publications/jss18.pdf));
* [SABER](https://github.com/SVF-tools/SVF/tree/master/svf/include/SABER) (<b>memory error checking</b>): memory leaks and double-frees ([ISSTA'12](https://yuleisui.github.io/publications/issta12.pdf), [TSE'14](https://yuleisui.github.io/publications/tse14.pdf), [ICSE'18](https://yuleisui.github.io/publications/icse18a.pdf));
* [MTA](https://github.com/SVF-tools/SVF/tree/master/svf/include/MTA) (<b>analysis of mutithreaded programs</b>): value-flows for multithreaded programs ([CGO'16](https://yuleisui.github.io/publications/cgo16.pdf));
* [CFL](https://github.com/SVF-tools/SVF/tree/master/svf/include/CFL) (<b>context-free-reachability analysis</b>): standard CFL solver, graph and grammar ([OOPSLA'22](https://yuleisui.github.io/publications/oopsla22.pdf), [PLDI'23](https://yuleisui.github.io/publications/pldi23.pdf));
* [SVFIR](https://github.com/SVF-tools/SVF/tree/master/svf/include/SVFIR) and [MemoryModel](https://github.com/SVF-tools/SVF/tree/master/svf/include/MemoryModel) (<b>SVFIR</b>): SVFIR, memory abstraction and points-to data structure ([SAS'21](https://yuleisui.github.io/publications/sas21.pdf));
* [Graphs](https://github.com/SVF-tools/SVF/tree/master/svf/include/Graphs): <b> generating a variety of graphs</b>, including call graph, ICFG, class hirachary graph, constraint graph, value-flow graph for static analyses and code embedding ([OOPSLA'20](https://yuleisui.github.io/publications/oopsla20.pdf), [TOSEM'21](https://yuleisui.github.io/publications/tosem21.pdf))

<b>SVF</b>'s doxygen document is available [here](https://svf-tools.github.io/SVF-doxygen/html).

<br />

| About SVF       | Setup  Guide         | User Guide  | Developer Guide  |
| ------------- |:-------------:| -----:|-----:|
| ![About](https://github.com/svf-tools/SVF/blob/master/docs/images/help.png?raw=true)| ![Setup](https://github.com/svf-tools/SVF/blob/master/docs/images/tools.png?raw=true)  | ![User](https://github.com/svf-tools/SVF/blob/master/docs/images/users.png?raw=true)  |  ![Developer](https://github.com/svf-tools/SVF/blob/master/docs/images/database.png?raw=true) 
| Introducing SVF -- [what it does](https://github.com/svf-tools/SVF/wiki/About#what-is-svf) and [how we design it](https://github.com/svf-tools/SVF/wiki/SVF-Design#svf-design)      | A step by step [setup guide](https://github.com/svf-tools/SVF/wiki/Setup-Guide#getting-started) to build SVF | Command-line options to [run SVF](https://github.com/svf-tools/SVF/wiki/User-Guide#quick-start), get [analysis outputs](https://github.com/svf-tools/SVF/wiki/User-Guide#analysis-outputs), and test SVF with [an example](https://github.com/svf-tools/SVF/wiki/Analyze-a-Simple-C-Program) or [PTABen](https://github.com/SVF-tools/PTABen) | Detailed [technical documentation](https://github.com/svf-tools/SVF/wiki/Technical-documentation) and how to [write your own analyses](https://github.com/svf-tools/SVF/wiki/Write-your-own-analysis-in-SVF) in SVF or [use SVF as a lib](https://github.com/SVF-tools/SVF-example) for your tool  |

<br />

#### We are looking for self-motivated PhD students and welcome industry collaboration to improve SVF (Please contact y.sui@unsw.edu.au)

<br />
<p>We release SVF source code in the hope of benefiting others. You are kindly asked to acknowledge usage of the tool by citing some of our publications listed http://svf-tools.github.io/SVF, especially the following two: </p>

```
@inproceedings{sui2016svf,
  title={SVF: interprocedural static value-flow analysis in LLVM},
  author={Sui, Yulei and Xue, Jingling},
  booktitle={Proceedings of the 25th international conference on compiler construction},
  pages={265--266},
  year={2016},
  organization={ACM}
}
```

```
@article{sui2014detecting,
  title={Detecting memory leaks statically with full-sparse value-flow analysis},
  author={Sui, Yulei and Ye, Ding and Xue, Jingling},
  journal={IEEE Transactions on Software Engineering},
  volume={40},
  number={2},
  pages={107--122},
  year={2014},
  publisher={IEEE}
}
```
