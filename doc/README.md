# 文档中心

本目录提供 dual-mode-uart-enhanced 的详细工程文档。根目录 [README](../README.md) 用于项目首页展示，这里则按“架构、模块、配置、运维、安全”拆分说明，便于团队协作和二次开发。

## 建议阅读顺序

1. [系统架构](architecture/README.md)
2. [模块说明](modules/README.md)
3. [配置与接口](configuration/README.md)
4. [构建、烧录与运维](operation/README.md)
5. [安全设计](security/README.md)

## 文档索引

| 文档 | 适用对象 | 主要内容 |
| --- | --- | --- |
| [系统架构](architecture/README.md) | 新接手开发者、架构设计者 | 启动流程、主循环、数据流、任务模型 |
| [模块说明](modules/README.md) | 开发者 | 每个 .ino/.h 文件的职责、关键函数、扩展位置 |
| [配置与接口](configuration/README.md) | 调试工程师、固件维护者 | 引脚表、EEPROM 布局、AT 指令、Web 配置入口 |
| [构建、烧录与运维](operation/README.md) | 测试、交付、现场工程师 | 依赖、编译、烧录、启动流程、日志目录、Web 路由 |
| [安全设计](security/README.md) | 固件开发、安全评审 | 安全帧、白名单、错误计数、超时与敏感信息脱敏 |

## 相关资源

- [项目首页](../README.md)
- [PCB PDF](../PCB/dual-mode-uart-enhanced.pdf)
- [Arduino CLI 编译脚本](../compile.ps1)
- [一键烧录脚本](../build_and_flash.ps1)

## 文档维护约定

- 根目录 README 只保留项目概览、快速开始和文档导航。
- 设计细节、参数说明和运维手册统一沉淀在 doc 目录。
- 修改模块行为时，应同步更新对应的专题文档。