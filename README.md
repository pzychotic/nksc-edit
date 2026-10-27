# nksc-edit

[![GitHub License](https://badgen.net/github/license/pzychotic/nksc-edit)](https://github.com/pzychotic/nksc-edit/blob/main/LICENSE)
[![CI](https://github.com/pzychotic/nksc-edit/actions/workflows/ci.yml/badge.svg)](https://github.com/pzychotic/nksc-edit/actions/workflows/ci.yml)

A command line tool to edit Nikon NX Studio sidecar files (`.nksc`) without opening NX Studio.
It can change exposure gain, straighten (roll) angle, crop and white balance (color temperature
and image balance), and dump the edit settings as readable XML.

A `.nksc` file is an XMP packet. All adjustments live as an escaped XML document in its
`nine:NineEdits` element. nksc-edit only rewrites that element; the rest of the file stays
byte for byte as NX Studio wrote it. The project is self-contained: the only dependency,
[tinyxml2](https://github.com/leethomason/tinyxml2), is included in `external/`.

## Usage

```
nksc-edit [options] <file.nksc>
```

| Option | Description |
|---|---|
| `-d`, `--dump` | Write the NineEdits XML to `<file>_NineEdits.xml` and exit |
| `-g`, `--gain <value>` | Exposure gain (EV) |
| `-r`, `--roll <degrees>` | Straighten angle in degrees |
| `-c`, `--crop <x1,y1,x2,y2>` | Crop rectangle in pixels |
| `-t`, `--colortemp <kelvin>` | White balance color temperature |
| `-b`, `--imagebalance <value>` | White balance image balance (tint) |
| `-h`, `--help` | Show help |

Options also accept the `--name=value` form. The file is modified in place, so keep a backup.

```
nksc-edit --dump DSC_0001.NEF.nksc
nksc-edit -g 0.7 -r -1.5 DSC_0001.NEF.nksc
nksc-edit --crop 120,80,5900,3950 --colortemp 5200 --imagebalance 2 DSC_0001.NEF.nksc
```

## Build

Open `nksc-edit.slnx` in Visual Studio 2026, or build from a developer command prompt:

```
msbuild nksc-edit.slnx -p:Configuration=Release -p:Platform=x64
```

The executable is written to `build\x64\Release\nksc-edit.exe`.

## Tests

`nksc-edit-tests` uses the sample files in `tests/data/` and returns a non-zero exit code if a
test fails. Run it from Visual Studio (the debugger working directory is `tests/`) or manually:

```
build\x64\Release\nksc-edit-tests.exe tests\data
```

## Layout

| Path | Content |
|---|---|
| `src/core/` | `nksc-core` static library: file handling, NineEdits editing, base64, XML escaping |
| `src/app/` | `nksc-edit` executable and command line parser |
| `tests/` | `nksc-edit-tests` executable and sample data |
| `external/tinyxml2/` | tinyxml2 11.0.0 (zlib license, see `LICENSE.txt`) |

## License

nksc-edit is licensed under the MIT License, see `LICENSE`. It includes
[tinyxml2](https://github.com/leethomason/tinyxml2) in `external/tinyxml2/`, which is
distributed under the zlib license, see `external/tinyxml2/LICENSE.txt`.
