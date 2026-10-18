# Building Enshrouded VR

The source of the mod is in `vrmod`. It is built on Windows with Visual Studio and CMake.

## Folder layout

The development layout puts two folders into the Enshrouded game folder (the one with `enshrouded.exe`):

```
Enshrouded\
  enshrouded.exe
  vrmod\        <- this source
  references\   <- third-party SDKs (not included here, see below)
```

## Third-party SDKs (`references`)

`vrmod\CMakeLists.txt` takes them from `..\references`:

| Folder | What | Needed for |
|---|---|---|
| `references\OpenXR-SDK` | Khronos OpenXR SDK (loader and headers) | the mod |
| `references\Vulkan-Headers` | Khronos Vulkan headers | the mod |
| `references\minhook` | MinHook (TsudaKageyu/minhook) | the mod |
| `references\renderdoc\1.46\RenderDoc_1.46_64` | RenderDoc 1.46 (portable): the in-application API header, `glslangValidator.exe` | the mod, the shaders |
| `references\renderdoc\source-v1.46` | RenderDoc 1.46 source (replay API) | the capture analysis tool only |
| `references\OpenXR-SDK-Source` | OpenXR SDK source (hello_xr shaders) | some tests only |
| `references\openvr` | Valve OpenVR SDK | the virtual headset test tools only |
| `references\vulkan-sdk` | Vulkan SDK (portable) | some tests only |

## Build

Visual Studio 2019 (Desktop development with C++) and CMake 3.20 or newer:

```
cmake -S vrmod -B vrmod\build -G "Visual Studio 16 2019" -A x64
cmake --build vrmod\build --config Release
ctest --test-dir vrmod\build -C Release
```

(`vrmod\build.ps1` does the same with the CMake that `vrmod\tools\bootstrap.ps1` puts into `vrmod\tools\python`.)

## Package and release

```
pwsh vrmod\tools\pack_release.ps1 -Version x.y.z -ReleaseZip
```

makes `vrmod\dist\package-<time>` (everything that goes next to `enshrouded.exe`) and `vrmod\release\EnshroudedVR-x.y.z.zip`.
`-Install` also copies the package into the game folder. The update button of "Enshrouded VR setting.exe"
(`vrmod\package\Update-EnshroudedVR.ps1`) installs the latest GitHub release of the repository named in the package's
manifest (`-Repository`, default `momotori01/ENSHROUDED-VR-MOD`): the release's tag must be `EnshroudedVR-x.y.z` and its
asset `EnshroudedVR-x.y.z.zip`.

## Game updates

The mod finds the game's functions by their addresses and byte patterns for one build of `enshrouded.exe`
(`vrmod\include\evr\game_build.hpp`, `vrmod\config\profiles`). After a game update the mod switches itself off; the
functions are matched to the new build with `vrmod\tools\match_functions.py`, `apply_rematch.py` and `shift_rematch.py`
(Python 3 with `pefile` and `capstone`).

## Not included

The development logs and notes, the test saves and the Mock test data (`vrmod\test_env`), and the bundled Python of
`vrmod\tools\python`.
