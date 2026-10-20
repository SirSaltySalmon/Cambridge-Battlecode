**v84 = v83 + 5 changes** (Mike's branch, not on main yet)

Each change was tested alone as "v83 + that change" vs v83 (34 games normal fuel + 34 unlimited, all 17 maps from both sides), then together. Wins out of 68:

- **New builds** (@Nic please check, it's your area): Stellar Durability Health → Home Health → Power Focus. Ren Durability Health → Bruiser Health → Power Focus. **38-30**, gems +2% / +11%
- **Gold focus**: Stellar and Ren always take camp captures (before: Ren only, until his 2nd emblem), so the gold goes to them. **35-33**, gems +9% / +15%. Taking it back out of v84 drops us to 29-39.
- **Locks off**: we no longer buy or place altar locks. **38-30**, gems +10% / +9%. Mixed against a bot that locks every altar, so the ladder test decides.
- **No mage-cheese cutoff**: mages keep casting at camps from outside their reach after 4 emblems. **35-33** (neutral)
- **Fuel**: while fighting, the team keeps its target instead of re-picking it every turn, and a hurt champion stops leading. **37-31**, gems +14% / +11%

**v84 vs v83: 37-31**, more gems in both fuel modes. Release build: 18-16, 10 turns over 100k fuel in 34 games (v83: 18). Level with v83 against the brawler and v80-5.

Kept from v83: aggr (aggressive farming stays on our side of the map), bold (fight threshold 5 → 0), hold (3-round memory so we stop flip-flopping between farm and fight), step, ward, split, race.

Dropped:
- emblem recall (recall as soon as an emblem is affordable): 37-31 alone, but it made the combined bot worse
- tribute every bag: 29-39
- skip gold clusters: no gem gain
- fight threshold −10: 33-35

⚠️ ±3-4 wins per 34 games is noise. Next: unranked ladder games vs top 50, top 100 and teams that beat us in ranked, then v82 goes back live.

Code: `bots/v84`. Each change is a switch in `constants.h`.
