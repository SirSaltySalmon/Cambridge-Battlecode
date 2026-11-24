import sys
sys.path.insert(0, 'tools/hc')
from patch import Patch
p = Patch('bots/%s/src/constants.h' % sys.argv[1])
p.rep("""inline constexpr Item VESPER_FALLBACK[] = {Item::power_shard_of_health, Item::power_shard_of_strength,
    Item::power_shard_of_strength};""", """inline constexpr Item VESPER_FALLBACK[] = {Item::greed_shard_of_strength};""")
p.rep("""inline constexpr Item MARCELLA_FALLBACK[] = {Item::power_shard_of_strength, Item::power_shard_of_health};""",
      """inline constexpr Item MARCELLA_FALLBACK[] = {Item::greed_shard_of_strength};""")
p.rep("""inline constexpr Item PRISM_FALLBACK[] = {Item::power_shard_of_focus, Item::power_shard_of_health};""",
      """inline constexpr Item PRISM_FALLBACK[] = {Item::greed_shard_of_focus};""")
p.save()
