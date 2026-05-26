# Versioning Convention

## Rule

**Base version follows Solar2D upstream (coronalabs/corona).**

Our tags append a suffix to distinguish our branches:

```
<BASE_VERSION>[.<SUFFIX>[.v<N>]]
```

- `BASE_VERSION` = Solar2D upstream tag number (e.g. `3730`)
- `SUFFIX` = branch identifier (`bgfx`, `b3`, etc.)
- `.v<N>` = sub-version increment when the same branch gets updated without upstream bump
- Master branch uses bare `.v<N>` (no suffix): e.g. `3730.v1`

## Examples

| Branch | Tag | When to bump |
|---|---|---|
| master | `3730.v1` → `3730.v2` | Per upstream release |
| bgfx-solar2d | `3730.bgfx.v1` → `3730.bgfx.v2` | Per bgfx feature update |
| dev_add_box2d_v3 | `3730.b3.v1` → `3730.b3.v2` | Per box2d feature update |
| (next upstream) | `3731.v1` | Only after upstream tags 3731 |
| (upstream + bgfx) | `3731.bgfx.v1` | Only after upstream tags 3731 |

## DO NOT

- ❌ Do not invent version numbers out of thin air (e.g. 3742, 9999)
- ❌ Do not use plain numeric tags (3731, 3732, etc.) — those belong to upstream only
- ❌ Do not use workflow_dispatch defaults (buildNumber=9999, buildYear=2100) for releases

## How CI enforces this

`tools/GHAction/daily_env.sh` extracts BUILD_NUMBER **directly from the git tag** when a tag is pushed (unconditionally, ignoring input defaults). This ensures the release version always matches the tag name.
