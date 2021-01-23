## News
* <b>[VSFS](https://github.com/SVF-tools/SVF/wiki/VSFS) published in our [CGO paper](https://yuleisui.github.io/publications/cgo21.pdf) is now available in SVF </b>
* <b>[TypeClone](https://github.com/SVF-tools/SVF/wiki/TypeClone) published in our [ECOOP paper](https://yuleisui.github.io/publications/ecoop20.pdf) is now available in SVF </b>
* <b>SVF now uses a single script for its build. Just type [`source ./build.sh`](https://github.com/SVF-tools/SVF/blob/master/build.sh) in your terminal, that's it!</b>
* <b>SVF now supports LLVM-10.0.0! </b>
* <b>We thank [bsauce](https://github.com/bsauce) for writing a user manual of SVF ([link1](https://www.jianshu.com/p/068a08ec749c) and [link2](https://www.jianshu.com/p/777c30d4240e)) in Chinese </b>
* <b>SVF now supports LLVM-9.0.0 (Thank [Byoungyoung Lee](https://github.com/SVF-tools/SVF/issues/142) for his help!). </b>
* <b>SVF now supports a set of [field-sensitive pointer analyses](https://yuleisui.github.io/publications/sas2019a.pdf). </b>
* <b>[Use SVF as an external lib](https://github.com/SVF-tools/SVF/wiki/Using-SVF-as-a-lib-in-your-own-tool) for your own project (Contributed by [Hongxu Chen](https://github.com/HongxuChen)). </b>
* <b>SVF now supports LLVM-7.0.0. </b>
* <b>SVF now supports Docker. [Try SVF in Docker](https://github.com/SVF-tools/SVF/wiki/Try-SVF-in-Docker)! </b>
* <b>SVF now supports [LLVM-6.0.0](https://github.com/svf-tools/SVF/pull/38) (Contributed by [Jack Anthony](https://github.com/jackanth)). </b>
* <b>SVF now supports [LLVM-4.0.0](https://github.com/svf-tools/SVF/pull/23) (Contributed by Jared Carlson. Thank [Jared](https://github.com/jcarlson23) and [Will](https://github.com/dtzWill) for their in-depth [discussions](https://github.com/svf-tools/SVF/pull/18) about updating SVF!) </b>
* <b>SVF now supports analysis for C++ programs.</b>
<br />

## Documentation

If you want to build the documentation yourself go into doc and invoke doxygen:
```
cd doc && doxygen doxygen.config
```

#### We are looking for self-motivated PhD students and we welcome industry collaboration/sponsorship to improve SVF (Please contact yulei.sui@uts.edu.au if you are interested)


<br />
<br />
<br />
SVF is a source code analysis tool that enables interprocedural dependence analysis for LLVM-based languages. SVF is able to perform pointer alias analysis, memory SSA form construction, value-flow tracking for program variables and memory error checking. 
<br />
<br />

| About SVF       | Setup  Guide         | User Guide  | Developer Guide  |
| ------------- |:-------------:| -----:|-----:|
| ![About](https://github.com/svf-tools/SVF/blob/master/images/help.png?raw=true)| ![Setup](https://github.com/svf-tools/SVF/blob/master/images/tools.png?raw=true)  | ![User](https://github.com/svf-tools/SVF/blob/master/images/users.png?raw=true)  |  ![Developer](https://github.com/svf-tools/SVF/blob/master/images/database.png?raw=true) 
| Introducing SVF -- [what it does](https://github.com/svf-tools/SVF/wiki/About#what-is-svf) and [how we design it](https://github.com/svf-tools/SVF/wiki/SVF-Design#svf-design)      | A step by step [setup guide](https://github.com/svf-tools/SVF/wiki/Setup-Guide#getting-started) to build SVF | Command-line options to [run SVF](https://github.com/svf-tools/SVF/wiki/User-Guide#quick-start), get [analysis outputs](https://github.com/svf-tools/SVF/wiki/User-Guide#analysis-outputs), and test SVF with [an example](https://github.com/svf-tools/SVF/wiki/Analyze-a-Simple-C-Program) or [PTABen](https://github.com/SVF-tools/PTABen) | Detailed [technical documentation](https://github.com/svf-tools/SVF/wiki/Technical-documentation) and how to [write your own analyses](https://github.com/svf-tools/SVF/wiki/Write-your-own-analysis-in-SVF) in SVF or [use SVF as a lib](https://github.com/SVF-tools/SVF-example) for your tool  |


<br />
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




