Semi-sparse的进一步改造，以适配dense和semi-sparse共存

背景：
目前虽然能够运行semi-sparse模式，但代码充满了打膏药的氛围。

## 需求一：统一 abstract value 访问入口

### 核心设计

将所有 `as[varId]` 这种直接通过中括号访问 AbstractState 的用法，
替换为 AbstractInterpretation 上的统一 API：

- **读 ValVar:** `ae.getAbstractValue(var, node)` — 从 def-site 拉取 top-level 变量的值
- **写 ValVar:** `ae.updateAbstractValue(var, val, node)` — 写入 top-level 变量的值
- **读 ObjVar:** `ae.getAbstractValue(objVar, node)` — 从当前 node 的 state 中 load
- **写 ObjVar:** `ae.updateAbstractValue(objVar, val, node)` — store 到当前 node 的 state

### 两层存储策略

| 层次 | dense 模式 | semi-sparse 模式 |
|------|-----------|-----------------|
| **ValVar (top-level)** | 每个 node 的 state 都有完整副本 | 只在 def-site 存储，use-site 通过 getAbstractValue 从 def-site 拉取 |
| **ObjVar (addr-taken)** | 每个 node 的 state 都有 | 同 dense，不变 |

### 依赖关系

```
AbstractInterpretation
  ├─ owns: AbsExtAPI (持有 AbstractInterpretation& ae 反向引用)
  ├─ owns: vector<unique_ptr<AEDetector>> (detect 接口传入 AbstractInterpretation& ae)
  └─ owns: Map<ICFGNode*, AbstractState> abstractTrace
```

- AEDetector.h / AbsExtAPI.h 只需 forward declare AbstractInterpretation，无循环 include
- .cpp 文件中 #include AbstractInterpretation.h 获取完整定义

### Load/Store 解耦

`as.loadValue(rhs)` / `as.storeValue(lhs, val)` 内部耦合了两层：
1. `(*this)[valVarId].getAddrs()` — 取指针的地址集 (top-level ValVar)
2. `load(addr)` / `store(addr, val)` — 操作 ObjVar (addr-taken)

semi-sparse 下 ValVar 可能不在当前 state，所以必须拆开：

```cpp
// updateStateOnLoad: q = *p
void updateStateOnLoad(const LoadStmt *load) {
    const ICFGNode* node = load->getICFGNode();
    // 步骤1: 通过 ae API 拿 p 的地址集（ValVar，可能在 def-site）
    const AbstractValue& ptrVal = getAbstractValue(load->getRHSVar(), node);
    // 步骤2: 对每个 addr 做 load（ObjVar，始终 dense）
    AbstractState& as = getAbstractState(node);
    AbstractValue res;
    for (auto addr : ptrVal.getAddrs())
        res.join_with(as.load(addr));
    // 步骤3: 写结果到 lhs（ValVar）
    updateAbstractValue(load->getLHSVar(), res, node);
}

// updateStateOnStore: *p = q
void updateStateOnStore(const StoreStmt *store) {
    const ICFGNode* node = store->getICFGNode();
    const AbstractValue& ptrVal = getAbstractValue(store->getLHSVar(), node);
    const AbstractValue& rhsVal = getAbstractValue(store->getRHSVar(), node);
    AbstractState& as = getAbstractState(node);
    for (auto addr : ptrVal.getAddrs())
        as.store(addr, rhsVal);
}
```

### 影响范围

需要消除 `as[id]` 的文件：
- AbstractInterpretation.cpp: updateStateOnLoad/Store/Copy/Gep/Phi/Select/... 等所有 updateStateOnXxx
- AbsExtAPI.cpp: 24 处 as[id]（SSE_FUNC_PROCESS 宏、各 ext API handler）
- AEDetector.cpp: 11 处 as[id]（BufOverflow/NullptrDeref 检测逻辑）
