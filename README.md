# TWFC player mip fix — experimental

This is an injectable **32-bit DLL** for the examined Transformers: War for Cybertron `TWFC.exe`. Static analysis found a character-texture residency helper that sets a one-mip reduction, and a matching streaming branch that subtracts that reduction from the available mip count. The DLL changes the helper to write zero while preserving forced residency.

## Use

For Ultimate ASI Loader, use the **Win32/x86** loader as `dinput8.dll` beside `TWFC.exe`. Put the included `TWFC_PlayerMipFix.asi` in `Binaries\scripts` and the included `dinput8.ini` beside `dinput8.dll`. Restart the game. The configuration enables `dinput8.log`; the patch writes `TWFC_PlayerMipFix.log` beside its ASI (inside `scripts` in this setup). Do not install both the ASI and DLL as separately loaded plugins.

1. Start the supported game executable.
2. Use your existing **32-bit DLL injector** to load `TWFC_PlayerMipFix.dll` into `TWFC.exe`, preferably at the main menu **before entering a level**. The DLL applies its patch automatically; no export call is required. An injector is not included.
3. Check `TWFC_PlayerMipFix.log` beside the DLL. Look for `apply: Success (code 0)`. A failed log-file write does not prevent the patch; the same message is also sent to Windows debug output.
4. Enter a level and compare the player character with the unmodified game under identical settings. Test both robot and vehicle forms, and respawning/changing characters.

If injected after textures were already marked resident, **return to the menu and enter a fresh level, or respawn/change character** so the game's helper runs again. The DLL changes future field assignments; it does not rewrite existing texture objects or invoke engine functions from its worker thread. If those actions do not refresh the textures, restart and inject before entering a level.

If it has no effect despite a successful application and fresh character load, the static finding is insufficient to explain your observed rendering. Useful next evidence is a live inspection of the character texture's total/requested/resident mip counts and the reduction field before and after the patch.

The patch is applied to process memory only. It does not edit the executable, assets, configuration, or texture groups. Engine limits on available/cooked mips and cached LOD bias remain in effect. Only textures processed by these game-specific residency helpers receive the corrected field value; this can also include a character preview or another character explicitly preloaded through the same helper. It is not an object-identity filter exclusively for the player.

## Build

Requires Visual Studio / Build Tools with **Desktop development with C++**, x86 compiler tools and a Windows SDK. No third-party hooking library is required.

Run `build.cmd` from this directory. It locates Visual Studio, selects the x86 environment, and builds the DLL and test executable. If using a developer prompt, it must target x86.

Alternatively:

```text
cmake -S . -B cmake-build -A Win32
cmake --build cmake-build --config Release
ctest --test-dir cmake-build -C Release --output-on-failure
```

`test.cmd` runs the included synthetic-image tests. They check relocation, RX page protection, exactly two byte changes, idempotence, restoration, unsupported-build rejection, mismatched signatures, inconsistent patch states and refusal to overwrite another modification. They do not render the game or prove visual improvement.

## Supported executable

The examined file has:

| Item | Value |
|---|---|
| Architecture | PE32 / x86 |
| Preferred image base | `0x10900000` |
| PE timestamp | `0x4BF33BE6` |
| SizeOfImage | `0x0250A000` |
| File SHA-256 | `c06a98ee8648727ad6312227f66a3cd9aef5dc9d2ada67fcc4ab2eaa9692be27` |

The DLL checks process name, architecture, timestamp, image size and six instruction signatures before writing anything. It uses relative addresses to support relocation. The SHA-256 is a reference fingerprint, not a runtime check. Other editions are refused if their headers or relevant bytes differ; do not remove the guards to force compatibility.

## Patch and rollback

| Examined VA | RVA of changed byte | Original → replacement | Effect |
|---|---|---|---|
| `0x10BEC1BB` | `0x002EC1BC` | `6E → 5E` | `mov [esi+0x70],ebp` → `mov [esi+0x70],ebx`; EBP is 1 and EBX is 0 here |
| `0x10BEC22B` | `0x002EC22E` | `01 → 00` | Single-texture helper writes 0 instead of 1 |

ESI/EAX at these sites refer to a wrapper at `UTexture2D + 0x10`, so wrapper offset `0x70` corresponds to texture offset `0x80`. The nearby `OR ..., 4` setting forced residency remains intact. No trampoline, detour, API hook, persistent polling thread or external process access is used.

Exports use the Windows calling convention, take no arguments, and return a DWORD:

- `ApplyFix`: apply, or succeed if already applied.
- `RestoreFix`: restore both original bytes, or succeed if already restored.
- `GetFixStatus`: result of the latest operation; 256 means initialization pending. A result of zero means that operation succeeded, so check the log to distinguish apply from restore.

A compatible injector/tool can call `RestoreFix` in the game process. Otherwise, exit the game to discard the memory patch. Restoring the code does not immediately change fields already written into texture objects; use a fresh character load or restart for a clean comparison. The DLL pins itself for the process lifetime to prevent unloading during initialization/export calls; it does not restore automatically on an unload attempt.

Codes: 0 success; 1 wrong process; 2 unsupported build; 3 signature mismatch; 4 memory protection failure; 5 cache flush failure with byte rollback; 6 inconsistent partial patch; 256 pending. Code 4 during protection restoration can occur after the byte writes; do not interpret it as proof that neither byte changed. Calls are serialized within the DLL and each modified byte store is atomic on x86. This is not a global transaction against unrelated third-party patchers.

The game-specific helper may have used the one-mip reduction intentionally to save memory. Loading the highest available mip increases memory use for those textures. The patch fixes the observed arithmetic rather than establishing the original developer's intent.

See `ANALYSIS.md` for the static evidence chain and verification limits.