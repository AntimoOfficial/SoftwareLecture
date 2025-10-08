# 贡献指南 (Contribution Guide)

欢迎为本项目做出贡献！为保证协作效率，请所有成员遵守以下约定。

## 🚀 Git 工作流 (Git Workflow)

1.  **分支管理:**
    - `main` 分支为受保护的稳定分支，只接受来自 `dev` 或 `feature` 分支的 Pull Request (PR)。
    - 日常开发请在 `dev` 分支进行。
    - 对于一个较大的独立功能，请从 `dev` 分支创建新的 `feature` 分支，如 `feature/gui-redesign`。

2.  **提交代码:**
    - 在 `push` 代码前，请确保在本地编译通过且功能正常。
    - 提交信息 (Commit Message) 请使用清晰的英文或中文描述，例如: `feat: add backup compression using zlib` 或 `修复: 解决了还原时文件权限丢失的问题`。

## 🎨 代码风格 (Code Style)

- 我们将采用 Google C++ Style Guide 作为代码规范。
- 请在 IDE 中配置好代码格式化工具，确保提交的代码格式统一。

## ✅ Pull Request (PR)

- 当一个功能开发完成后，请提交 Pull Request 到 `dev` 分支。
- PR 需要至少一名其他成员进行代码审查 (Code Review) 并批准后，方可合并。