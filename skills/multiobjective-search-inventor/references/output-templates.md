# Output templates

Use these as defaults. Adapt the level of detail to the user's request.

## 1. Paper synthesis template

### [paper title]

**Problem setting**
- graph/search setting:
- exact or approximate:
- objectives:

**Nearest lineage**
- nearest ancestors:
- neighboring alternatives:

**Core mechanism**
- state or label representation:
- dominance strategy:
- heuristic strategy:
- main algorithmic trick:

**Guarantees**
- completeness:
- pareto optimality or approximation type:
- required assumptions:

**Complexity and bottlenecks**
- time drivers:
- memory drivers:

**Empirical takeaway**
- where it seems to help:
- where it may struggle:

**What is actually new**
- precise delta:

## 2. Method comparison template

| axis | method a | method b | method c |
|---|---|---|---|
| setting |  |  |  |
| exact / approximate |  |  |  |
| objective count |  |  |  |
| dominance checks |  |  |  |
| heuristics |  |  |  |
| open-list policy |  |  |  |
| reopening policy |  |  |  |
| guarantees |  |  |  |
| main bottleneck |  |  |  |
| best-use regime |  |  |  |

Follow the table with a short paragraph explaining the real trade-off.

## 3. New algorithm concept note

### [working name]

**Problem variant**
- define the objective vector, graph assumptions, and target regime precisely.

**Nearest lineage**
- baseline:
- inherited pieces:
- changed pieces:

**Core idea**
- one paragraph focused on the single main mechanism.

**Algorithm sketch**
1. initialization
2. expansion rule
3. dominance or pruning rule
4. termination condition

**Why it might help**
- expected runtime or memory gain:
- regime where the gain should appear:

**Proof obligations**
- soundness of pruning:
- heuristic conditions:
- termination:
- exactness or approximation guarantee:

**Experiment plan**
- baselines:
- datasets or graph families:
- metrics:
- ablations:

**Failure modes**
- where the idea may underperform or break assumptions:

## 4. Pseudocode template

```text
Input:
    G = (V, E), start s, goal t
    objective count m
    admissible lower bounds h[1..m] if used
Data structures:
    Open               // priority queue over labels
    Front[v]           // current nondominated labels or summaries at vertex v
    DomIndex[v]        // optional acceleration structure for Front[v]
    Sol                // goal labels or incumbent frontier

Procedure Search():
    initialize label ell0 = (v=s, g=zeros(m), parent=nil)
    Insert(ell0)
    while Open not empty:
        ell = Pop(Open)
        if GoalFiltered(ell, Sol):
            continue
        if LazyCheckNeeded(ell) and IsDominated(ell, Front[ell.v], DomIndex[ell.v]):
            continue
        if ell.v == t:
            AddSolution(ell, Sol)
            continue
        for each edge (ell.v, u) in E:
            child = Extend(ell, u)
            if NotPromisingByBounds(child, Sol):
                continue
            if not IsDominated(child, Front[u], DomIndex[u]):
                InsertAndPrune(child, Front[u], DomIndex[u])
                Push(Open, child)

Procedure IsDominated(label, Front[v], DomIndex[v]):
    // state exact fast path, fallback, and return condition explicitly

Procedure InsertAndPrune(label, Front[v], DomIndex[v]):
    // define how dominated old labels are removed and how indexes are updated
```

After the pseudocode, always add:

- exact or approximate semantics
- reopening policy
- dominant time costs
- dominant memory costs

## 5. Related-work skeleton

Use this order by default:

1. exact core MOSP / MOA* / BOS
2. theory and surveys
3. OR-side exact shortest-path methods
4. heuristic and dominance engineering
5. approximate or anytime methods
6. dynamic or multi-agent extensions

For each bucket, write one paragraph that answers:

- what problem regime this bucket targets
- what the key technical lever is
- how it differs from the user's current problem

## 6. Experiment matrix template

| factor | levels |
|---|---|
| graph family | road / grid / random / domain-specific |
| objective count | 2 / 3 / k |
| correlation | low / medium / high |
| weight model | nonnegative / negative without negative cycles |
| query type | one-to-one / one-to-many / global |
| method knobs | exact / approximate / anytime |

Add one short sentence explaining why each factor matters for the claimed contribution.
