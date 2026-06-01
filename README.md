# 🎬 MP4 → GIF

一个简洁的 MP4 视频转 GIF 动图工具，带 GUI 界面。**打包后为单个 EXE，复制到任意 Windows 10/11 电脑即可运行。**

A simple MP4-to-GIF converter with GUI. **Packaged as a single EXE, runs on any Windows 10/11 PC.**

---

## 功能 Features

- **GUI 界面** — 无需命令行，所有操作可视化
- **预设尺寸** — 微信表情包 (300px)、小 (200px)、中 (480px)、大 (720px)、原始尺寸、自定义
- **保持比例** — 勾选后只需设置最短边像素数，自动计算另一边
- **画质控制** — 最佳 / 高 / 中等 / 低（控制颜色数和抖动）
- **帧率可调** — 默认 15 fps
- **实时预览** — 导入视频后显示第一帧，转换完成后显示结果
- **文件大小预估** — 修改参数时实时更新预计大小
- **中文界面** — 微软雅黑字体，清爽白色背景

## 截图 Screenshot

```
┌─────────────────────────────────────────────────────┐
│              MP4 → GIF 转换器                         │
│ 输入视频: [________________________] [浏览...]       │
│ 输出GIF:  [________________________] [浏览...]       │
│ 视频: 1920×1080 | 10.0秒 | 30.0fps    ┌──────────┐ │
│ ──────────────────────────────────     │ 输入视频  │ │
│ 尺寸设置                               │          │ │
│ 预设: [微信表情包 (300px) ▼]           │ (预览图) │ │
│ ☑ 保持原始宽高比 (仅需设置最短边)      │          │ │
│ 最短边: [300] 像素                     └──────────┘ │
│ 输出尺寸: 300 × 534   预计大小: 1.2MB (150帧)      │
│ ──────────────────────────────────     ┌──────────┐ │
│ 画质: [最佳 ▼]  帧率: [15] fps        │ 输出GIF   │ │
│ [          开始转换          ]        │          │ │
│ ████████████████████████░░░░ 80%      │ (结果图) │ │
│ 就绪                                   │          │ │
└─────────────────────────────────────┴──────────┘ │
```

## 快速开始 Quick Start

### 直接运行 Run Directly

```bash
pip install -r requirements.txt
python mp4_to_gif.py
```

### 打包为 EXE Build EXE

```bash
# 生成图标 (首次)
python make_icon.py

# 打包
pyinstaller --onefile --windowed --icon=icon.ico --name mp4_to_gif mp4_to_gif.py

# EXE 在 dist\mp4_to_gif.exe (~50 MB)
```

或直接双击 `build_exe.bat`。

## 依赖 Dependencies

- Python 3.8+
- `opencv-python-headless` — 视频解码
- `numpy` — 数组运算
- `Pillow` — 图像处理 + GIF 编码

## 项目结构 Project Structure

```
mp42gif/
├── mp4_to_gif.py      # 主程序
├── requirements.txt   # 依赖
├── make_icon.py       # 图标生成
├── build_exe.bat      # 打包脚本
├── icon.ico           # 应用图标
├── LICENSE            # MIT 许可证
└── README.md          # 说明
```

## 许可 License

MIT — 自由使用、修改、分发。
