# docc-to-md

Generate doc for a C project into markdown

> *Pronounced "dok C to MD"*

## Usage

Get a markdown documentation for a C projet, for example for a [Forgejo wiki page](https://forgejo.org/docs/latest/user/getting-started/wiki/)

### Arguments list

When the binary is compiler (see [installation](#installation) to compile it/download it), you can run it with these arguments :

| Argument | Type | Effect | State | Default |
|:--------:|:----:|:-------|:------|:-------:|
| `--src` | *String* | Specify the entry point of the project to read from | **Required** | **N/A** |
|  `--dest` | *String* | Specify the path of the output markdown. If is is not found, it will be created | **Required** | **N/A** |
| `--work` | *String* | Specify the work directory of the project. It will be used to put temp files for comment removal | *Optional* | `/tmp/docc-to-md` |
| `--help` | *Toggle* | Displays help page and exits. If specified, has priority on any other argument | *Optional* | **N/A** |
|  `--version` | *Toggle* | Displays program version and exits. If specified, has lower priority than `--help`, but priority over anything else | *Optional* | **N/A** |
| `-d` | *Toggle* | Enables dump. This is to get a raw output of the parsed content | *Optional* | **N/A** |
| `--dump` | *String* | Specify dump files path. If not exists, it will be created and truncated | **Required if and only if `-d` specified | **N/A** |
| `--verbose-registration` | *Toggle* | If specified, it will enable the print to the standard output of the name and type of each element registered, the summary of the parsing and the summary of the linkage | *Optional* | **N/A** |
| `--enable-docstirng` | *Optional* | At the cost of more computing, enable the linkage of docstring to functions, typedefs, macros and structures. It finds the content within `/*` and `*/` above the mentionned items | *Optional* | **N/A** |
| `--rebase-path` | *String* | If not specified, the **absolute path** of the local files will be shown, as display and link in documentation. If specified : in links, the display will be the individual filenames, relative to the root of the project (specified by `--src`), and the link will be pointing to whatever this argument specifies immediately followed by the relative path of the file to the project root | *Optional* | **N/A** |
| `--git-line` | *Toggle* | If enabled, add `#line-number-reference` of the elements in the link of their definition file. it is useful for git repositories so it links directly to the definition. Useful in large files | *Optional* | **N/A** |
| `--reference-files` | *Toggle* | If enabled, add a section at the end of each documentation file, specifying which file are referenced inside the specific documentation file | *Optional* | **N/A** |
| `--pre-linkage` | *Toggle* | If enabled, includes an additional step between parsing and linkage. It is used to enhance the resulting documentation, by sorting objects by name (using quick sort). On very large projects with a lot of objects, it might not be recommended | **N/A** |

### Usage examples

As previously stated, this was originnaly designed to generate documentation for [Forgejo wiki pages](https://forgejo.org/docs/latest/user/getting-started/wiki/). It could be applicable to other services as well
> Please [contact me](#contact) if you want to improve/suggest anything

This repository contains a [wiki creation and synchronisation workflow](./.forgejo/workflows/wiki.yml). Here are the relevant parts :

```bash
# Compiling step
/usr/bin/docc-to-md --src src --dest "$WIKI_DIR" --verbose-registration --enable-docstring --reference-files --git-line --pre-linkage --rebase-path $URL/src/branch/main/

# Later on :
## Publishing step
WIKI_URL="$SERVER_URL/$REPO_NAME.wiki.git"
AUTH_HEADER="Authorization: Basic $(printf '%s' "$WIKI_USERNAME:$WIKI_TOKEN" | base64 | tr -d '\n')"

rm -rf wiki
if git clone "$WIKI_URL" wiki; then
  echo "Wiki cloned"
else
  mkdir wiki
  git -C wiki init
  git -C wiki checkout -b main
fi

find wiki -maxdepth 1 -type f -name 'api-*.md' -delete
find wiki -maxdepth 1 -type f -name 'api.md' -delete

rm -rf wiki/*
cp -v "$WIKI_DIR"/*.md wiki/

cd wiki
git add .

git -c user.name="forgejo docs" \
  -c user.email="docs@noreply.$SERVER_URL" \
  commit -m "Update generated API documentation" || true

if ! git remote get-url origin >/dev/null 2>&1; then
  git remote add origin "$WIKI_URL"
fi

git -c http.extraheader="$AUTH_HEADER" push -u origin HEAD
```

> ⚠️
> Some forgejo instances cannot programmatically create a wiki if it doesn't exist. That's why the original workflow has a message on failure

### Examples

See the [wiki page of this repository](https://git.greensky.tf/Greensky/docc-to-md/wiki), it has been generated with docc-to-md

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

> **Compile arguments list**
> You can include these arguments right in the compilation command, for example : `make DBG_POSITION`

| Argument | Type | Effect |
|:--------:|:----:|:------:|
| `NODEBUG` | Int | Disable debug macros if set to anything |
| `DISABLE_DEBUG` | int | Same as `NODEBUG` |
| `DBG_POSITION` | int | Enables position prefix in debugging macros if set to anything |
| `DBG_BLOCK` | int | Enables the real time clock in debug macros if set to anything |
| `DBG_NO_COL` | int | Disables default  color in debug macros if set to anything |

> *`cleanbuild` target comes with `NODEBUG` flag*

### From PKGBUILD (arch)

If you are using arch, you can install it through the PKGBUILD file :

```bash
curl -o PKGBUILD https://git.greensky.tf/Greensky/docc-to-md/src/branch/master/PKGBUILD
makepkg -siCcf
```

### From repository

The [PKGBUILDING](https://git.greensky.tf/Greensky/PKGBUILDING) repository continuously compiles a version of this file and publishes it

> [See more detailled instructions](https://git.greensky.tf/Greensky/-/packages/docc-to-md)

## Roadmap

This is essentially a list of things to do, so I don't forget

| What | Importance | Notes | Status |
|:----:|:----------:|:-----:|:------:|
| Macros parsing | High | `N/A` | ✅ |
| Structures parsing | Necessary | `N/A` | ✅ |
| Functions parsing | Necessary | `N/A` | ✅ |
| Typedef parsing | Necessary | `N/A` | ✅ |
| Typedef functions parsing | High | Typedefs don't register functions typedefs for now | ✅ |
| Include docstring for other types | Medium | For now, only the functions take the docstring | ✅ |
| Unamed functions arguments resolving | Low | Finish the resolving of unamed function arguments. This is mainly for arguments that are custom structs/enums/typedefs or standard types that don't fit in one word | ✅ |
| Types resolving | Necessary | Needs the typedef parsing to be done | ✅ |
| Markdown writing | Necessary | Needs types resolved | ✅ |
| Pre Linkage | Optional | Add a prelinkage phase to do things like sorting types | ✅ |
| Type modifiers | Important | Introduce type modifiers (like constant, volatile...) | ✅ |
| Windows Portability | Low | Check installation steps on [Windows intall](#installation) | ✅ |

> Legend :<br>
> 🚧 : Planned<br>
> 🏗️ : Working on it<br>
> ❌ : Aborted<br>
> ✅ : Done

## Contact

Since the code makes a lot of assumptions and purposely ignores some valid C cases, like :
```c
enum Thing
{

};
```
is not recognized as an enum.

If you want to help to improve the project, and/or have any request regarding the project, please contact me on [this discord server](https://discord.gg/fHyN5w84g6), [My matrix space](https://matrix.to/#/#greensky-dev-space:matrix.greensky.tf), or by email
