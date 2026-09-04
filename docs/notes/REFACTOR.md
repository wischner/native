# Deferred Theme Interface Split

Review date: 2026-09-03.

This note records the only unimplemented finding from the refactoring review.
It is a proposal, not a description of shipping code.

## `theme` is a 69-getter god interface

**Measured: `theme::metrics` has 37 fields. `theme` exposes 69 getters and 18
draw virtuals.**

Every consumer of `theme` depends on all of it. A `ruler` that needs
`get_ruler_extent()` also compiles against `tree_item_gap`,
`icon_view_min_item_width` and `table_fill_last_column`. Adding a metric for
one control recompiles every control and forces all nine backends to consider
a field most of them do not care about. That is the interface-segregation
principle inverted.

## Proposal

Keep one `theme` implementation per backend — that part is right, and a single
native look is the point. Split the *interface* by consumer:

```cpp
class theme : public control_metrics,
              public collection_metrics,
              public chrome_metrics { /* draw operations */ };
```

Controls take the narrow reference they need (`chrome_metrics &` for a ruler),
so a new collection metric no longer recompiles the button.

## Deferral criteria

This change touches every backend theme and every control. Implement it only
if `theme` keeps growing enough to justify that cost. If the metric count stays
stable, leave the interface intact and group the metrics fields by consumer
with comments.
