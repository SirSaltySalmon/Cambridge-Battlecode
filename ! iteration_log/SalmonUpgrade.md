# Salmon upgrade

Changes in `bots/vesper-salmon-upgraded` on top of the original `bots/my-bot/vesper-demo`

## Team stays on one farm spot

The shared target was already in memory, but the first living champion each round got to rewrite it. Cassi acts first, so she kept re-picking while recalling, standing on spawn, or walking back, and dragged the team off the camp they were clearing.

- A champion that is recalling, teleporting, on spawn, or still walking back does not lead. It starts leading again once it is within 8 tiles of the shared target.
- If nobody led the previous round, anyone may re-pick, so the team does not march back to a dead spot.
- A group whose camps are already damaged is finished before the target can change.
- Vision is not shared, so a follower that sees free loot the leader cannot writes that group into the shared slot. The leader keeps a detour it cannot see until it sees the group is gone.

## Gold and gem clusters get taken

Clusters on the route never qualified as detours, and a gold cluster (worth about 21) loses the value-per-round comparison to a wolf pack (about 75), so they were walked past.

- Any visible wolf, gem cluster or gold cluster within 5 tiles is taken before that comparison, on the route or not.
- Ranged champions no longer stand off from clusters. Clusters have no strength, so there is nothing to absorb.

## Draft follows the map's team size

`play()` was still reading a deleted `ROSTER`, which would have crashed the draft and left the game to pick at random.

- 5 champions: Varg, Vendetta, Ren, Willow, Vesper Vale.
- 4 champions: Cassi & Issac, Varg, Vesper Vale, Ren.
- 3 champions: Cassi & Issac, Varg, Vesper Vale.
- Size comes from `cambc.draft.team_size()`. A size outside 3 to 5 uses the nearest roster.
- Checked: elkring drafted the 5 and won in round 725. rows drafted the 4 and won in round 680. No map tried asks for 3.

## Escorted tributes, and only at contested altars

A champion with a full bag walked onto an altar and started tributing even with an enemy standing on it. Fighting now comes before any tribute, and enemies that are themselves tributing are targeted first.

- A bag of 150 gems or more only calls the team when every altar is contested: within 6 tiles of the map centre, or closer to the enemy spawn than to ours.
- If a solo altar exists (near our spawn, or otherwise far from the centre), the carrier goes there alone. A lock does not change that. Tributing at a locked altar attacks the lock.
- In TRIBUTE mode the carrier regroups with the nearest ally, or waits beside the altar, until two allies are within 3 tiles. It gives up waiting after 15 turns.
- Escorts stay beside the carrier. While it channels they farm wolves and clusters within 6 tiles of the altar.
- A carrier under 800 health stays behind the team instead of fighting at the front. Vesper still does not tribute while she is building emblems.
- The carrier is stored in the spare bits of the mode byte. An escort is dropped if the carrier is dead, or after 60 rounds if nobody can see them.

## Dash whenever it gains ground

Issac, Vesper, Vendetta and Ren dash on the way to anything, not only in a fight. The dash does not spend the step, so they dash and then walk.

- The landing tile has to be one the pathfinder reached and closer to the goal than where they stand. Any gain spends the cooldown.
- A dash and another cast cannot happen on the same turn, and the dash may be on cooldown when a fight starts.

## Stop chasing into the enemy spawn

Within 8 tiles of the enemy spawn, a champion only chases an enemy it can kill before that enemy walks home. Otherwise it leaves the zone.

- Time to kill is the steps to get into attack range, plus the enemy's health divided by the combined strength of allies already in range.

## Gold weighs more during farming

Gold is 1.5 times more useful than gems in farming phase