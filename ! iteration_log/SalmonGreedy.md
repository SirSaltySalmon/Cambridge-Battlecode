# Salmon greedy NOT COMPLETELY UPDATED WITH normal

Changes in `bots/vesper-salmon-greedy` on top of `bots/vesper-salmon-upgraded`

## Farm what sits on the walk

The upgraded bot walked from one planned pocket group to the next and only stepped aside for wolves or clusters within 5 tiles. Camps on the path but further than that, and elk of any distance, were skipped. The 1.25 switch margin then kept the planned goal even when a nearby stop was only a little better.

- The planned goal is still a gold-weighted pocket while farming, and the centre pocket once aggressive mode is on.
- Any visible group on that walk is taken first, nearest first: extra walking of 4 tiles or less, or anything within 8 tiles.
- A follower that sees loot the leader cannot still writes it into the shared slot, as long as it is not behind them.
- Clusters in attack range are hit while walking past, even if they are not the shared target. They have no strength, so the hit is free.
- When the pocket is still respawning on arrival, the team spends the wait on the best group from where they stand.

## Monoliths wait until the team is strong

A 20,000-health monolith is a long commit for an early team.

- Monoliths are left until the team owns at least 10 emblems.
- Emblem counts are remembered from vision; Vesper's done flag counts as her six if nobody else can see her bag.
