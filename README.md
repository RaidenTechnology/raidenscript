# RaidenScript

> **Phase 1 complete — the language runs.**
> Lexer, parser, resolver, tree-walking interpreter and REPL all work.
> Types, bytecode VM and embedding come next. Expect breaking changes before v1.0.

**RaidenScript** (**RS**) is an embeddable scripting language. You can write
standalone programs with it, but its real purpose is to go *inside* another
application and make it programmable: mods for a game, plugins for a server,
automation for a desktop app.

```python
fn greet(name: str) -> str:
    return f"Hello {name}"

class Ship(Entity):
    hp: int = 100
    fn draw(self, painter):
        painter.sprite("ship", self.x, self.y)

# Declarative UI lives in the language, not in a separate template file
view Hud(player):
    <column gap=4 pad=8>
        <bar value={player.hp} max=100 color="#e33"/>
        <button on_click={() => openShop()}>SHOP</button>
    </column>
```

It borrows a different layer from each of four languages:

| Source | What it contributes |
|---|---|
| **Python** | Readability — indentation blocks, f-strings, little punctuation |
| **JavaScript** | Runtime model — closures, `async`/`await`, event loop, arrow functions |
| **C++** | Optional static types, performance awareness, native embeddability |
| **HTML** | `view` blocks — declarative UI inside the language itself |

## Try it

Needs a C++20 compiler and `make`. On Windows,
[w64devkit](https://github.com/skeeto/w64devkit) is the easiest option —
`build.ps1` finds it automatically without touching your PATH.

```bash
make                                       # or: powershell -File build.ps1
./build/rs run examples/01-temeller.rai
./build/rs                                 # REPL
```

```
>>> fn square(x):
...     return x * x
...
>>> square(square(3))
81
>>> [1,2,3].map((x) => x * 10)
[10, 20, 30]
```

## Design decisions worth knowing

These are the choices that give the language its character. The rationale for
each one is in [`SPEC.md`](SPEC.md).

- **Only `nil` and `false` are falsy.** `0` and `""` are truthy. This kills the
  classic `if count:` bug at the root.
- **`/` always returns a float**, `//` is integer division. C's silent truncation
  was rejected as a bug factory.
- **No package registry.** Modules resolve straight from a git repository
  (Go's model) — no server, no account, no infrastructure to run.
- **`import` and `include` differ by *when*, not by *what*.** `import` is resolved
  at runtime — the standard library, a git repository, a pinned version.
  `include` is resolved when the host builds you in, so it refuses quoted paths and
  version tags: on an ESP32 there is no filesystem and nothing to fetch. Hardware
  and software separate as a side effect, and the rule is one the compiler can
  actually enforce. A file may use both — that is what a bridge looks like.
- **The core stays small.** 29 keywords, hard-capped at 30 until v1.0. Power comes
  from host bindings, not from the language growing.
- **Diagnostics were built before the lexer.** Errors carry line/column, a source
  excerpt, a caret and a hint — and the column counts UTF-8 *characters*, so it
  lands correctly in non-ASCII source.

```
error: unexpected '!'
 --> test.rai:3:7
  |
3 | c = 5 ! 3
  |       ^
  |
  = hint: use 'not' for negation
```

## Why embeddable?

The core is deliberately small. Everything domain-specific comes from bindings
the host provides:

| Binding | Host | Provides |
|---|---|---|
| `game.*` | Browser game (WASM) | sprites, audio, input, entities |
| `mc.*` | Minecraft server (JVM) | players, world, events, inventory |
| `sys.*` | Desktop app (Node/native) | files, processes, network |
| `serial.*` | Embedded / telemetry | ports, flight data |
| `ui.*` | all | `view` renderer |

`examples/06-oyun-modu.rai` and `examples/10-minecraft-plugin.rai` are the same
language driving two completely different hosts, with the core untouched.

## Roadmap

| Phase | Work | Status |
|---|---|---|
| 0 | Language spec + example programs | ✅ done |
| 1 | Lexer, parser, AST, resolver, interpreter, REPL | ✅ done |
| 2 | Gradual types, nil-safety, standard library | ⬜ |
| 3 | Bytecode VM + garbage collector | ⬜ |
| 4 | C API, WASM, embedding | ⬜ |
| 5 | `view` — declarative UI | ⬜ |
| 6 | JVM bridge | ⬜ |
| 7 | LSP, formatter, package resolver | ⬜ |
| 8 | Self-hosting | ⬜ |

## Repository layout

| Path | Contents |
|---|---|
| [`SPEC.md`](SPEC.md) | Language definition, grammar, design decisions and rationale |
| [`examples/`](examples/) | 15 example programs — also the regression suite |
| [`src/`](src/) | The implementation (C++20) |
| [`WORKLOG.md`](WORKLOG.md) | Development log, round by round |

`SPEC.md` and `WORKLOG.md` are written in Turkish — they are working documents.
Code, comments and this README are the parts meant for everyone.

## Naming

Full name **RaidenScript**, short **RS**, command `rs`, extension `.rai` — the
first three letters of *Rai*den, and also 雷 (thunder).

`.rs` belongs to Rust, `.rds` to R, `.rsc` to MikroTik RouterOS, `.ra` to RealAudio.

## License

MIT — see [LICENSE](LICENSE). The *RaidenScript* name and the Raiden Technology
brand are not covered by it.

---

Built by [Raiden Technology](https://github.com/RaidenTechnology).

