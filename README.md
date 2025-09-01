# cloudmeeting

工厂-专家双端云会议视频工单交流（Client-Server 架构）项目（Qt 5.12.8，纯 C++，qmake，QtCreator）。

## 环境
- 操作系统：Ubuntu（VMware 虚拟机）
- Qt：5.12.8
- 构建系统：qmake（.pro）
- 编译器：g++/make

## 目录结构
```
test2.1/
├── client
│   ├── client.pro
│   ├── Forms/         # .ui
│   ├── Headers/       # 头文件（含 comm 子目录）
│   ├── Resources/     # 资源（icons/qss/resources.qrc）
│   └── Sources/       # 源码（含 comm 子目录）
└── server
    ├── server.pro
    ├── run_server.sh
    ├── common/        # 协议等
    └── src/           # 服务器源码
```

## 构建
确保 Qt 5.12.8 / qmake 可用（QtCreator 或终端均可）。

终端构建（示例）：
```bash
# Client
cd client
qmake client.pro
make -j$(nproc)

# Server
cd ../server
qmake server.pro
make -j$(nproc)
```

## 运行
```bash
# 服务器（示例脚本）
./run_server.sh
# 客户端（Qt 应用，双端：factory/expert）
```

> 如需进一步优化构建脚本、CI、打包或代码结构，请在 Issue/PR 中说明需求。
