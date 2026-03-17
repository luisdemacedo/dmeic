# Open-WBO MOCO Solver
## Version 0.0

This version extends the Open-WBO to the multiobjective case.

This tool solves the multi-objective optimization problem
(MOCO). Given some instance (check the example below) you will get a
set of Pareto-optimal solutions if you wait enough. If the solver is
interrupted, it will report back an approximation to the complete set
of Pareto-optimal solutions.

### Compile
Hopefully, you will be able to build everything with a single make call,
```
make
```
### Usage ###
```
./RUNME.sh <algorithm> <instance> [conflict budget] [waiting list type]
```

You can run any of the following algorithms:
1. ParetoMCS (sat4j), `pmcs`; (<https://www.researchgate.net/publication/326204042_Multi-Objective_Optimization_Through_Pareto_Minimal_Correction_Subsets>)
2. P-Minimal , `pmin`; (<https://link.springer.com/chapter/10.1007/978-3-319-66158-2_38>)
3. Hitting-Sets, `hs`; (<https://arxiv.org/abs/2204.10856>)
4. Core-Guided, `us`; (<https://arxiv.org/abs/2204.10856>)
6. tandem Core-Guided and p-minimal, `uspmin`; (<https://arxiv.org/abs/2204.10856>)
7. Slide and Drill, `sd`; (https://doi.org/10.4230/LIPIcs.CP.2024.8)
8. Lower bound refiner, `lbr`;
9. Epsilon lower bound refiner, `e-lbr`;


Some algorithms use a conflict budget.  When set , the calls to the
sat oracle may return before completion.  Besides, some algorithms use
a waiting list.  0 corresponds to a stack, 1 to a queue, and 2 to a
priority queue. The priority function takes into consideration the
number of attempts for each stored item and the hyper volume of the
point.

For Example:

```
./RUNME.sh sd examples/DAL1.pbmo 100 1
```
will run the Slide and Drill algorithm, using a conflict budget of 100
and a queue as the waiting list that stores the points that have yet
to be "drilled".


Run ./RUNME.sh with no arguments for more details.
### Input format ###
The file should start with a list of Pseudo-Boolean functions to
minimize, followed by a set of pseudo-Boolean restrictions. For
instance,

```
min: +1 ~x1 -5 x2
min: +2 x1 +9 x3
x1 + x2 < 1
x1 + x3 >= 1
```
The tilde operator negates a boolean variable. The weights can be
either positive or negative integers.

## Output format

Typical problems will take a long time to compute the full
Pareto-front. In any case, the solver will report back a partial
solution if interrupted by SIGINT.

Open-WBO follows the standard output of MaxSAT solvers:
* Comments ("c " lines) 
* Solution Status ("s " line):
  * `s OPTIMUM` : the full Pareto-front was found;
  * `s UNSATISFIABLE` : the hard clauses are unsatisfiable;
  * `s SATISFIABLE`   : some solutions were found but optimality was not proven;
* Solution Cost Line ("o " lines):
  * This represents the cost of some solution found by the solver;
* Solution Values (Truth Assignment) ("v " lines): 
  * This represents the truth assignment (true/false) assigned to each
  variable;
* Number of Solutions ("n " line): the cardinality of the current partial solution;

## Instances

You will find a dataset in
[zenodo.](https://zenodo.org/records/15587066?token=eyJhbGciOiJIUzUxMiJ9.eyJpZCI6IjJhMTVkNzFjLTZkYWEtNDMzMS04NTMxLWU1YWJkYWEzMThiOCIsImRhdGEiOnt9LCJyYW5kb20iOiJmMzk1ZDBkZmZhNmRhMjIzNjE1Y2Q4ZThhYjc2NDliNCJ9.t2iUo_sn_Gaqywg2yoxOozv_iOBzDGKln0GLsmsul9vYkD3UpaXjsoL__0CjVC8M8t4WijGdwWGFnA9AOqxF1A)

It has in it instances from 4 different MOCO problems: 

1. Development Assurance Levels;
2. Flying Tourist Problem;
3. Set Covering;
4. Package Upgradability;
