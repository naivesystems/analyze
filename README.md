# NaiveSystems Analyze

NaiveSystems Analyze is a static analysis tool for code security and compliance.
This repository holds the source code for the Community Edition which is free
and open-source. Contact `hello[AT]naivesystems.com` to learn more about the
Enterprise Edition.

## Coding Standards

NaiveSystems Analyze Community Edition currently supports the following coding
standards:

- [MISRA C:2012](https://docs.naivesystems.com/analyze/misrac2012)
- [MISRA C++:2008](https://docs.naivesystems.com/analyze/misracpp2008)
- [AUTOSAR C++14](https://www.autosar.org/fileadmin/standards/R22-11/AP/AUTOSAR_RS_CPP14Guidelines.pdf)
- [Google C++ Style Guide](https://naive.systems/styleguide/cppguide.html)

The Enterprise Edition supports (a) more recent versions of the above coding
standards, (b) other C/C++ coding standards including more security-oriented
rules from CERT and CWE, and (c) many other programming languages.

Refer to our demo repositories (e.g. [analyze-demo](https://github.com/naivesystems/analyze-demo)
and [googlecpp-demo](https://github.com/naivesystems/googlecpp-demo)) to see how
to specify and configure the various coding standards and their rules.

## Getting Started

You may choose to use the prebuilt container images, GitHub Actions, or build
directly from the source code.

### Using prebuilt container images

For projects using Makefiles, run the commands below in your project root:

```
mkdir -p output

podman run --rm \
  -v $PWD:/src:O \
  -v $PWD/.naivesystems:/config:Z \
  -v $PWD/output:/output:Z \
  ccr.ccs.tencentyun.com/naivesystems/analyze:2023.3.0.0 \
  /opt/naivesystems/misra_analyzer -show_results
```

A few notes:

* You may use `docker` instead of `podman` here.
  * Read the wiki to learn more about how to run on
    [Windows](https://github.com/naivesystems/analyze/wiki/How-to-run-on-Windows)
    and
    [macOS](https://github.com/naivesystems/analyze/wiki/How-to-run-on-macOS).
  * Running on Linux with `podman` is the only officially supported way in the
    Community Edition.

* You must configure the rules in `.naivesystems/check_rules`.
  - Refer to
    [analyze-demo](https://github.com/naivesystems/analyze-demo/blob/master/.naivesystems/check_rules)
    for an example.
  - Most (if not all) supported rules are listed in `rulesets/*.check_rules.txt` in this repository.

* You may remove `:Z` if you are not using SELinux.

* Replace `2023.3.0.0` with the actual version that you want to use.

NaiveSystems Analyze can trace and capture your build process automatically.
Currently we only publish Fedora-based images in the Community Edition, so your
code must compile successfully under Fedora Linux in order to use the prebuilt
container images. For other operating systems such as Debian, Ubuntu, CentOS, or
RHEL, please reach out to us to get the Enterprise Edition.

The analysis results are also available in the `output` directory. You may use
our [VS Code Extension](https://marketplace.visualstudio.com/items?itemName=naivesystems.analyze)
to view the results in Visual Studio Code.

In addition to Makefiles, we support many other project types. See also:

* [How to analyze CMake projects](https://github.com/naivesystems/analyze/wiki/How-to-analyze-CMake-projects)
* [How to analyze Keil MDK projects](https://github.com/naivesystems/analyze/wiki/How-to-analyze-Keil-MDK-projects)

### Using GitHub Actions

NaiveSystems Analyze supports running directly in GitHub Actions. For example,
[googlecpp-action](https://github.com/naivesystems/googlecpp-action) is our
officially published action for checking the Google C++ Style Guide. Refer to
[googlecpp-demo](https://github.com/naivesystems/googlecpp-demo) for more
information.

### Building from source

To build from source, follow the steps below on Fedora 36 or 37. Other versions
may also work but are not officially supported in the Community Edition.

1. Install build dependencies

```
dnf install -y autoconf automake clang cmake libtool lld make python3-devel wget which xz zip
```

2. Install Go 1.18 or later by following [the official instructions](https://go.dev/doc/install).

3. Install Bazel 6.0 or later by following [the official instructions](https://bazel.build/install).

4. Build the project

```
make
```

3. Build a container image

```
make -C podman_image build-en
```

This will build an image named `naive.systems/analyzer/misra:dev_en` for MISRA
C:2012. You may specify other targets if needed. Read the code for more details.

NaiveSystems Analyze can be built on a variety of Linux distros. For example,
the Community Edition in this repository can be built in GitHub Actions with the
official runner image of Ubuntu 22.04 LTS. For other operating systems such as
Debian, Ubuntu 18.04/20.04 LTS, CentOS 7/8, or RHEL and its derivatives, please
reach out to us to get the Enterprise Edition.

## License

The Community Edition of NaiveSystems Analyze is licensed under the GNU General
Public License version 3. Some subcomponents may have separate licenses. See
their respective subdirectories in this repository for details.

The Enterprise Edition is offered in separate licenses and terms. Contact us to
learn more.
