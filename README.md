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

### From PKGBUILD (arch)

If you are using arch, you can install it through the PKGBUILD file :

```bash
curl -o PKGBUILD https://git.greensky.tf/Greensky/docc-to-md/src/branch/master/PKGBUILD
makepkg -siCcf
```

### From repository

When it will be finished, it will be released, it will be available on my [forgejo packages repository page](https://git.greensky.tf/Greensky/-/packages), auto compiled and published by the [PKGBUILDING](https://git.greensky.tf/Greensky/PKGBUILDING) repository

## Contact

Since the code makes a lot of assumptions and purposely ignores some valid C cases, like :
```c
enum Thing
{

};
```
is not recognized as an enum.

If you want to help to improve the project, and/or have any request regarding the project, please contact me on [this discord server](https://discord.gg/fHyN5w84g6), or by email
