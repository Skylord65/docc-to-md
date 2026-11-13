# docc-to-md

Generate doc for a C project into markdown

## Usage

Get a markdown documentation for a C projet, for example for a [Forgejo wiki page](https://forgejo.org/docs/latest/user/getting-started/wiki/)

## Installation

### From source

```bash
git clone https://git.greensky.tf/Greensky/docc-to-md && cd docc-to-md
make cleanbuild
sudo mv bin/main.uwu /usr/bin/docc-to-md
```

or :
```bash
git clone https://git.greeensky.tf/docc-to-md && cd docc-to-md
cmake CMakeLists.txt
make -f Makefile
```

### From PKGBUILD (arch)

If you are using arch, you can install it through the PKGBUILD file :

```bash
curl -o PKGBUILD https://git.greensky.tf/Greensky/docc-to-md/src/branch/master/PKGBUILD
makepkg -siCcf
```

### From repository

When it will be finished, it will be released, it will be available on my [forgejo packages repository page](https://git.greensky.tf/Greensky/-/packages), auto compiled and published by the [PKGBUILDING](https://git.greensky.tf/Greensky/PKGBUILDING) repository

## Roadmap

This is essentially a list of things to do, so I don't forget

| What | Importance | Notes | Status |
|:----:|:----------:|:-----:|:------:|
| Macros parsing | High | `N/A` | ✅ |
| Structures parsing | Necessary | `N/A` | ✅ |
| Functions parsing | Necessary | `N/A` | ✅ |
| Typedef parsing | Necessary | `N/A` | ✅ |
| Typedef functions parsing | High | Typedefs don't register functions typedefs for now | ✅ |
| Include docstring for other types | Medium | For now, only the functions take the docstring | 🚧 |
| Unamed functions arguments resolving | Low | Finish the resolving of unamed function arguments. This is mainly for arguments that are custom structs/enums/typedefs or standard types that don't fit in one word | 🚧 |
| Types resolving | Necessary | Needs the typedef parsing to be done | 🏗️ |
| Markdown writing | Necessary | Needs types resolved | 🚧 |
| Pre Linkage | Optional | Add a prelinkage phase to do things like sorting types | 🚧 |
| Type modifiers | Important | Introduce type modifiers (like constant, volatile...) | 🚧 |
| Windows Portability | Low | `N/A` | 🏗️ |

> Legend :
> 🚧 : Planned
> 🏗️ : Working on it
> ❌ : Aborted
> ✅ : Done

## Contact

Since the code makes a lot of assumptions and purposely ignores some valid C cases, like :
```c
enum Thing
{

};
```
is not recognized as an enum.

If you want to help to improve the project, and/or have any request regarding the project, please contact me on [this discord server](https://discord.gg/fHyN5w84g6), or by email
