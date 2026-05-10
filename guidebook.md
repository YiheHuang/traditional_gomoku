# 五子棋 AI — 从零到部署：完整教学指南

> 适用读者：有一定 C++ 基础，想了解博弈树搜索、棋类 AI 设计与工程化的开发者。
> 本文档覆盖从算法理论到安装包生成的完整链路。

---

## 目录

1. [五子棋 AI 的理论基础](#1-五子棋-ai-的理论基础)
2. [项目架构总览](#2-项目架构总览)
3. [模块一：棋盘与局面表示](#3-模块一棋盘与局面表示)
4. [模块二：棋型识别与评分](#4-模块二棋型识别与评分)
5. [模块三：局面评估器](#5-模块三局面评估器)
6. [模块四：走法生成与启发式排序](#6-模块四走法生成与启发式排序)
7. [模块五：置换表](#7-模块五置换表)
8. [模块六：威胁空间搜索](#8-模块六威胁空间搜索)
9. [模块七：Alpha-Beta 搜索引擎](#9-模块七alpha-beta-搜索引擎)
10. [模块八：游戏控制器与 UI](#10-模块八游戏控制器与-ui)
11. [构建系统与静态链接](#11-构建系统与静态链接)
12. [安装包部署](#12-安装包部署)
13. [性能优化经验总结](#13-性能优化经验总结)

---

## 1. 五子棋 AI 的理论基础

### 1.1 为什么五子棋适合 Alpha-Beta？

五子棋是**完全信息零和博弈**——双方看到的信息完全相同，一人赢则另一人输。这类游戏的数学模型叫**博弈树（Game Tree）**：每个节点是一个棋盘局面，边代表一个合法着法，叶子节点是终局（胜/负/和）。

理论上，如果能遍历整棵博弈树，就能得到完美下法。但五子棋 15×15 棋盘的状态空间约为 10^28 量级，不可能暴力穷举。因此需要两个核心技术：

1. **深度限制**：只搜索到某个深度，用评估函数估算局面价值
2. **Alpha-Beta 剪枝**：在搜索过程中跳过那些"不可能影响最终结果"的分支

### 1.2 博弈树搜索的核心矛盾

搜索面临一个经典权衡——**搜索深度 vs 搜索宽度**：

- 深度不够 → "水平线效应"（看不到远处的威胁）
- 宽度太大 → 指数爆炸，时间不够

本项目解决这个矛盾的手段：
- **走法截断**（限制候选着法到 18 个）
- **PVS（主变例搜索）**（零窗口剪枝）
- **置换表**（缓存重复局面）
- **启发式排序**（让好着法先被搜索，提高剪枝效率）
- **静止期搜索**（在叶节点只搜强力着法，缓解水平线效应）

---

## 2. 项目架构总览

```
src/
├── board.h/cpp          ← 棋盘数据 + Zobrist 哈希
├── pattern.h/cpp        ← 棋型识别 + 评分数值
├── evaluator.h/cpp      ← 局面评估（AI 的"眼睛"）
├── movegen.h/cpp        ← 着法生成 + 启发式排序
├── transposition.h/cpp  ← 置换表缓存
├── threat.h/cpp         ← 威胁空间搜索（VCT/VCF）
├── search.h/cpp         ← 主搜索（Alpha-Beta + 迭代加深）
├── game.h/cpp           ← 对局控制（逻辑层）
├── ui_win.h/cpp         ← Win32 图形界面
├── main.cpp             ← 程序入口
├── resource.rc          ← 图标 + 版本信息资源
└── icon.ico             ← 程序图标

CMakeLists.txt           ← 构建配置
installer/setup.iss      ← Inno Setup 安装包脚本
```

**模块依赖关系**：

```
main.cpp
 └─ ui_win.cpp ───── 用户界面层
      └─ game.cpp ─── 游戏逻辑层
           ├─ board.cpp ──── 数据层（基础）
           │    └─ pattern.cpp ── 棋型分析（被多处调用）
           └─ search.cpp ─── 搜索层（核心）
                ├─ movegen.cpp ─── 着法生成
                ├─ evaluator.cpp ── 局面评估
                ├─ transposition.cpp ── 缓存
                └─ threat.cpp ── 威胁搜索
```

---

## 3. 模块一：棋盘与局面表示

### 3.1 棋盘存储方式

```cpp
// board.h
constexpr int BOARD_SIZE = 15;
constexpr int BOARD_CELLS = 225;   // = 15 × 15

int cells[BOARD_CELLS];            // 0=空, 1=黑, 2=白
```

**设计要点**：
- **一维数组 vs 二维数组**：一维访问更快（一次乘法 vs 两次），缓存局部性更好
- **位置索引**：`idx = row × 15 + col`
- **0/1/2 编码**：比 `enum` 灵活，方便算术操作（如 `opponent = 3 - color`）

### 3.2 着法结构

```cpp
struct Move {
    int row, col;
    int index() const { return row * BOARD_SIZE + col; }
    bool valid() const;
};
```

### 3.3 胜负判定：增量算法

```cpp
int Board::checkWinner() const {
    // 只检查最后落子，不扫描全盘
    Move last = history.back();
    int color = cells[last.index()];

    for (int d = 0; d < 4; ++d) {          // 四个方向
        int count = 1;
        // 正向延伸
        for (int r = last.row+dr, c = last.col+dc;
             inBounds(r,c) && get(r,c)==color; r+=dr, c+=dc) ++count;
        // 反向延伸
        for (int r = last.row-dr, c = last.col-dc;
             inBounds(r,c) && get(r,c)==color; r-=dr, c-=dc) ++count;
        if (count >= 5) return color;        // 连五即胜
    }
    return EMPTY;
}
```

**复杂度**：O(1) 均摊 —— 只检查最后落子周围的 4 个方向，最多检查 20 个格子。

四个方向的偏移量：
| 方向 | dr | dc | 说明 |
|------|----|----|------|
| 水平 | 0 | 1 | 东-西 |
| 垂直 | 1 | 0 | 南-北 |
| 对角线 | 1 | 1 | 东南-西北 |
| 反对角线 | 1 | -1 | 西南-东北 |

### 3.4 Zobrist 哈希：局面指纹

**问题**：如何快速判断两个 Board 是否相同？（置换表需要这个）

**方案**：Zobrist 哈希 —— 给每个位置的每种状态分配一个随机 64 位整数，局面哈希 = 所有非空位置的随机数异或和。

```cpp
// 初始化：为每个位置 × 3种状态 生成随机数
void Board::initZobrist() {
    std::mt19937_64 rng(0x1A2B...);   // 固定种子保证可复现
    for (int i = 0; i < 225; ++i)
        for (int st = 0; st < 3; ++st)
            zobristTable[i][st] = rng();
    zobristSideToMove = rng();    // 额外一个值区分轮到谁走
}

// 增量更新（落子/悔棋时只需 3 次异或）
void Board::updateHash(int idx, int oldSt, int newSt) {
    zobristHash ^= zobristTable[idx][oldSt];  // 移除旧状态
    zobristHash ^= zobristTable[idx][newSt];  // 加入新状态
}
// makeMove 时额外异或 zobristSideToMove 来切换走棋方
```

**为什么用异或**：`a ^ b ^ b = a`，撤销操作只需再异或一次。

### 3.5 着法撤销

```cpp
std::vector<Move> history;    // 落子历史栈

void Board::undoMove() {
    Move m = history.back(); history.pop_back();
    sideToMove = opponent(sideToMove);  // 切回
    cells[m.index()] = EMPTY;           // 清空
    // 哈希会自动恢复，因为 updateHash 对称
}
```

---

## 4. 模块二：棋型识别与评分

### 4.1 什么是"棋型"？

在五子棋中，决定局面的不是每个孤立的棋子，而是它们形成的**棋型**——连续同色棋子的排列形态。关键棋型及其威胁等级：

| 棋型 | 定义 | 评分 | 威胁等级 |
|------|------|------|----------|
| 连五 | 5 连 | 10,000,000 | 直接获胜 |
| 活四 | 4连 + 两端空 | 1,000,000 | 必杀（无法同时堵两端） |
| 双四 | 两个冲四交叉 | 500,000 | 必杀 |
| 冲四 | 4连 + 一端空 | 100,000 | 必须立即防守 |
| 双三 | 两个活三交叉 | 50,000 | 必杀（分叉） |
| 活三 | 3连 + 两端空 | 10,000 | 下一步形成活四 |
| 眠三 | 3连 + 一端空 | 1,000 | 有一定威胁 |
| 活二 | 2连 + 两端空 | 200 | 发展潜力 |
| 眠二 | 2连 + 一端空 | 20 | 微小价值 |

**评分设计的核心原则**：高分棋型的价值必须远大于任意数量低分棋型之和。例如：活四（1,000,000）> 100 个活三（1,000,000）。这样才能保证搜索优先处理致命威胁。

### 4.2 方向分析算法

```cpp
DirResult analyseDir(const int* board, int r, int c,
                     int dr, int dc, int color) {
    // 从 (r,c) 出发，沿 (dr,dc) 方向计算
    // 如果在这里放一个 `color` 棋子，方向的棋型如何？

    int count = 0;     // 连续同色棋子数（不含 (r,c) 本身）
    int openEnds = 0;  // 空端数（0, 1, 2）

    // 正向延伸：跳过连续 `color` 棋子，遇到空位则 openEnds++
    for (nr=r+dr, nc=c+dc; inBounds&&board[nr][nc]==color; nr+=dr, nc+=dc)
        count++;
    if (inBounds && board[nr][nc]==EMPTY) openEnds++;

    // 反向同理
    for (nr=r-dr, nc=c-dc; ...)
        count++;
    if (inBounds && board[nr][nc]==EMPTY) openEnds++;

    return {count + 1, openEnds};  // +1 是 (r,c) 自身
}
```

**示例**：在位置 O 放黑子，已有两个黑子相邻：
```
· · ○ · ● ● O · ·    ← 水平方向
```
- 向左（反向）：遇到 ●●，count+=2，再左是空 → openEnds++
- 向右（正向）：全空 → openEnds++
- 结果：`{count=3, openEnds=2}` → 活三（10,000分）

### 4.3 复合棋型检测

一个位置可能同时在多个方向形成棋型。当两个活三在同一位置交叉，就形成**双三**（无法同时防守两个活三）：

```cpp
int compoundBonus(const DirResult dr[4]) {
    // 统计四个方向中各有多少冲四、活三、眠三
    int fours=0, openThrees=0, threes=0;
    for (int i=0; i<4; ++i) {
        if (dr[i].count >= 5) return VAL_FIVE;
        if (dr[i].count==4 && dr[i].openEnds>=1) fours++;
        if (dr[i].count==3 && dr[i].openEnds==2) openThrees++;
        if (dr[i].count==3 && dr[i].openEnds==1) threes++;
    }
    if (fours >= 2)                 return VAL_DOUBLE_FOUR;
    if (fours>=1 && openThrees>=1)  return VAL_DOUBLE_FOUR;
    if (openThrees >= 2)            return VAL_DOUBLE_THREE;
    // ...
}
```

---

## 5. 模块三：局面评估器

### 5.1 评估函数的设计思路

**输入**：一个棋盘局面（`Board`）+ 评估视角（`side`，黑或白）
**输出**：一个整数，正值 = 对 `side` 有利，负值 = 不利

核心问题：如何从 15×15 的棋盘上提取出有意义的评分数值？

### 5.2 基于"最佳着法潜力"的评估方法

本项目采用的方法：**对每个空位，模拟："如果我在这里落子，能形成多大威胁？如果对手在这里落子呢？"**

具体步骤：

```
1. 找出所有候选空位（已有棋子周围 2 格范围内）
2. 对每个候选位：
   a. 调用 quickScore(board, r, c, AI颜色)  → 攻击潜力
   b. 调用 quickScore(board, r, c, 对手颜色) → 防御价值
3. 分别取前 4 强的攻击分和前 4 强的防御分
4. 加权求和：
   score = Σ(攻击Top4 × 权重) - Σ(防御Top4 × 权重) + 位置加成
```

权重公式：`score = best[0] + best[1]/3 + best[2]/9 + best[3]/27`

**为什么用 1-3-9-27 衰减**：最强威胁占据主导地位（冲四挡不住，活三再多也白搭），次要威胁作为辅助参考。

### 5.3 位置加成

五子棋中，**棋盘中心比边角更有价值**（中心位置可控制更多方向）：

```
      0  0  0  0  0  0  0  0  0  0  0  0  0  0  0
      0  1  1  1  1  1  1  1  1  1  1  1  1  1  0
      0  1  2  2  2  2  2  2  2  2  2  2  2  1  0
      ...
      0  1  2  3  4  5  6 [7] 6  5  4  3  2  1  0   ← 天元=7
      ...
```

### 5.4 胜负检测优先

```cpp
if (winner == side)  return  20000000;   // 已获胜
if (winner == opp)   return -20000000;   // 已输
if (board.isFull())  return  0;          // 平局
```

胜负值必须远大于任何评估值，确保搜索能区分"确定胜负"和"评估优势"。

### 5.5 性能优化：近邻扫描

不遍历全部 225 个空位，只扫描已有棋子周围 2 格（Chebyshev 距离）的空位：

```cpp
bool near[225] = {false};
for (每个已有棋子)
    for (dr=-2; dr<=2; ++dr)
        for (dc=-2; dc<=2; ++dc)
            near[(r+dr)*15 + (c+dc)] = true;
// 只评估 near[i] == true 的空位
```

**效果**：开局时空位只有约 24 个（vs 225），中盘约 50-80 个。

---

## 6. 模块四：走法生成与启发式排序

### 6.1 候选着法筛选

**需求**：从最多 225 个空位中，挑出"可能有意义"的候选，限制搜索宽度。

**方法**：只考虑已有棋子周围 2 格内的空位。

```cpp
vector<Move> MoveGenerator::generateMoves(const Board& board) {
    if (board.ply() == 0) return {Move(7,7)};  // 第一手下天元

    // 收集近邻空位
    bool visited[225] = {false};
    for (每个已有棋子)
        for (dr=-2; dr<=2; ++dr)
            for (dc=-2; dc<=2; ++dc)
                如果 (nr,nc) 在棋盘内且为空且没被访问 → 加入候选

    // 限制到前 18 个
    if (moves.size() > 18) {
        // 用 std::nth_element 取评分前 18（O(n) 部分排序）
        评分 = attackScore + defenseScore/10;
        保留前 18;
    }
}
```

**为什么是 18**：经验值，在搜索宽度和深度之间取得平衡。太少会漏掉好着法，太多消耗时间。

### 6.2 启发式排序：让好着法先被搜

Alpha-Beta 的特性是：**着法排序越好，剪枝越多**。理想情况下有效分支因子 = √原始分支因子。

排序策略（按优先级从高到低）：

1. **置换表最佳着法**（来自前一次搜索"记住"的好着法） → 移到列表第一位
2. **杀手着法**（`killers[ply]`） → 权重 900,000 / 800,000
3. **历史启发**（`historyTable[pos]`） → 权重最高 700,000
4. **快速评分**（`quickScore`） → 攻击分 + 防御分/10

```cpp
void MoveGenerator::sortMoves(vector<Move>& moves, Board& board, int ply) {
    // 对每个着法计算综合评分
    for (auto& m : moves) {
        score = quickScore(board, m)           // 静态评估
              + killerBonus(ply, m)            // 杀手启发
              + historyScore(m);               // 历史启发
    }
    sort(moves, by_score_descending);
}
```

### 6.3 杀手启发式

**直觉**：在一个节点，如果着法 M 导致了 Beta 截断（即 M 太好了，对手不会让这种情况发生），那么在同层的兄弟节点中，M 很可能也是好着法。

```cpp
void addKiller(int ply, Move m) {
    // 二层 FIFO：新的放 [0]，旧的移到 [1]
    if (killers[ply][0] != m) {
        killers[ply][1] = killers[ply][0];
        killers[ply][0] = m;
    }
}
```

### 6.4 历史启发式

**直觉**：如果某个位置（比如 (7,7) 天元）在搜索树的许多地方都被证明是好着法，那它应该在全局优先被尝试。

```cpp
void addHistory(Move m, int depth) {
    historyTable[m.index()] += depth * depth;  // 深层截断更有价值
}

void ageHistory() {
    for (auto& v : historyTable) v /= 2;  // 衰减旧经验
}
```

---

## 7. 模块五：置换表

### 7.1 为什么需要置换表？

在 Alpha-Beta 搜索中，不同的着法序列可能到达相同的局面。例如：

```
序列 1：黑下 A，白下 B，黑下 C
序列 2：黑下 A，白下 C，黑下 B    ← 到达同样的局面（如果 B 和 C 互不影响）
```

置换表缓存每个已搜索局面的结果，遇到重复局面直接查表。

### 7.2 双路组相联存储

```cpp
struct TTBucket {
    TTEntry entries[2];   // 每个桶 2 个槽位
};

struct TTEntry {
    uint64_t key;    // Zobrist 哈希键（用于验证身份）
    int depth;       // 搜索深度（更深 = 信息更可靠）
    int value;       // 评价值
    Bound bound;     // EXACT / LOWER / UPPER
    Move bestMove;   // 该局面下的最佳着法
};
```

### 7.3 边界类型

Alpha-Beta 搜索可能返回三种情况：

| 边界 | 含义 | 如何使用 |
|------|------|----------|
| `EXACT` | 精确值 | 直接返回 |
| `LOWER` | 下界（实际值 ≥ 存储值） | 如果存储值 ≥ beta，可以截断 |
| `UPPER` | 上界（实际值 ≤ 存储值） | 如果存储值 ≤ alpha，可以忽略 |

**示例**：
```
alphaBeta(board, depth=5, alpha=100, beta=200)
→ 搜索后发现 score=150 → 存储为 EXACT, value=150

alphaBeta(board, depth=5, alpha=100, beta=200)
→ 某个分支 beta 截断，score≥200 → 存储为 LOWER, value=200
  （表示"至少 200"，下次如果 beta≤200 可以直接截断）

alphaBeta(board, depth=5, alpha=100, beta=200)
→ 全部着法都 ≤100 → 存储为 UPPER, value=100
  （表示"最多 100"，下次如果 alpha≥100 可以直接跳过）
```

### 7.4 替换策略

```cpp
void store(key, depth, value, bound, bestMove) {
    // 1. 如果同一局面已有更深记录 → 保留旧条目
    if (entries[i].depth >= depth) return;

    // 2. 否则替换深度最浅的条目
    replace the one with smallest depth;
}
```

**设计哲学**：深度更深的信息更可靠，优先保留。

### 7.5 内存大小

默认 64 MiB：
```
numBuckets = 64 × 1024 × 1024 / sizeof(TTBucket)
           ≈ 64 × 1,048,576 / 48
           ≈ 1,398,101 个桶（≈ 280 万个条目）
```

---

## 8. 模块六：威胁空间搜索

### 8.1 理论背景

VCT（Victory by Continuous Threats）和 VCF（Victory by Continuous Four）是专门用于寻找强制获胜序列的搜索技术。

**核心思想**：
- 攻击方只走"威胁着法"（冲四、活三）——这些着法对方必须回应
- 防守方只能回应威胁（堵住冲四端点、破坏活三）
- 攻击方继续走下一个威胁...
- 如果最终攻击方形成连五，就找到了必胜序列

### 8.2 威胁分类

```cpp
enum ThreatType { THR_NONE, THR_FOUR, THR_OPEN_THREE };
```

- **THR_FOUR**：冲四、活四、双四、双三等（防守方必须立即回应，通常只有 1-2 个防守选择）
- **THR_OPEN_THREE**：活三（防守方有多个选择，但必须回应）

### 8.3 VCF vs VCT

**VCF**（只走冲四）：防守选择极少（通常 1 个），分支因子 ≈ 1，可以搜索很深（15 层+）

**VCT**（走冲四 + 活三）：活三的防守选择多（2-4 个），分支更大，采用分层策略：
- 冲四威胁 → 递归调用 VCF（防守选择少）
- 活三威胁 → 递归调用 VCT（防守选择多）

### 8.4 防守着法生成

```cpp
void findDefenses(board, attacker, attackMove, defenses) {
    // 1. 攻击位置本身总是防守点（先占掉）
    defenses.push_back(attackMove);

    // 2. 对四个方向分析连续棋子的端点
    for (每个方向) {
        统计通过 attackMove 的连续 attacker 棋子数 cnt
        if (cnt == 4)  堵住两个端点
        if (cnt == 3)  堵住两个端点 + 中间跳位（处理跳活三）
    }
}
```

### 8.5 实际使用情况

VCT/VCF 模块已在项目中完整实现，编入了引擎，但最终版本的搜索函数中**已将其移除**。原因是在实践中，VCT/VCF 出现了难以调试的假阳性问题——即在某些复杂局面下错误地报告找到了必胜序列（实际并不能保证获胜）。

对于五子棋这种"深度优先"的棋类，高质量的 Alpha-Beta 搜索（深度 7-9 层配合充分的静止期搜索）本身已经能覆盖绝大多数攻防场景。VCT/VCF 更适合作为独立的功能模块用于特殊用途（如解残局题、教学演示等）。

---

## 9. 模块七：Alpha-Beta 搜索引擎

### 9.1 算法核心：Alpha-Beta 剪枝

```
function alphaBeta(board, depth, alpha, beta):
    if depth == 0 or game_over:
        return evaluate(board)

    for each move in generateMoves(board):
        makeMove(move)
        score = -alphaBeta(board, depth-1, -beta, -alpha)
        undoMove(move)

        alpha = max(alpha, score)
        if alpha >= beta:
            break    // β 截断：对手不会让这个分支发生

    return alpha
```

**参数解释**：
- `alpha`：我方在这个节点的最低保证值
- `beta`：对手能容忍的最高值
- `alpha >= beta`：出现了"我方要太多"的情况 → 剪枝

### 9.2 PVS：主变例搜索

标准 Alpha-Beta 对所有子节点都用 `(-beta, -alpha)` 窗口。PVS 优化：**只对第一个子节点用全窗口，后续子节点先用零窗口试探**。

```cpp
for (size_t i = 0; i < moves.size(); ++i) {
    if (i == 0) {
        // 主变例：全窗口
        score = -alphaBeta(depth-1, -beta, -alpha);
    } else {
        // 试探窗口：[alpha, alpha+1] — 几乎为零
        score = -alphaBeta(depth-1, -alpha-1, -alpha);
        // 如果有希望（alpha < score < beta），再全窗口重搜
        if (score > alpha && score < beta)
            score = -alphaBeta(depth-1, -beta, -alpha);
    }
    // ...
}
```

**零窗口 = [alpha, alpha+1]**：这个窗口窄到几乎不可能有合法值落在里面。只有两种可能：
- `score <= alpha`：这个着法比当前最佳差 → 跳过
- `score > alpha`：这个着法可能更好 → 用全窗口确认

### 9.3 迭代加深

```cpp
for (int d = 1; d <= maxDepth; ++d) {
    score = alphaBeta(board, d, -INF, +INF);
    if (outOfTime()) break;    // 时间到了就返回上层的 bestMove
    bestMove = tt.probe(hash)->bestMove;
}
```

**为什么迭代加深**：
1. **时间控制**：随时可以返回一个"虽然不完美但合理"的着法
2. **置换表热身**：浅层搜索填充 TT，帮助深层搜索更好地排序着法
3. **主变例持续更新**：每层都更新最佳着法

### 9.4 静止期搜索：解决水平线效应

**水平线效应**：搜索深度有限（比如 6 层），AI 可能会"故意"在第 7 层制造一个它"看不到"的灾难。类似"掩耳盗铃"。

**解决**：到达叶节点（depth=0）时不直接评估，而是继续搜索"强制着法"：

```cpp
int quiescence(board, alpha, beta, ply) {
    standPat = evaluate(board);     // 当前局面的静估值
    if (standPat >= beta) return beta;

    // 只搜强力威胁：连五、活四、冲四
    for (每个候选空位) {
        如果 quickScore >= VAL_FOUR → 走一步，递归搜索
    }
    // 最多搜 4 层防止爆炸
}
```

### 9.5 AI 线程模型

在图形界面中，AI 搜索运行在独立线程中，避免阻塞 UI：

```cpp
// 主线程（UI）
case WM_LBUTTONDOWN:
    玩家落子;
    启动 AI 线程;

// AI 线程
void aiThinkFunc() {
    Board boardCopy = gGame.getBoard();    // 复制棋盘（不持锁）
    SearchResult res = engine.search(boardCopy);  // 耗时操作
    lock(mutex);
    gGame.applyMove(res.bestMove);         // 回写结果（持锁）
    unlock(mutex);
    PostMessage(hwnd, WM_APP, 0, 0);       // 通知 UI 刷新
}

// 主线程
case WM_APP:
    更新屏幕;
    检查胜负;
```

**关键设计**：AI 线程在锁外复制棋盘、在锁外搜索，只在写回结果时短暂持锁。这保证了：
- UI 线程始终响应
- 锁持有时间极短
- 不会有数据竞争

---

## 10. 模块八：游戏控制器与 UI

### 10.1 Game（游戏逻辑层）

```cpp
class Game {
    Board board;
    SearchEngine ai;
    bool aiIsBlack;
};
```

**职责**：连接 Board（数据）和 SearchEngine（AI），为 UI 提供统一接口。

关键方法：
- `makePlayerMove(r,c)` — 玩家落子（简单验证）
- `applyMove(m)` — AI 落子（应用已计算好的着法，不重新搜索）
- `result()` — 对局状态查询

### 10.2 Win32 图形界面

**为什么用 Win32 GDI**：
- 零外部依赖（任何 Windows 都自带）
- 体积小（静态链接后整个 exe 仅 2.5MB）
- 鼠标操作自然（直接点击棋盘交叉点）

**双缓冲绘制**（消除闪烁）：

```cpp
HDC memDC = CreateCompatibleDC(hdc);           // 离屏 DC
HBITMAP memBM = CreateCompatibleBitmap(...);   // 离屏位图
// 在 memDC 上绘制所有内容...
BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);  // 一次性输出
```

**动态布局**（适应不同 DPI 和窗口大小）：

```cpp
void computeLayout() {
    availW = clientW - 70;   // 留出坐标标签空间
    availH = clientH - 130;  // 留出标题 + 状态栏

    gCell = min(availW, availH) / 14;   // 自适应格子大小
    gLeft = (clientW - gCell*14) / 2;   // 水平居中
    gTop  = (clientH - gCell*14) / 2;   // 垂直居中
    gRad  = gCell * 17 / 38;            // 棋子半径比例
}
```

**鼠标坐标映射**：

```cpp
void getBoardPos(int mx, int my, int& r, int& c) {
    col = round((mx - gLeft) / gCell);   // 最近交叉点
    row = round((my - gTop)  / gCell);
    // 距离检查：防止点到格子空白区域
    int dx = mx - (gLeft + col*gCell);
    int dy = my - (gTop  + row*gCell);
    if (dx*dx + dy*dy > gRad*gRad) { r=-1; c=-1; }
}
```

---

## 11. 构建系统与静态链接

### 11.1 CMakeLists.txt 解析

```cmake
cmake_minimum_required(VERSION 3.16)
project(gomoku LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)          # C++17（std::optional 等现代化特性）
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)       # 禁止 GNU 扩展，保证可移植

if(MSVC)
    add_compile_options(/O2 /W3)
else()
    add_compile_options(-O3 -Wall -Wextra)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")  # ★
endif()
```

**`-static` 标志的作用**：告诉 MinGW 链接器将 C++ 标准库（`libstdc++`）、GCC 运行时（`libgcc_s`）、POSIX 线程库（`libwinpthread`）全部静态链接进 exe。结果是一个**自包含**的可执行文件，不依赖任何 MinGW 特有的 DLL。

### 11.2 验证自包含

```bash
# 编译前（动态链接）
$ ldd gomoku.exe
    libstdc++-6.dll => /mingw64/bin/libstdc++-6.dll      # ← 用户可能没有
    libgcc_s_seh-1.dll => /mingw64/bin/libgcc_s_seh-1.dll # ← 用户可能没有
    libwinpthread-1.dll => /mingw64/bin/...               # ← 用户可能没有
    GDI32.dll => /c/WINDOWS/System32/GDI32.dll            # ← 系统自带 ✓

# 编译后（静态链接）
$ ldd gomoku.exe
    GDI32.dll => /c/WINDOWS/System32/GDI32.dll            # ← 系统自带 ✓
    KERNEL32.dll => /c/WINDOWS/System32/KERNEL32.dll      # ← 系统自带 ✓
    USER32.dll => /c/WINDOWS/System32/USER32.dll          # ← 系统自带 ✓
    msvcrt.dll => /c/WINDOWS/System32/msvcrt.dll          # ← 系统自带 ✓
```

### 11.3 版本信息资源

```rc
// resource.rc
MAINICON ICON "icon.ico"    // 程序图标

VS_VERSION_INFO VERSIONINFO
FILEVERSION 1,0,0,0
...
BEGIN
    VALUE "FileDescription", "Gomoku AI - Alpha-Beta Search"
    VALUE "ProductName",     "Gomoku AI"
    VALUE "ProductVersion",  "1.0.0.0"
END
```

这个文件让 exe 的属性页显示版本、产品名等信息，也提供任务栏和开始菜单中的图标。

---

## 12. 安装包部署

### 12.1 选择 Inno Setup 的理由

- **免费开源**，久经考验（自 1997 年）
- **脚本化**，易于版本控制和 CI 集成
- **原生 Windows** 安装体验
- 支持压缩（LZMA2）、多语言、环境检测

### 12.2 安装脚本结构

```iss
[Setup]       ← 基本信息（名称、版本、默认路径、压缩方式）
[Languages]   ← 中文简体语言包
[Files]       ← 要安装的文件列表
[Icons]       ← 快捷方式（开始菜单、桌面）
[Run]         ← 安装后可选启动程序
[Code]        ← Pascal 脚本（环境检测 + 自定义逻辑）
```

### 12.3 环境检测

```pascal
function InitializeSetup: Boolean;
var WinVer: TWindowsVersion;
begin
    GetWindowsVersionEx(WinVer);
    // Windows 7 = 6.1, Windows 8 = 6.2, Windows 10 = 10.0
    if (WinVer.Major < 6) or ((WinVer.Major = 6) and (WinVer.Minor < 1)) then
    begin
        MsgBox('需要 Windows 7 或更高版本。', mbCriticalError, MB_OK);
        Result := False;   // 拒绝安装
    end;
end;
```

### 12.4 构建安装包

```bash
"C:/InnoSetup/ISCC.exe" installer/setup.iss
# 输出：installer/GomokuAI-Setup-1.0.exe (2.2 MB)
```

安装包体积仅 2.2MB（exe 2.5MB LZMA2 压缩后），下载和安装速度极快。

---

## 13. 性能优化经验总结

### 13.1 搜索性能对比

| 优化措施 | 优化前 | 优化后 | 提升 |
|---------|--------|--------|------|
| 走法限制到 18 | 50+ 着法 | 18 着法 | 分支因子 ÷3 |
| 近邻扫描评估 | 扫描 225 格 | ~50 格 | 评估速度 ×4-5 |
| 置换表 | 0% 命中 | 30-45% 命中 | 避免 30%+ 重复搜索 |
| 杀手+历史启发 | 有效分支 ~18 | 有效分支 ~5-8 | 剪枝效率 ×3 |
| 静止期搜索 | 水平线效应 | 重大威胁不遗漏 | 质量提升 |
| 静态链接 | 依赖 3 个 DLL | 自包含 | 部署简化 |

**最终性能**：5 秒搜索深度 7-9，约 150-200 万节点，有效分支因子约 3-5。

### 13.2 关键设计教训

1. **评估函数是核心**：棋型评分的数量级设计直接决定 AI 水平。活四必须比 100 个活三评分更高。

2. **着法排序 = 半壁江山**：Alpha-Beta 的效率几乎完全取决于着法排序的质量。杀手启发和历史启发虽然简单，但对排序的贡献巨大。

3. **TT 的深度优先替换策略**：保留深度更深的条目比保留"最近使用的"条目更有价值。

4. **静态链接的价值**：对于个人/小团队项目，静态链接消除了"用户机器上缺少某某 DLL"的烦恼。

5. **VCT/VCF 的陷阱**：专门的威胁搜索虽然理论上强大，但在实践中容易出现假阳性。对于大多数场景，深度足够的 Alpha-Beta 配合静止期搜索是更稳健的选择。

---

## 附录：快速上手

### 编译

```bash
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
# → build/gomoku.exe
```

### 打包安装包

```bash
"C:/InnoSetup/ISCC.exe" installer/setup.iss
# → installer/GomokuAI-Setup-1.0.exe
```

### 运行

直接双击 `gomoku.exe` 或 `GomokuAI-Setup-1.0.exe` 安装后从开始菜单启动。

### 项目结构速查

```
src/                     ← 源代码（11 个 .cpp, 10 个 .h, 1 个 .rc）
CMakeLists.txt           ← 构建配置
installer/
  setup.iss              ← Inno Setup 安装脚本（Pascal）
guidebook.md             ← 本文档
```
