# LanLink C++ / Qt 0.5.0 preview

原生 C++17 + Qt6 Widgets/Network/Sql 实现，无 Python 运行时。采用 CMake；Windows 推荐 Qt 6.8.3 MSVC2022 x64。

## 构建
```sh
cmake -S cpp -B build -DCMAKE_PREFIX_PATH=<Qt路径>
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```
解压便携包完整目录后运行 LanLink.exe。保持 Qt DLL、plugins 和许可证文件，不可只复制 EXE。未签名开发预览；不要关闭安全软件。

## 范围
局域网发现、手动 IP、单聊与固定成员群聊、SQLite 历史和发送端离线队列、字体/字号、表情、区域截图/图片、昵称/个人资料、单文件传输与续传、只读共享、逐人确认的单/群投屏、点击加载网页标题预览。

- 窄联系人窗口与独立会话窗口按飞鸽9.2公开截图布局实现，不复制其品牌或素材；不是原厂软件，也不保证逐像素一致。
- 离线消息为发送端暂存；双方需最终同时运行且可达。没有云端存储。
- 投屏仅低帧率 JPEG，无声音和远控，须双方同意。
- 链接仅点击后访问，当前限标准端口IPv4公网网页标题，不跟随重定向、不支持压缩/分块响应。不是完整浏览器预览。
- 文件为单文件，不能一键递归发送目录。共享只读，撤销中止相关票据。最终发布依赖硬链接能力，不支持的文件系统将报错并保留partial。
- 无加密和设备身份认证，仅可信局域网开发测试。不是生产安全验收版本。
- C++ 默认使用独立 ~/.lanlink-cpp 数据目录，保留原 Python ~/.lanlink 数据不自动导入；不共享并发写同一数据库。
- 窗口、字体等使用独立 LanLinkCpp 设置。Python版本保留在原构建分支。

Windows Actions 编译和启动截图不等价两台实体电脑的网络、截图、性能和投屏验收。请勿将自动化通过描述为量产验收。
