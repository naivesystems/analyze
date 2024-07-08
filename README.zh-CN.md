This is a translation done by ChatGPT.

# NaiveSystems Analyze

NaiveSystems Analyze 是一个用于代码安全和合规性的静态分析工具。此存储库包含社区版的源代码，该版本是免费且开源的。要了解更多关于企业版的信息，请访问[官方网站](https://www.naivesystems.com/)或联系 `hello[AT]naivesystems.com`。

## 编码标准

NaiveSystems Analyze 社区版目前支持以下编码标准：

- [MISRA C:2012](https://docs.naivesystems.com/analyze/misrac2012)
- [MISRA C++:2008](https://docs.naivesystems.com/analyze/misracpp2008)
- [AUTOSAR C++14](https://www.autosar.org/fileadmin/standards/R22-11/AP/AUTOSAR_RS_CPP14Guidelines.pdf)
- [Google C++ 风格指南](https://naive.systems/styleguide/cppguide.html)

企业版支持 (a) 上述编码标准的更新版本，(b) 其他 C/C++ 编码标准，包括 CERT 和 CWE 的更多安全导向规则，以及 (c) 许多其他编程语言。

参考我们的演示仓库（例如 [analyze-demo](https://github.com/naivesystems/analyze-demo) 和 [googlecpp-demo](https://github.com/naivesystems/googlecpp-demo)）来了解如何指定和配置各种编码标准及其规则。

## 入门指南

您可以选择使用预构建的容器镜像、GitHub Actions，或直接从源代码构建。

### 使用预构建容器镜像

对于使用 Makefile 的项目，在项目根目录下运行以下命令：

```
podman pull ccr.ccs.tencentyun.com/naivesystems/analyze:2023.4.1.0

mkdir -p output

podman run --rm \
  -v $PWD:/src:O \
  -v $PWD/.naivesystems:/config:Z \
  -v $PWD/output:/output:Z \
  ccr.ccs.tencentyun.com/naivesystems/analyze:2023.4.1.0 \
  /opt/naivesystems/misra_analyzer -show_results
```

几点说明：

* 您可以使用 `docker` 代替 `podman`。
  * 阅读 wiki 了解更多关于如何在
    [Windows](https://github.com/naivesystems/analyze/wiki/How-to-run-on-Windows) 和
    [macOS](https://github.com/naivesystems/analyze/wiki/How-to-run-on-macOS) 上运行。
  * 在 Linux 上使用 `podman` 是社区版唯一官方支持的方式。

* 您必须在 `.naivesystems/check_rules` 中配置规则。
  - 参考
    [analyze-demo](https://github.com/naivesystems/analyze-demo/blob/master/.naivesystems/check_rules)
    了解示例。
  - 此存储库中的 `rulesets/*.check_rules.txt` 列出了大多数（如果不是全部）支持的规则。

* 如果您不使用 SELinux，可以移除 `:Z`。

* 用您想使用的实际版本号替换 `2023.4.1.0`。

NaiveSystems Analyze 可以自动追踪并捕获您的构建过程。目前我们仅在社区版中发布基于 Fedora 的镜像，因此您的代码必须在 Fedora Linux 下成功编译才能使用预构建的容器镜像。对于 Debian、Ubuntu、CentOS 或 RHEL 等其他操作系统，请联系我们获取企业版。

分析结果也可在 `output` 目录中找到。您可以使用我们的 [VS Code 扩展](https://marketplace.visualstudio.com/items?itemName=naivesystems.analyze) 在 Visual Studio Code 中查看结果。

除了 Makefile，我们还支持许多其他项目类型。另见：

* [如何分析 CMake 项目](https://github.com/naivesystems/analyze/wiki/How-to-analyze-CMake-projects)
* [如何分析 Keil MDK 项目](https://github.com/naivesystems/analyze/wiki/How-to-analyze-Keil-MDK-projects)

### 使用 GitHub Actions

NaiveSystems Analyze 支持在 GitHub Actions 中直接运行。例如，[googlecpp-action](https://github.com/naivesystems/googlecpp-action) 是我们官方发布的用于检查 Google C++ 风格指南的 action。请参阅 [googlecpp-demo](https://github.com/naivesystems/googlecpp-demo) 了解更多信息。

### 从源代码构建

要从源代码构建，请在 Fedora 36 或 37 上遵循以下步骤。社区版中并未官方支持其他版本，但它们可能也适用。

1. 安装构建依赖

```
dnf install -y autoconf automake clang cmake libtool lld make python3-devel wget which xz zip
```

2. 按照 [官方说明](https://go.dev/doc/install) 安装 Go 1.18 或更高版本。

3. 按照 [官方说明](https://bazel.build/install) 安装 Bazel 6.0 或更高版本。

4. 构建项目

```
make
```

5. 构建容器镜像

```
make -C podman_image build-en
```

这将为 MISRA C:2012 构建一个名为 `naive.systems/analyzer/misra:dev_en` 的镜像。如有需要，您可以指定其他目标。阅读代码了解更多细节。

NaiveSystems Analyze 可以在各种 Linux 发行版上构建。例如，本存储库中的社区版可以在官方的 Ubuntu 22.04 LTS 运行器镜像中的 GitHub Actions 中构建。对于 Debian、Ubuntu 18.04/20.04 LTS、CentOS 7/8 或 RHEL 及其衍生版等其他操作系统，请联系我们获取企业版。

## 加入社区

如果您发现 NaiveSystems Analyze 的错误，请随时在 issues 中报告。

如果您使用微信，可以扫描下方的二维码加入我们的群聊：

![20240711](https://github.com/naivesystems/analyze/assets/22113324/5efee839-36a2-459a-82bd-39d39fbfdfd4)

（二维码在过期或群组人数超过 200 人时会更新。）

## 许可证

NaiveSystems Analyze 的社区版根据 GNU 通用公共许可证版本 3 授权。某些子组件可能有不同的许可证。有关详细信息，请参阅此存储库中的相应子目录。

企业版提供不同的许可证和条款。联系我们了解更多。
